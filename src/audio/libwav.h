#ifndef LIBWAV_H
#define LIBWAV_H

#include <sys/cdefs.h>
__BEGIN_DECLS

#include <kos/fs.h>
#include <inttypes.h>

#define WAVE_FORMAT_PCM                   0x0001 /* PCM */
#define WAVE_FORMAT_IEEE_FLOAT            0x0003 /* IEEE float */
#define WAVE_FORMAT_ALAW                  0x0006 /* 8-bit ITU-T G.711 A-law */
#define WAVE_FORMAT_MULAW                 0x0007 /* 8-bit ITU-T G.711 µ-law */
#define WAVE_FORMAT_YAMAHA_ADPCM          0x0020 /* Yamaha ADPCM (ffmpeg) */
#define WAVE_FORMAT_YAMAHA_ADPCM_ITU_G723 0x0014 /* ITU G.723 Yamaha ADPCM (KallistiOS) */
#define WAVE_FORMAT_EXTENSIBLE            0xfffe /* Determined by SubFormat */

typedef struct {
    uint32_t format;
    uint32_t channels;
    uint32_t sample_rate;
    uint32_t sample_size;
    uint32_t data_offset;
    uint32_t data_length;
} WavFileInfo;

int wav_get_info_file(file_t file, WavFileInfo *result);
int wav_get_info_cdda(file_t file, WavFileInfo *result);
int wav_get_info_adpcm(file_t fileL, WavFileInfo *result);
int wav_get_info_buffer(const uint8_t *buffer, WavFileInfo *result);

__END_DECLS

#endif
