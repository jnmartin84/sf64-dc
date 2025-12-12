#include "n64sys.h"
#include "sf64audio_provisional.h"
#undef VIRTUAL_TO_PHYSICAL2
#undef OS_K0_TO_PHYSICAL

#include "mixer.h"

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

void AudioSynth_DisableSampleStates(s32 updateIndex, s32 noteIndex);
void AudioSynth_SyncSampleStates(s32 updateIndex);
Acmd* AudioSynth_ProcessNote(s32 noteIndex, NoteSubEu* noteSub, NoteSynthesisState* synthState, s16* aiBufL, s16* aiBufR,
                             s32 aiBufLen, Acmd* aList, s32 updateIndex);
Acmd* AudioSynth_DoOneAudioUpdate(s16* aiBufL, s16* aiBufR, s32 aiBufLen, Acmd* aList, s32 updateIndex);
Acmd* AudioSynth_LoadRingBufferPart(Acmd* aList, u16 dmem, u16 startPos, s32 size, s32 reverbIndex);
Acmd* AudioSynth_SaveRingBufferPart(Acmd* aList, u16 dmem, u16 startPos, s32 size, s32 reverbIndex);
Acmd* AudioSynth_LoadReverbSamples(Acmd* aList, s32 aiBufLen, s16 reverbIndex, s16 updateIndex);
Acmd* AudioSynth_SaveReverbSamples(Acmd* aList, s16 reverbIndex, s16 updateIndex);
Acmd* AudioSynth_LoadWaveSamples(Acmd* aList, NoteSubEu* noteSub, NoteSynthesisState* synthState, s32 numSamplesToLoad);
Acmd* AudioSynth_FinalResample(Acmd* aList, NoteSynthesisState* synthState, s32 size, u16 pitch, u16 inpDmem,
                               u32 resampleFlags);
Acmd* AudioSynth_ProcessEnvelope(Acmd* aList, NoteSubEu* noteSub, NoteSynthesisState* synthState, s32 aiBufLen,
                                 u16 dmemSrc, s32 flags);
Acmd* AudioSynth_ApplyHaasEffect(Acmd* aList, NoteSubEu* noteSub, NoteSynthesisState* synthState, s32 size, s32 flags,
                                 s32 delaySide);

void AudioSynth_HartleyTransform(f32* arg0, s32 arg1, f32* arg2) {
    ;
}

void AudioSynth_DisableSampleStates(s32 updateIndex, s32 noteIndex) {
    s32 i;

    for (i = updateIndex + 1; i < gAudioBufferParams.ticksPerUpdate; i++) {
        if (!gNoteSubsEu[(gNumNotes * i) + noteIndex].bitField0.needsInit) {
            gNoteSubsEu[(gNumNotes * i) + noteIndex].bitField0.enabled = 0;
        } else {
            break;
        }
    }
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
    Acmd* aCmdPtr;
    s32* aiBufPtr[2];
    s32 chunkLen;
    s32 i;

    aCmdPtr = aList;
    for (i = gAudioBufferParams.ticksPerUpdate; i > 0; i--) {
        AudioSeq_ProcessSequences(i - 1);
        AudioSynth_SyncSampleStates(gAudioBufferParams.ticksPerUpdate - i);
    }

    aiBufPtr[0] = (s32*) aiBufStartL;
    aiBufPtr[1] = (s32*) aiBufStartR;
    for (i = gAudioBufferParams.ticksPerUpdate; i > 0; i--) {
        if (i == 1) {
            chunkLen = aiBufLen;
        } else if ((aiBufLen / i) >= gAudioBufferParams.samplesPerTickMax) {
            chunkLen = gAudioBufferParams.samplesPerTickMax;
        } else if (gAudioBufferParams.samplesPerTickMin >= (aiBufLen / i)) {
            chunkLen = gAudioBufferParams.samplesPerTickMin;
        } else {
            chunkLen = gAudioBufferParams.samplesPerTick;
        }

        aCmdPtr =
            AudioSynth_DoOneAudioUpdate((s16*) aiBufPtr[0], (s16*) aiBufPtr[1], chunkLen, aCmdPtr, gAudioBufferParams.ticksPerUpdate - i);

        aiBufLen -= chunkLen;
        if (aiBufLen < 0) {
            chunkLen = chunkLen + aiBufLen;
            aiBufLen = chunkLen;
        }

        aiBufPtr[0] += chunkLen>>1;
        aiBufPtr[1] += chunkLen>>1;
    }

    *cmdCount = aCmdPtr - aList;

    return aCmdPtr;
}

