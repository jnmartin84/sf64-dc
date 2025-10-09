#ifndef SNDWAV_H
#define SNDWAV_H

#include <sys/cdefs.h>
__BEGIN_DECLS

#include <kos/fs.h>
typedef enum {
    /* 0 */ WAV_PLAYER_BGM,
    /* 1 */ WAV_PLAYER_FANFARE,
//    /* 2 */ WAV_PLAYER_SFX,
//    /* 3 */ WAV_PLAYER_VOICE,
/*2*/    /* 4 */ WAV_PLAYER_MAX,
} WavPlayerId;

typedef int wav_stream_hnd_t;

int wav_init(void);
void wav_shutdown(void);
void wav_destroy(WavPlayerId playerId);

wav_stream_hnd_t wav_create(WavPlayerId playerId, char *filename, int loop, int loopStart, int loopEnd);

void wav_play(WavPlayerId playerId);
void wav_pause(WavPlayerId playerId);
void wav_stop(WavPlayerId playerId);
void wav_volume(WavPlayerId playerId, int vol);
int wav_is_paused(WavPlayerId playerId);
int wav_is_playing(WavPlayerId playerId);

__END_DECLS

#endif
