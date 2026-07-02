#ifndef AICA_SYNTH_H
#define AICA_SYNTH_H

/*
 * AICA hardware-mixed voice driver (Dreamcast) for Star Fox 64.
 * Replaces the SH4 software synthesis render path: maps each active gNoteSubsEu
 * voice to an AICA hardware channel via the KOS command queue. Ported from the
 * OoT driver (same EU audio engine) with the SM64 long-sample streamer grafted in.
 *
 *   AicaSynth_Update() : call from AudioSynth_Update, after the
 *                        AudioSeq_ProcessSequences loop (replaces the render).
 *   AicaSynth_Init()   : call once after audio init (uploads synthetic wavetables,
 *                        anchors the sample-bank base + resident ADPCM pool).
 */

void AicaSynth_Init(void);
void AicaSynth_Update(void);
/* Call inside the tick loop after AudioSynth_SyncSampleStates(tick) to keep
   sub-frame pan/vol/freq resolution for fast pan/tremolo/vibrato. */
void AicaSynth_RefreshActive(s32 tick);
/* Drop resident-but-unreferenced samples on a scene/memory reset (hooked in nuke_everything) so the
   ARAM sample cache doesn't carry one level's samples into the next -> CACHETBLFULL. */
void AicaSynth_ClearSampleCache(void);

/* Base of the resident AICA-ADPCM sample pool; set by the DC asset loader. */
extern const unsigned char* gAicaAdpcmPoolBase;

#endif /* AICA_SYNTH_H */
