#ifndef SNDWAV_H
#define SNDWAV_H

#include <sys/cdefs.h>
__BEGIN_DECLS

#include <kos/fs.h>

#define WAVE_FORMAT_YAMAHA_ADPCM          0x0020 /* Yamaha ADPCM (ffmpeg) */
typedef struct {
    uint32_t format;
    uint32_t channels;
    uint32_t sample_rate;
    uint32_t sample_size;
    uint32_t data_offset;
    uint32_t data_length;
} WavFileInfo;

typedef int wav_stream_hnd_t;

int wav_init(void);
void wav_shutdown(void);
void wav_destroy(void);

wav_stream_hnd_t wav_create(const char *filename, float ratemul, int loop, size_t loops, size_t loope);

void wav_play(void);
void wav_pause(void);
void wav_stop(void);
void wav_volume(int vol);
int wav_is_playing(void);

__END_DECLS

#endif