Acmd* AudioSynth_DoOneAudioUpdate(s16* aiBufL, s16* aiBufR, s32 aiBufLen, Acmd* aList, s32 updateIndex) {
    u8 sp84[0x3C];
    s16 count;
    s32 j;

    count = 0;

    for (j = 0; j < gNumNotes; j++) {
        if (gNoteSubsEu[gNumNotes * updateIndex + j].bitField0.enabled) {
            sp84[count++] = j;
        }
    }

    aClearBuffer(aList++, DMEM_LEFT_CH, DMEM_2CH_SIZE);

    j = 0;
    while (j < count) {
        aList = AudioSynth_ProcessNote(sp84[j], &gNoteSubsEu[updateIndex * gNumNotes + sp84[j]],
                                       &gNotes[sp84[j]].synthesisState, aiBufL, aiBufR, aiBufLen, aList, updateIndex);
        j++;
    }

    j = aiBufLen * 2;
    aSetBuffer(aList++, 0, 0, DMEM_TEMP, j);
    aSaveBuffer(aList++, DMEM_LEFT_CH, OS_K0_TO_PHYSICAL(aiBufL), j);
    aSaveBuffer(aList++, DMEM_RIGHT_CH, OS_K0_TO_PHYSICAL(aiBufR), j);
    return aList;
}

