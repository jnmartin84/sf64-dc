#include "libwav.h"

#include <string.h>

#include <stdio.h>

typedef struct __attribute__((__packed__)) {
    uint8_t riff[4];
    int32_t totalsize;
    uint8_t riff_format[4];
} wavmagic_t;

typedef struct __attribute__((__packed__)) {
    uint8_t id[4];
    size_t size;
} chunkhdr_t;

typedef struct __attribute__((__packed__)) {
    int16_t format;
    int16_t channels;
    int32_t sample_rate;
    int32_t byte_per_sec;
    int16_t blocksize;
    int16_t sample_size;
} fmthdr_t;

int wav_get_info_file(file_t file, WavFileInfo *result) {
    wavmagic_t wavmagic;
    chunkhdr_t chunkhdr;
    fmthdr_t   fmthdr;

    memset(result, 0, sizeof(WavFileInfo));

    fs_seek(file, 0, SEEK_SET);
    if(fs_read(file, &wavmagic, sizeof(wavmagic)) != sizeof(wavmagic)) {
        fs_close(file);
        return 0;
    }

    if(strncmp((const char*)wavmagic.riff, "RIFF", 4)) {
        fs_close(file);
        return 0;
    }

    if(strncmp((const char*)wavmagic.riff_format, "WAVE", 4)) {
        fs_close(file);
        return 0;
    }

    do {
        /* Read the chunk header */
        if(fs_read(file, &(chunkhdr), sizeof(chunkhdr)) != sizeof(chunkhdr)) {
            fs_close(file);
            return 0;
        }

        /* If it is the fmt chunk, grab the fields we care about and skip the 
           rest of the section if there is more */
        if(strncmp((const char *)chunkhdr.id, "fmt ", 4) == 0) {
            if(fs_read(file, &(fmthdr), sizeof(fmthdr)) != sizeof(fmthdr)) {
                fs_close(file);
                return 0;
            }

            /* Skip the rest of the fmt chunk */ 
            fs_seek(file, chunkhdr.size - sizeof(fmthdr), SEEK_CUR);
        }
        /* If we found the data chunk, we are done */
        else if(strncmp((const char *)chunkhdr.id, "data", 4) == 0) {
            break;
        }
        /* Skip meta data */
        else { 
            fs_seek(file, chunkhdr.size, SEEK_CUR);
        }
    } while(1);

    result->format = fmthdr.format;
    result->channels = fmthdr.channels;
    result->sample_rate = fmthdr.sample_rate;
    result->sample_size = fmthdr.sample_size;
    result->data_length = chunkhdr.size;

    result->data_offset = fs_tell(file);

    return 1;
}

int wav_get_info_adpcm(file_t fileL, WavFileInfo *result) {
    result->format = WAVE_FORMAT_YAMAHA_ADPCM;
    result->channels = 2;
    result->sample_rate = 44100;
    result->sample_size = 4;
    result->data_length = fs_total(fileL);
    result->data_offset = 0;
    printf("%d -> %d\n", fileL, result->data_length);
    return 1;
}
