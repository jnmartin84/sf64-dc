/*
 * File: driver.c
 * Project: sf64-dc
 * Author: Hayden Kowalchuk (hayden@hkowsoftware.com)
 * Modifications by jnmartin84
 *
 * AICA hardware mixing: the KOS sound stream is no longer used. The AICA voice
 * driver (src/audio/aica_synth.c) plays voices directly on the AICA hardware
 * channels via the SH4->AICA command queue. This backend only brings up the base
 * sound system (snd_init: AICA ARM program + snd_mem/ARAM allocator + the
 * SH4->AICA command queue) that the voice driver relies on; it produces no
 * output of its own. The vblank-gated AudioThread still ticks the sequence/synth
 * each frame (driving AICA voice updates); its play() push is now a no-op.
 */

#include <kos.h>
#include <dc/sound/sound.h>
#include <stdio.h>
#include <stdbool.h>

#include "audio_dc.h"
#include "macros.h"

static bool audio_dc_init(void) {
    if (snd_init() != 0) {
        printf("AICA INIT FAILURE!\n");
        return false;
    }
    /* keep the scheduler lively for the vblank-driven synthesis thread */
    thd_set_hz(300);
    printf("Sound init complete (AICA hardware mixing, no stream)!\n");
    return true;
}

static int audio_dc_buffered(void) {
    return 0;
}

static int audio_dc_get_desired_buffered(void) {
    return 0;
}

/* AICA mixes in hardware; there is no software-mixed output to push. */
static void audio_dc_play(UNUSED uint8_t* bufL, UNUSED uint8_t* bufR, UNUSED size_t len) {
}

struct AudioAPI audio_dc = {
    audio_dc_init,
    audio_dc_buffered,
    audio_dc_get_desired_buffered,
    audio_dc_play
};