Acmd* AudioSynth_ProcessNote(s32 noteIndex, NoteSubEu* noteSub, NoteSynthesisState* synthState, s16* aiBufL,
                             s16* aiBufR, s32 aiBufLen, Acmd* aList, s32 updateIndex) {
    Sample* bookSample = NULL;
    AdpcmLoop* loopInfo = NULL;
    void* currentBook = NULL;
    u8 sampleFinished = 0;
    u8 loopToPoint = 0;
    s32 flags = 0;
    u16 resampleRateFixedPoint = 0;
    s32 numSamplesToLoad = 0;
    s32 skipBytes = 0;
    u32 sampleAddr = 0;
    s32 numSamplesToLoadAdj = 0;
    s32 numSamplesProcessed = 0;
    u32 endPos = 0;
    s32 nSamplesToProcess = 0;
    s32 numTrailingSamplesToIgnore = 0;
    s32 frameSize = 0;
    s32 skipInitialSamples = 0;
    s32 sampleDmaStart = 0;
    s32 numParts = 0;
    s32 curPart = 0;
    s32 numSamplesInThisIteration = 0;
    s32 sampleDataChunkAlignPad = 0;
    s32 resampledTempLen = 0;
    u16 noteSamplesDmemAddrBeforeResampling = 0;
    s32 frameIndex = 0;
    s32 sampleDataOffset = 0;
    Note* note = NULL;
    u16 sp56 = 0;
    s32 numSamplesInFirstFrame = 0;
    s32 nFramesToDecode = 0;
    s32 nFirstFrameSamplesToIgnore = 0;
    s32 dmemUncompressedAddrOffset1 = 0;
    u32 sampleslenFixedPoint = 0;
    u8* samplesToLoadAddr = NULL;
    s32 gain = 0;
    u32 nEntries = 0;
    s32 aligned = 0;
    s16 addr = 0;
    s32 samplesRemaining = 0;
    s32 numSamplesToDecode = 0;

    currentBook = NULL;
    note = &gNotes[noteIndex];
    flags = A_CONTINUE;

    if (noteSub->bitField0.needsInit == 1) {
        flags = A_INIT;
        synthState->restart = 0;
        synthState->samplePosInt = 0;
        synthState->samplePosFrac = 0;
        synthState->curVolLeft = 0;
        synthState->curVolRight = 0;
        synthState->numParts = synthState->prevHaasEffectRightDelaySize = synthState->prevHaasEffectLeftDelaySize = 0;
        note->noteSubEu.bitField0.finished = 0;
    }

    resampleRateFixedPoint = noteSub->resampleRate;
    numParts = noteSub->bitField1.hasTwoParts + 1;
    sampleslenFixedPoint = ((resampleRateFixedPoint * aiBufLen) * 2) + synthState->samplePosFrac;
    numSamplesToLoad = sampleslenFixedPoint >> 16;
    synthState->samplePosFrac = sampleslenFixedPoint & 0xFFFF;

    if ((synthState->numParts == 1) && (numParts == 2)) {
        numSamplesToLoad += 2;
        sp56 = 2;
    } else if ((synthState->numParts == 2) && (numParts == 1)) {
        numSamplesToLoad -= 4;
        sp56 = 4;
    } else {
        sp56 = 0;
    }

    synthState->numParts = numParts;

    if (noteSub->bitField1.isSyntheticWave) {
        aList = AudioSynth_LoadWaveSamples(aList, noteSub, synthState, numSamplesToLoad);
        noteSamplesDmemAddrBeforeResampling = DMEM_UNCOMPRESSED_NOTE + (synthState->samplePosInt * SAMPLE_SIZE);
        synthState->samplePosInt += numSamplesToLoad;
    } else {
        bookSample = *((Sample**) noteSub->waveSampleAddr);
        loopInfo = bookSample->loop;

        endPos = __builtin_bswap32(loopInfo->end);
        sampleAddr = bookSample->sampleAddr;
        resampledTempLen = 0;

        for (curPart = 0; curPart < numParts; curPart++) {
            numSamplesProcessed = 0;
            dmemUncompressedAddrOffset1 = 0;

            if (numParts == 1) {
                numSamplesToLoadAdj = numSamplesToLoad;
            } else if (numSamplesToLoad & 1) {
                numSamplesToLoadAdj = (numSamplesToLoad & ~1) + (curPart * 2);
            } else {
                numSamplesToLoadAdj = numSamplesToLoad;
            }

            if ((bookSample->codec == CODEC_ADPCM) && (currentBook != bookSample->book->book)) {
                switch (noteSub->bitField1.bookOffset) {
                    case 1:
                        currentBook = &gD_800DD200[1];
                        break;

                    case 2:
                        currentBook = &gD_800DD200[2];
                        break;

                    default:
                    case 3:
                        currentBook = bookSample->book->book;
                        break;
                }

                nEntries = (SAMPLES_PER_FRAME * __builtin_bswap32(bookSample->book->order)) *
                           __builtin_bswap32(bookSample->book->numPredictors);
                aLoadADPCM(aList++, nEntries, OS_K0_TO_PHYSICAL(currentBook));
            }

            while (numSamplesProcessed != numSamplesToLoadAdj) {
                sampleFinished = 0;
                loopToPoint = 0;

                samplesRemaining = endPos - synthState->samplePosInt;
                nSamplesToProcess = numSamplesToLoadAdj - numSamplesProcessed;

                nFirstFrameSamplesToIgnore = synthState->samplePosInt & 0xF;

                if ((nFirstFrameSamplesToIgnore == 0) && (!synthState->restart)) {
                    nFirstFrameSamplesToIgnore = SAMPLES_PER_FRAME;
                }

                numSamplesInFirstFrame = SAMPLES_PER_FRAME - nFirstFrameSamplesToIgnore;

                if (nSamplesToProcess < samplesRemaining) {
                    nFramesToDecode = ((nSamplesToProcess - numSamplesInFirstFrame) + SAMPLES_PER_FRAME - 1) >>
                                      LOG2_SAMPLES_PER_FRAME;
                    numSamplesToDecode = nFramesToDecode * SAMPLES_PER_FRAME;
                    numTrailingSamplesToIgnore = (numSamplesInFirstFrame + numSamplesToDecode) - nSamplesToProcess;
                } else {
                    numSamplesToDecode = samplesRemaining - numSamplesInFirstFrame;
                    numTrailingSamplesToIgnore = 0;

                    if (numSamplesToDecode <= 0) {
                        numSamplesToDecode = 0;
                        numSamplesInFirstFrame = samplesRemaining;
                    }

                    nFramesToDecode = (numSamplesToDecode + SAMPLES_PER_FRAME - 1) >> LOG2_SAMPLES_PER_FRAME;
                    if (loopInfo->count != 0) {
                        // Loop around and restart
                        loopToPoint = 1;
                    } else {
                        sampleFinished = 1;
                    }
                }

                frameSize = 9;
                skipInitialSamples = SAMPLES_PER_FRAME;
                sampleDmaStart = 0;

                aligned = ALIGN16((nFramesToDecode * frameSize) + SAMPLES_PER_FRAME);
                addr = DMEM_COMPRESSED_ADPCM_DATA - aligned;

                if (nFramesToDecode != 0) {
                    frameIndex = (synthState->samplePosInt + skipInitialSamples - nFirstFrameSamplesToIgnore) >>
                                 LOG2_SAMPLES_PER_FRAME;
                    sampleDataOffset = frameIndex * frameSize;
                    if (bookSample->medium == 0) {
                        samplesToLoadAddr = (u8*) (sampleDmaStart + sampleDataOffset + sampleAddr);
                    } else {
                        samplesToLoadAddr =
                            AudioLoad_DmaSampleData(sampleDmaStart + sampleDataOffset + sampleAddr, aligned, flags,
                                                    &synthState->sampleDmaIndex, bookSample->medium);
                    }
                    sampleDataChunkAlignPad = (u32) samplesToLoadAddr % SAMPLES_PER_FRAME;

                    aLoadBuffer(aList++, OS_K0_TO_PHYSICAL(samplesToLoadAddr - sampleDataChunkAlignPad), addr, aligned);
                } else {
                    numSamplesToDecode = 0;
                    sampleDataChunkAlignPad = 0;
                }

                if (synthState->restart) {
                    aSetLoop(aList++, OS_K0_TO_PHYSICAL(bookSample->loop->predictorState));
                    flags = A_LOOP;
                    synthState->restart = 0;
                }

                numSamplesInThisIteration = (numSamplesToDecode + numSamplesInFirstFrame) - numTrailingSamplesToIgnore;

                if (numSamplesProcessed == 0) {
                    aSetBuffer(aList++, 0, /* addr + */ sampleDataChunkAlignPad, DMEM_UNCOMPRESSED_NOTE,
                               numSamplesToDecode * SAMPLE_SIZE);
                    aADPCMdec(aList++, flags, OS_K0_TO_PHYSICAL(synthState->synthesisBuffers));

                    skipBytes = nFirstFrameSamplesToIgnore * SAMPLE_SIZE;
                } else {
                    aligned = ALIGN16(dmemUncompressedAddrOffset1 + SAMPLES_PER_FRAME);
                    aSetBuffer(aList++, 0, /* addr + */ sampleDataChunkAlignPad, DMEM_UNCOMPRESSED_NOTE + aligned,
                               numSamplesToDecode * SAMPLE_SIZE);
                    aADPCMdec(aList++, flags, OS_K0_TO_PHYSICAL(synthState->synthesisBuffers));

                    aDMEMMove(aList++, DMEM_UNCOMPRESSED_NOTE + aligned + (nFirstFrameSamplesToIgnore * SAMPLE_SIZE),
                              DMEM_UNCOMPRESSED_NOTE + dmemUncompressedAddrOffset1,
                              numSamplesInThisIteration * SAMPLE_SIZE);
                }

                numSamplesProcessed += numSamplesInThisIteration;

                switch (flags) {
                    case A_INIT:
                        skipBytes = SAMPLES_PER_FRAME * SAMPLE_SIZE;
                        dmemUncompressedAddrOffset1 = (numSamplesToDecode + SAMPLES_PER_FRAME) * SAMPLE_SIZE;
                        break;

                    case A_LOOP:
                        dmemUncompressedAddrOffset1 =
                            (numSamplesInThisIteration * SAMPLE_SIZE) + dmemUncompressedAddrOffset1;
                        break;

                    default:
                        if (dmemUncompressedAddrOffset1 != 0) {
                            dmemUncompressedAddrOffset1 =
                                (numSamplesInThisIteration * SAMPLE_SIZE) + dmemUncompressedAddrOffset1;
                        } else {
                            dmemUncompressedAddrOffset1 =
                                (nFirstFrameSamplesToIgnore + numSamplesInThisIteration) * SAMPLE_SIZE;
                        }
                        break;
                }

                flags = A_CONTINUE;

                if (sampleFinished) {
                    aClearBuffer(aList++, DMEM_UNCOMPRESSED_NOTE + dmemUncompressedAddrOffset1,
                                 (numSamplesToLoadAdj - numSamplesProcessed) * SAMPLE_SIZE);
                    noteSub->bitField0.finished = 1;
                    note->noteSubEu.bitField0.finished = 1;
                    AudioSynth_DisableSampleStates(updateIndex, noteIndex);
                    break;
                }

                if (loopToPoint) {
                    synthState->restart = 1;
                    synthState->samplePosInt = __builtin_bswap32(loopInfo->start);
                } else {
                    synthState->samplePosInt += nSamplesToProcess;
                }
            }

            switch (numParts) {
                case 1:
                    noteSamplesDmemAddrBeforeResampling = DMEM_UNCOMPRESSED_NOTE + skipBytes;
                    break;

                case 2:
                    switch (curPart) {
                        case 0:
                            aInterl(aList++, skipBytes + DMEM_UNCOMPRESSED_NOTE,
                                    DMEM_TEMP + (SAMPLES_PER_FRAME * SAMPLE_SIZE), ALIGN8(numSamplesToLoadAdj >> 1));
                            resampledTempLen = numSamplesToLoadAdj;
                            noteSamplesDmemAddrBeforeResampling = DMEM_TEMP + (SAMPLES_PER_FRAME * SAMPLE_SIZE);
                            if (noteSub->bitField0.finished) {
                                aClearBuffer(aList++, resampledTempLen + noteSamplesDmemAddrBeforeResampling,
                                             numSamplesToLoadAdj + SAMPLES_PER_FRAME);
                            }
                            break;

                        case 1:
                            aInterl(aList++, skipBytes + DMEM_UNCOMPRESSED_NOTE,
                                    resampledTempLen + DMEM_TEMP + (SAMPLES_PER_FRAME * SAMPLE_SIZE),
                                    ALIGN8(numSamplesToLoadAdj >> 1));
                            break;
                    }

                    break;
            }

            if (noteSub->bitField0.finished) {
                break;
            }
        }
    }

    flags = A_CONTINUE;
    if (noteSub->bitField0.needsInit == 1) {
        flags = A_INIT;
        noteSub->bitField0.needsInit = 0;
    }

    flags = sp56 | flags;

    aList = AudioSynth_FinalResample(aList, synthState, aiBufLen * SAMPLE_SIZE, resampleRateFixedPoint,
                                     noteSamplesDmemAddrBeforeResampling, flags);

    if (flags & A_INIT) {
        flags = A_INIT;
    }

    gain = noteSub->gain;
    if (gain <= 0x10) {
        goto gotoenv;
    }
    aHiLoGain(aList++, gain, (aiBufLen + SAMPLES_PER_FRAME) * SAMPLE_SIZE, DMEM_TEMP, 0);
gotoenv:
    aList = AudioSynth_ProcessEnvelope(aList, noteSub, synthState, aiBufLen, DMEM_TEMP, flags);
    return aList;
}

