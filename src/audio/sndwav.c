#include <kos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

#include <kos/thread.h>
#include <dc/sound/stream.h>

#include "sndwav.h"
#define RANGECHECK 1
/* Keep track of things from the Driver side */
#define SNDDRV_STATUS_NULL 0x00
#define SNDDRV_STATUS_READY 0x01
#define SNDDRV_STATUS_DONE 0x02

/* Keep track of things from the Decoder side */
#define SNDDEC_STATUS_NULL 0x00
#define SNDDEC_STATUS_READY 0x01
#define SNDDEC_STATUS_STREAMING 0x02
#define SNDDEC_STATUS_PAUSING 0x03
#define SNDDEC_STATUS_STOPPING 0x04
#define SNDDEC_STATUS_RESUMING 0x05

typedef void *(*snddrv_cb)(snd_stream_hnd_t, int, int *);

typedef struct
{
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
	uint32_t vol; /* 0-255 */

	uint32_t format;	  /* Wave format */
	uint32_t channels;	  /* 1-Mono/2-Stereo */
	uint32_t sample_rate; /* 44100Hz */
	uint32_t sample_size; /* 4/8/16-Bit */

	/* Offset into the file or buffer where the audio
	   data starts */
	uint32_t data_offset;

	/* The length of the audio data */
	uint32_t data_length;

	/* Used only in reading wav data from a buffer
	   and not a file */
	uint32_t buf_offset;

    size_t loopstart; // sample number to restart at when looping
    size_t loopend; // sample number to end at when looping
    size_t cursample;
} snddrv_hnd;

static snddrv_hnd stream;
static volatile int sndwav_status = SNDDRV_STATUS_NULL;
static kthread_t *audio_thread;
static kthread_attr_t audio_attr;
static mutex_t stream_mutex;

static void *sndwav_thread(void *param);
static void *wav_file_callback(snd_stream_hnd_t hnd, int req, int *done);

void I_Error(char *str) {
    printf("%s\n",str);
    exit(-1);
}

int wav_init(void)
{
	if (snd_stream_init() < 0)
		return 0;

#if RANGECHECK
	mutex_init(&stream_mutex, MUTEX_TYPE_ERRORCHECK);
#else
	mutex_init(&stream_mutex, MUTEX_TYPE_NORMAL);
#endif

	stream.shnd = SND_STREAM_INVALID;
	stream.vol = 0;
	stream.status = SNDDEC_STATUS_NULL;
	stream.callback = NULL;
	audio_attr.create_detached = 0;
	audio_attr.stack_size = 32768;
	audio_attr.stack_ptr = NULL;
	audio_attr.prio = PRIO_DEFAULT;
	audio_attr.label = "MusicPlayer";

	audio_thread = thd_create_ex(&audio_attr, sndwav_thread, NULL);
	if (audio_thread != NULL)
		sndwav_status = SNDDRV_STATUS_READY;

	return sndwav_status;
}

void wav_shutdown(void)
{
	sndwav_status = SNDDRV_STATUS_DONE;

	thd_join(audio_thread, NULL);

	wav_destroy();
}

void wav_destroy(void)
{
	if (stream.shnd == SND_STREAM_INVALID)
		return;

#if RANGECHECK
	if (mutex_lock(&stream_mutex))
		I_Error("Failed to lock stream_mutex");
#else
	mutex_lock(&stream_mutex);
#endif

	snd_stream_destroy(stream.shnd);
	stream.shnd = SND_STREAM_INVALID;
	stream.status = SNDDEC_STATUS_NULL;
	stream.vol = 0;
	stream.callback = NULL;

	if (stream.wave_file != FILEHND_INVALID)
		fs_close(stream.wave_file);

	if (stream.drv_buf) {
		free(stream.drv_buf);
		stream.drv_buf = NULL;
	}

#if RANGECHECK
	if (mutex_unlock(&stream_mutex))
		I_Error("Failed to unlock stream_mutex");
#else
	mutex_unlock(&stream_mutex);
#endif
}

static int wav_get_info_adpcm(file_t file, WavFileInfo *result) {
    result->format = WAVE_FORMAT_YAMAHA_ADPCM;
    result->channels = 2;
    result->sample_rate = 44100;
    result->sample_size = 4;
    result->data_length = fs_total(file);

    result->data_offset = 0;

    return 1;
}

