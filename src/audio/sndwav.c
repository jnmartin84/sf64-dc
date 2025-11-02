
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

#include <kos/mutex.h>
#include <kos/thread.h>
#include <dc/sound/stream.h>

#include "sndwav.h"
#include "libwav.h"



/* Keep track of things from the Driver side */
#define SNDDRV_STATUS_NULL         0x00
#define SNDDRV_STATUS_READY        0x01
#define SNDDRV_STATUS_DONE         0x02

/* Keep track of things from the Decoder side */
#define SNDDEC_STATUS_NULL         0x00
#define SNDDEC_STATUS_READY        0x01
#define SNDDEC_STATUS_STREAMING    0x02
#define SNDDEC_STATUS_PAUSING      0x03
#define SNDDEC_STATUS_STOPPING     0x04
#define SNDDEC_STATUS_RESUMING     0x05

typedef void *(*snddrv_cb)(snd_stream_hnd_t, int, int*);

typedef struct {
    /* The buffer on the AICA side */
    snd_stream_hnd_t shnd;

    /* We either read the wav data from a file or 
       we read from a buffer */
    file_t wave_file;

    /* Contains the buffer that we are going to send
       to the AICA in the callback.  Should be 32-byte
       aligned */
    uint8_t *drv_buf;

    /* Status of the stream that can be started, stopped
       paused, ready. etc */
    volatile int status;

    snddrv_cb callback;

    uint32_t loop;
    uint32_t vol;         /* 0-255 */

    uint32_t format;      /* Wave format */
    uint32_t channels;    /* 1-Mono/2-Stereo */
    uint32_t sample_rate; /* 44100Hz */
    uint32_t sample_size; /* 4/8/16-Bit */

    /* Offset into the file or buffer where the audio 
       data starts */
    uint32_t data_offset;

    /* The length of the audio data */
    uint32_t data_length;

    uint32_t loop_start;
    uint32_t loop_end;

    uint32_t stream_pos;  
   
} snddrv_hnd;

static snddrv_hnd streams[WAV_PLAYER_MAX];
static volatile int sndwav_status = SNDDRV_STATUS_NULL;
static kthread_t *audio_thread;
static mutex_t stream_mutex = MUTEX_INITIALIZER;

static int handles[WAV_PLAYER_MAX] = { SND_STREAM_INVALID };

static void *sndwav_thread(void *param);
static void *audio_cb(snd_stream_hnd_t hnd, int req, int* done);

int wav_init(void) {
    int i;

    if(snd_stream_init() < 0)
        return 0;

    for(i = 0; i < WAV_PLAYER_MAX; i++) {
        streams[i].shnd = SND_STREAM_INVALID;
        streams[i].vol = 0;
        streams[i].status = SNDDEC_STATUS_NULL;
        streams[i].callback = NULL;
    }

    audio_thread = thd_create(0, sndwav_thread, NULL);
    if(audio_thread != NULL) {
        sndwav_status = SNDDRV_STATUS_READY;
        return 1;
	}
    else {
        return 0;
    }
}

void wav_shutdown(void) {
    int i;

    sndwav_status = SNDDRV_STATUS_DONE;

    thd_join(audio_thread, NULL);

    for(i = 0; i < WAV_PLAYER_MAX; i++) {
        wav_destroy(i);
    }
}

void wav_destroy(WavPlayerId playerId/* wav_stream_hnd_t hnd */) {
	if(streams[playerId].shnd == SND_STREAM_INVALID)
        return;

    mutex_lock(&stream_mutex);

    snd_stream_destroy(streams[playerId].shnd);
    streams[playerId].shnd = SND_STREAM_INVALID;
    streams[playerId].status = SNDDEC_STATUS_NULL;
    streams[playerId].vol = 0;
    streams[playerId].callback = NULL;

    if(streams[playerId].wave_file != FILEHND_INVALID)
        fs_close(streams[playerId].wave_file);

    if(streams[playerId].drv_buf) {
        free(streams[playerId].drv_buf);
        streams[playerId].drv_buf = NULL;
    }
	handles[playerId] = SND_STREAM_INVALID;

	mutex_unlock(&stream_mutex);
}