Acmd* AudioSynth_LoadWaveSamples(Acmd* aList, NoteSubEu* noteSub, NoteSynthesisState* synthState,
                                 s32 numSamplesToLoad) {
    s32 numSamplesAvail;
    s32 numDuplicates;

    aLoadBuffer(aList++, OS_K0_TO_PHYSICAL(noteSub->waveSampleAddr), DMEM_UNCOMPRESSED_NOTE,
                WAVE_SAMPLE_COUNT * SAMPLE_SIZE);

    // Offset in the WAVE_SAMPLE_COUNT samples of gWaveSamples to start processing the wave for continuity
    synthState->samplePosInt = (u32) synthState->samplePosInt & (WAVE_SAMPLE_COUNT - 1); //% WAVE_SAMPLE_COUNT;
    // Number of samples in the initial WAVE_SAMPLE_COUNT samples available to be used to process
    numSamplesAvail = WAVE_SAMPLE_COUNT - synthState->samplePosInt;

    if (numSamplesToLoad > numSamplesAvail) {
        // Duplicate (copy) the WAVE_SAMPLE_COUNT samples as many times as needed to reach numSamplesToLoad.
        // (numSamplesToLoad - numSamplesAvail) is the number of samples missing.
        // Divide by WAVE_SAMPLE_COUNT, rounding up, to get the amount of duplicates
        numDuplicates = ((numSamplesToLoad - numSamplesAvail + WAVE_SAMPLE_COUNT - 1) >> LOG2_WAVE_SAMPLE_COUNT);
        if (numDuplicates != 0) {
            aDuplicate(aList++, numDuplicates, /* DMEM_UNCOMPRESSED_NOTE */0,
                       DMEM_UNCOMPRESSED_NOTE + (WAVE_SAMPLE_COUNT * SAMPLE_SIZE));
        }
    }
    return aList;
}

