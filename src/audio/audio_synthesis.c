#include "n64sys.h"
#include "sf64audio_provisional.h"
#undef VIRTUAL_TO_PHYSICAL2
#undef OS_K0_TO_PHYSICAL

#include "mixer.h"

#include "aica_synth.h"

// was 0x470
// now 36*32
#define DMEM_WET_SCRATCH 0x480

// was 0x990
// now 77*32
#define DMEM_COMPRESSED_ADPCM_DATA 0x9A0

// was 0x990
// now 77*32
#define DMEM_LEFT_CH 0x9A0

// was 0xB10
// now 89*32
#define DMEM_RIGHT_CH 0xB20

// was 0x650
// now 51*32
#define DMEM_HAAS_TEMP 0x660

// was 0x450
// now 35*32
#define DMEM_TEMP 0x460

// was 0x5F0
// now 0x600
#define DMEM_UNCOMPRESSED_NOTE 0x600

// was 0xC90
// now 101*32
#define DMEM_WET_LEFT_CH 0xCA0

// was 0xE10
// now 113*32
#define DMEM_WET_RIGHT_CH 0xE20

// = DMEM_WET_LEFT_CH + DMEM_1CH_SIZE
#define SAMPLE_SIZE sizeof(s16)

typedef enum {
    /* 0 */ HAAS_EFFECT_DELAY_NONE,
    /* 1 */ HAAS_EFFECT_DELAY_LEFT, // Delay left channel so that right channel is heard first
    /* 2 */ HAAS_EFFECT_DELAY_RIGHT // Delay right channel so that left channel is heard first
} HaasEffectDelaySide;

s32 D_80145D40; // unused

// all of these are part of the DFT-related function
f32 D_80145D48[256];
f32 D_80146148[256];
f32 D_80146548[515];
f32 D_80146D54;
f32 D_80146D58;
f32 D_80146D5C;
f32 D_80146D60;
f32 D_80146D64;
f32 D_80146D68;
f32 D_80146D6C;
f32 D_80146D70;

void AudioSynth_SyncSampleStates(s32 updateIndex);

void AudioSynth_HartleyTransform(f32* arg0, s32 arg1, f32* arg2) {
    ;
}

/**
 * Sync the sample states between the notes and the list
 */
void AudioSynth_SyncSampleStates(s32 updateIndex) {
    NoteSubEu* noteSampleState;
    NoteSubEu* sampleState;
    s32 i;

    for (i = 0; i < gNumNotes; i++) {
        noteSampleState = &gNotes[i].noteSubEu;
        sampleState = &gNoteSubsEu[gNumNotes * updateIndex + i];
        if (noteSampleState->bitField0.enabled) {
            *sampleState = *noteSampleState;
            noteSampleState->bitField0.needsInit = 0;
        } else {
            sampleState->bitField0.enabled = 0;
        }
    }
}
#include <stdio.h>
Acmd* AudioSynth_Update(Acmd* aList, s32* cmdCount, s16* aiBufStartL, s16* aiBufStartR, s32 aiBufLen) {
    s32 i;

    for (i = gAudioBufferParams.ticksPerUpdate; i > 0; i--) {
        AudioSeq_ProcessSequences(i - 1);
        AudioSynth_SyncSampleStates(gAudioBufferParams.ticksPerUpdate - i);
        /* NOTE: sub-tick AicaSynth_RefreshActive() disabled while diagnosing an
           intro hang (suspected SH4->AICA command-queue overflow under the 4x
           burst + streamer). Re-enable once throttled. */
//        AicaSynth_RefreshActive(i-1);
    }

    /* AICA hardware mixing: the sequence tick + sample-state prepass above is all the
       SH4 does; AICA renders the finalized gNoteSubsEu voices in hardware. The N64 RSP
       software render (DoOneAudioUpdate -> ProcessNote -> ...) was dropped. */
    (void) aiBufStartL; (void) aiBufStartR; (void) aiBufLen;
    AicaSynth_Update();
    *cmdCount = 0;
    return aList;
}