wav_stream_hnd_t wav_create(WavPlayerId playerId, char *filename, int loop, int loopStart, int loopEnd) {
    file_t file;
    WavFileInfo info;
    wav_stream_hnd_t index;

	index = handles[playerId];
	if (index != SND_STREAM_INVALID) {
		wav_destroy(playerId);
	}
	index = SND_STREAM_INVALID;

	if(filename == NULL)
        return SND_STREAM_INVALID;

    file = fs_open(filename, O_RDONLY);
    if(file == FILEHND_INVALID) {
		while(1) 
			printf("couldn't open file %s\n", filename);
        exit(-1);
        return SND_STREAM_INVALID;
    }

    index = snd_stream_alloc(audio_cb, 32768);

    if(index == SND_STREAM_INVALID) {
		while(1) 
	        printf("couldn't alloc stream %d\n", 32768);
        exit(-1);
        fs_close(file);
        snd_stream_destroy(index);
        return SND_STREAM_INVALID;
    }

    wav_get_info_adpcm(file, &info);
    //wav_get_info_file(file, &info);
    streams[playerId].drv_buf = memalign(32, 32768);

    if(streams[playerId].drv_buf == NULL) {
		while(1) 
	        printf("couldn't memalign drv_buf\n");
        exit(-1);
        fs_close(file);
        snd_stream_destroy(index);
        return SND_STREAM_INVALID;
    }

	handles[playerId] = index;
    streams[playerId].shnd = index;
    streams[playerId].wave_file = file;
    streams[playerId].loop = loop;
    if (loop) {
		if (loopStart || loopEnd) {
        streams[playerId].loop_start = info.data_offset + loopStart;
        streams[playerId].loop_end = info.data_offset + loopEnd;
		} else {
			streams[playerId].loop_start = info.data_offset;
			streams[playerId].loop_end = info.data_offset + info.data_length;
		}
        streams[playerId].loop_start &= ~31;
        streams[playerId].loop_end &= ~31;
	}
    streams[playerId].callback = audio_cb;

    streams[playerId].format = info.format;
    streams[playerId].channels = info.channels;
    streams[playerId].sample_rate = info.sample_rate;
    streams[playerId].sample_size = info.sample_size;
    streams[playerId].data_length = info.data_length;
    streams[playerId].data_offset = info.data_offset;
    streams[playerId].stream_pos = info.data_offset;
    fs_seek(streams[playerId].wave_file, info.data_offset, SEEK_SET);
    streams[playerId].status = SNDDEC_STATUS_READY;

	return index;
}

void wav_play(WavPlayerId playerId) {
	if(streams[playerId].shnd == SND_STREAM_INVALID)
        return;

//        wav_stream_hnd_t hnd = handles[playerId];
//	if (hnd == SND_STREAM_INVALID)
//		return;

	if(streams[playerId].status == SNDDEC_STATUS_STREAMING)
       return;

    streams[playerId].status = SNDDEC_STATUS_RESUMING;
}

void wav_pause(WavPlayerId playerId) {
	if(streams[playerId].shnd == SND_STREAM_INVALID)
        return;

//    wav_stream_hnd_t hnd = handles[playerId];
//	if (hnd == SND_STREAM_INVALID)
//		return;

	if(streams[playerId].status == SNDDEC_STATUS_READY ||
       streams[playerId].status == SNDDEC_STATUS_PAUSING)
       return;
       
    streams[playerId].status = SNDDEC_STATUS_PAUSING;
}

void wav_stop(WavPlayerId playerId) {
	if(streams[playerId].shnd == SND_STREAM_INVALID)
        return;
//	wav_stream_hnd_t hnd = handles[playerId];
//	if (hnd == SND_STREAM_INVALID)
//		return;

	if(streams[playerId].status == SNDDEC_STATUS_READY ||
       streams[playerId].status == SNDDEC_STATUS_STOPPING)
       return;
       
    streams[playerId].status = SNDDEC_STATUS_STOPPING;
}

void wav_volume(WavPlayerId playerId, int vol) {
	if(streams[playerId].shnd == SND_STREAM_INVALID)
        return;

    //	wav_stream_hnd_t hnd = handles[playerId];
//	if (hnd == SND_STREAM_INVALID)
//		return;

    if(vol > 255)
        vol = 255;

    if(vol < 0)
        vol = 0;

    streams[playerId].vol = vol;
    snd_stream_volume(streams[playerId].shnd, streams[playerId].vol);
}

int wav_is_paused(WavPlayerId playerId) {
//	wav_stream_hnd_t hnd = handles[playerId];
//	if (hnd == SND_STREAM_INVALID)
//		return 0;
	if(streams[playerId].shnd == SND_STREAM_INVALID)
        return 0;

	return (streams[playerId].status == SNDDEC_STATUS_PAUSING);
}