wav_stream_hnd_t wav_create(const char *filename, int loop, size_t loopstart, size_t loopend)
{
	file_t file;
	WavFileInfo info;
	wav_stream_hnd_t index;

	if (filename == NULL)
		return SND_STREAM_INVALID;

	file = fs_open(filename, O_RDONLY);

	if (file == FILEHND_INVALID)
		return SND_STREAM_INVALID;

	index = snd_stream_alloc(wav_file_callback, 4096);//SND_STREAM_BUFFER_MAX);
	if (index == SND_STREAM_INVALID) {
		fs_close(file);
		snd_stream_destroy(index);
		return SND_STREAM_INVALID;
	}

	wav_get_info_adpcm(file, &info);

	stream.drv_buf = memalign(32, 4096);//SND_STREAM_BUFFER_MAX);

	if (stream.drv_buf == NULL) {
		fs_close(file);
		snd_stream_destroy(index);
		return SND_STREAM_INVALID;
	}

	stream.shnd = index;
	stream.wave_file = file;
	stream.loop = loop;
	stream.callback = wav_file_callback;
	stream.vol = (90 * 255) / 100;
	stream.format = info.format;
	stream.channels = info.channels;
	stream.sample_rate = info.sample_rate;
	stream.sample_size = info.sample_size;
	stream.data_length = info.data_length;
	stream.data_offset = info.data_offset;
    stream.loopstart = loopstart;
    stream.loopend = loopend;
    stream.cursample = 0;

	fs_seek(stream.wave_file, stream.data_offset, SEEK_SET);

	snd_stream_volume(stream.shnd, stream.vol);

	stream.status = SNDDEC_STATUS_READY;

	return index;
}

void wav_play(void)
{
	if (stream.status == SNDDEC_STATUS_STREAMING)
		return;

	stream.status = SNDDEC_STATUS_RESUMING;
}

void wav_play_volume(void)
{
	if (stream.status == SNDDEC_STATUS_STREAMING)
		return;

	stream.status = SNDDEC_STATUS_RESUMING;
}

void wav_pause(void)
{
	if (stream.status == SNDDEC_STATUS_READY || stream.status == SNDDEC_STATUS_PAUSING)
		return;

	stream.status = SNDDEC_STATUS_PAUSING;
}

void wav_stop(void)
{
	if (stream.status == SNDDEC_STATUS_READY || stream.status == SNDDEC_STATUS_STOPPING)
		return;

	stream.status = SNDDEC_STATUS_STOPPING;
}

void wav_volume(int vol)
{
	if (stream.shnd == SND_STREAM_INVALID)
		return;

	if (vol > 255)
		vol = 255;

	if (vol < 0)
		vol = 0;

	stream.vol = vol;
	snd_stream_volume(stream.shnd, stream.vol);
}

int wav_is_playing(void)
{
	return stream.status == SNDDEC_STATUS_STREAMING;
}

static void *sndwav_thread(void *param)
{
	(void)param;

	while (sndwav_status != SNDDRV_STATUS_DONE) {
#if RANGECHECK
		if (mutex_lock(&stream_mutex))
			I_Error("Failed to lock stream_mutex");
#else
		mutex_lock(&stream_mutex);
#endif
		switch (stream.status) {
		case SNDDEC_STATUS_RESUMING:
			snd_stream_volume(stream.shnd, stream.vol);
			snd_stream_start_adpcm(stream.shnd, stream.sample_rate, stream.channels - 1);
			snd_stream_volume(stream.shnd, stream.vol);			
			stream.status = SNDDEC_STATUS_STREAMING;
			break;
		case SNDDEC_STATUS_PAUSING:
			snd_stream_stop(stream.shnd);
			stream.status = SNDDEC_STATUS_READY;
			break;
		case SNDDEC_STATUS_STOPPING:
			snd_stream_stop(stream.shnd);
			if (stream.wave_file != FILEHND_INVALID)
				fs_seek(stream.wave_file, stream.data_offset, SEEK_SET);
			else
				stream.buf_offset = stream.data_offset;

			stream.status = SNDDEC_STATUS_READY;
			break;
		case SNDDEC_STATUS_STREAMING:
			snd_stream_poll(stream.shnd);
			break;
		case SNDDEC_STATUS_READY:
		default:
			break;
		}

#if RANGECHECK
		if (mutex_unlock(&stream_mutex))
			I_Error("Failed to unlock stream_mutex");
#else
		mutex_unlock(&stream_mutex);
#endif
		thd_sleep(50);
	}

	return NULL;
}

static void *wav_file_callback(snd_stream_hnd_t hnd, int req, int *done)
{
	(void)hnd;


	ssize_t readed = fs_read(stream.wave_file, stream.drv_buf, req);

	if (readed == -1) {
#if RANGECHECK
		dbgio_printf("Failed to read from stream wave_file\nDisabling stream\n");
#endif
		snd_stream_stop(stream.shnd);
		stream.status = SNDDEC_STATUS_READY;
		return NULL;
	}
    printf("actual read %d\n", readed);
    stream.cursample += readed;

	if (readed != req) {
        fs_seek(stream.wave_file, stream.loopstart, SEEK_SET);
        stream.cursample = stream.loopstart;

		if (stream.loop) {
			ssize_t readed2 = fs_read(stream.wave_file, stream.drv_buf, req);
            stream.cursample += readed2;

			if (readed2 == -1) {
#if RANGECHECK
				dbgio_printf("read != req: Failed to read from stream wave_file\nDisabling stream\n");
#endif
				snd_stream_stop(stream.shnd);
				stream.status = SNDDEC_STATUS_READY;
				return NULL;
			}
		} else {
			snd_stream_stop(stream.shnd);
			stream.status = SNDDEC_STATUS_READY;
			return NULL;
		}
	}

    printf("stream.cursample %d\n", stream.cursample);

	*done = req;

	return stream.drv_buf;
}