Acmd* AudioSynth_FinalResample(Acmd* aList, NoteSynthesisState* synthState, s32 size, u16 pitch, u16 inpDmem,
                               u32 resampleFlags) {
    if (pitch == 0) {
        aClearBuffer(aList++, DMEM_TEMP, size);
    } else {
        aSetBuffer(aList++, 0, inpDmem, DMEM_TEMP, size);
        aResample(aList++, resampleFlags, pitch, OS_K0_TO_PHYSICAL(synthState->synthesisBuffers->finalResampleState));
    }
    return aList;
}

Acmd* AudioSynth_ProcessEnvelope(Acmd* aList, NoteSubEu* noteSub, NoteSynthesisState* synthState, s32 aiBufLen,
                                 u16 dmemSrc, s32 flags) {
    s16 rampReverb;
    s16 rampRight;
    s16 rampLeft;
    u16 panVolLeft;
    u16 panVolRight;
    u16 curVolLeft;
    u16 curVolRight;
    s32 sourceReverbVol;
    s32 temp = 0;

    curVolLeft = synthState->curVolLeft;
    curVolRight = synthState->curVolRight;

    panVolLeft = noteSub->panVolLeft;
    panVolRight = noteSub->panVolRight;

    panVolLeft <<= 4;
    panVolRight <<= 4;

    f32 recipAiBufLenShr3 = shz_fast_invf((f32)((aiBufLen >> 3)));

    if (panVolLeft != curVolLeft) {
        rampLeft = (s16)((f32)(panVolLeft - curVolLeft) * recipAiBufLenShr3);
    } else {
        rampLeft = 0;
    }
    if (panVolRight != curVolRight) {
        rampRight = (s16)((f32)(panVolRight - curVolRight) * recipAiBufLenShr3);
        //(panVolRight - curVolRight) / (aiBufLen >> 3);
    } else {
        rampRight = 0;
    }

    sourceReverbVol = synthState->reverbVol;

    if (noteSub->reverb != sourceReverbVol) {
        temp = (((noteSub->reverb & 0x7F) - (sourceReverbVol & 0x7F)) << 8);
        rampReverb = (s32)((f32)temp  * recipAiBufLenShr3);
        synthState->reverbVol = noteSub->reverb;
    } else {
        rampReverb = 0;
    }

    synthState->curVolLeft = curVolLeft + (rampLeft * (aiBufLen >> 3));
    synthState->curVolRight = curVolRight + (rampRight * (aiBufLen >> 3));

    if (noteSub->bitField0.usesHeadsetPanEffects) {
        aClearBuffer(aList++, DMEM_HAAS_TEMP, DMEM_1CH_SIZE);
        aEnvSetup1(aList++, (sourceReverbVol & 0x7F), rampReverb, rampLeft, rampRight);
        aEnvSetup2(aList++, curVolLeft, curVolRight);

        aEnvMixer(aList++, dmemSrc, aiBufLen, DMEM_LEFT_CH);
    } else {
        aEnvSetup1(aList++, (sourceReverbVol & 0x7F), rampReverb, rampLeft, rampRight);
        aEnvSetup2(aList++, curVolLeft, curVolRight);
        aEnvMixer(aList++, dmemSrc, aiBufLen, DMEM_LEFT_CH);
    }

    return aList;
}