int wav_is_playing(WavPlayerId playerId) {
	if(streams[playerId].shnd == SND_STREAM_INVALID)
        return 0;

//    wav_stream_hnd_t hnd = handles[playerId];
//	if (hnd == SND_STREAM_INVALID)
//		return 0;

	return (streams[playerId].status == SNDDEC_STATUS_STREAMING);
}

static void *sndwav_thread(void *param) {
    (void)param;
    int i;

    while(sndwav_status != SNDDRV_STATUS_DONE) {

        mutex_lock(&stream_mutex);

        for(i = 0; i < WAV_PLAYER_MAX; i++) {
            switch(streams[i].status) {
                case SNDDEC_STATUS_RESUMING:
                    snd_stream_start_adpcm(streams[i].shnd, streams[i].sample_rate, streams[i].channels - 1);
                    snd_stream_volume(streams[i].shnd, streams[i].vol);
                    streams[i].status = SNDDEC_STATUS_STREAMING;
                    break;
                case SNDDEC_STATUS_PAUSING:
                    snd_stream_stop(streams[i].shnd);
                    streams[i].status = SNDDEC_STATUS_READY;
                    break;
                case SNDDEC_STATUS_STOPPING:
                    snd_stream_stop(streams[i].shnd);
                    if(streams[i].wave_file != FILEHND_INVALID) {
                        fs_seek(streams[i].wave_file, streams[i].data_offset, SEEK_SET);
                    }
                    streams[i].status = SNDDEC_STATUS_READY;
                    break;
                case SNDDEC_STATUS_STREAMING:
                    snd_stream_poll(streams[i].shnd);
                    break;
                case SNDDEC_STATUS_READY:
                default:
                    break;
            }
        }

        mutex_unlock(&stream_mutex);

        thd_sleep(50);
    }

    return NULL;
}

static void *audio_cb(snd_stream_hnd_t shnd, int req, int* done) {
	WavPlayerId playerId = SND_STREAM_INVALID;
	for (int i=0;i<WAV_PLAYER_MAX;i++) {
		if (streams[i].shnd == shnd) {
			playerId = i;
			break;
		}
	}
	if (playerId == SND_STREAM_INVALID) {
		return NULL;
	}

    if (streams[playerId].loop) {
        //printf("stream %d pos %d loop start %d loop end %d\n", playerId, streams[playerId].stream_pos, streams[playerId].loop_start, streams[playerId].loop_end);
        uint32_t total_len = streams[playerId].loop_end;
        uint32_t pos = streams[playerId].stream_pos;
        if ((pos + req) >= total_len) {
            uint32_t over = (((pos + req) - total_len)+3) & ~3 ;
            uint32_t actual = (req - over) & ~3;
            fs_read(streams[playerId].wave_file, streams[playerId].drv_buf, actual);
            //printf("read %d into %08x from %d\n", actual, streams[hnd].drv_buf, streams[hnd].wave_file);
            fs_seek(streams[playerId].wave_file, streams[playerId].loop_start, SEEK_SET);
            fs_read(streams[playerId].wave_file, streams[playerId].drv_buf + actual, over);
            //printf("read %d into %08x from %d\n", over, streams[hnd].drv_buf+ actual, streams[hnd].wave_file);
            streams[playerId].stream_pos = streams[playerId].loop_start + over;
        } else {
            //printf("read %d into %08x from %d\n", req, streams[hnd].drv_buf, streams[hnd].wave_file);
            fs_read(streams[playerId].wave_file, streams[playerId].drv_buf, req);
            streams[playerId].stream_pos += req;
        }

        *done = req;

        return streams[playerId].drv_buf;
    } else {
        //printf("stream %d pos %d\n", hnd, streams[hnd].stream_pos);
        int read = fs_read(streams[playerId].wave_file, streams[playerId].drv_buf, req);

        if(read != req) {
            fs_seek(streams[playerId].wave_file, streams[playerId].data_offset, SEEK_SET);
            snd_stream_stop(streams[playerId].shnd);
            streams[playerId].stream_pos = streams[playerId].data_offset;
            streams[playerId].status = SNDDEC_STATUS_READY;
			if (playerId == WAV_PLAYER_FANFARE) {
				wav_play(WAV_PLAYER_BGM);
			}
            *done = read;
            return streams[playerId].drv_buf;
        }

        *done = req;
        streams[playerId].stream_pos += req;
        return streams[playerId].drv_buf;
    }
}