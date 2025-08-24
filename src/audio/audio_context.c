#include "n64sys.h"
#include "sf64audio_provisional.h"

u64 gAudioContextStart[2] = {0};

SynthesisReverb gSynthReverbs[4] = {0};
u8 sAudioContextPad10[0x10] = {0}; // 0x10
u16 D_8014C1B0 = {0};
s8 D_8014C1B2 = {0};
s8 gNumSynthReverbs = {0};
s16 D_8014C1B4 = {0};
NoteSubEu* gNoteSubsEu = {0};
// 0x4
AudioAllocPool gSessionPool = {0};
AudioAllocPool gInitPool = {0};
AudioAllocPool gMiscPool = {0};
u8 gAudioContextPad20[0x20] = {0};// 0x20
AudioAllocPool gCachePool = {0};
AudioAllocPool gPersistentCommonPool = {0};
AudioAllocPool gTemporaryCommonPool = {0};
AudioCache gSeqCache = {0};        // seqCache
AudioCache gFontCache = {0};       // fontCache
AudioCache gSampleBankCache = {0};// sampleBankCache
PermanentCache gPermanentPool = {0};
AudioSampleCache gPersistentSampleCache = {0};
// 0x4
AudioSampleCache gTemporarySampleCache = {0};
// 0x4
AudioSessionPoolSplit gSessionPoolSplit = {0};
AudioCachePoolSplit gCachePoolSplit = {0};
AudioCommonPoolSplit gPersistentCommonPoolSplit = {0};
// 0x4
AudioCommonPoolSplit gTemporaryCommonPoolSplit = {0};
// 0x4
u8 gSampleFontLoadStatus[64] = {0};
u8 gFontLoadStatus[64] = {0};
u8 gSeqLoadStatus[256] = {0};
volatile u8 gAudioResetStep = {0};
u8 gAudioSpecId = {0};
s32 gResetFadeoutFramesLeft = {0};
u8 sAudioContextPad1000[0x1000] = {0};// 0x1000 gap
Note* gNotes = {0};
// 0x4
SequencePlayer gSeqPlayers[SEQ_PLAYER_MAX] = {0};
SequenceChannel gSeqChannels[48] = {0};
SequenceLayer gSeqLayers[64] = {0};
SequenceChannel gSeqChannelNone = {0};
AudioListItem gLayerFreeList = {0};
NotePool gNoteFreeLists = {0};
Sample* gUsedSamples[128] = {0};
AudioPreloadReq gPreloadSampleStack[128] = {0};
s32 gNumUsedSamples = {0};
s32 gPreloadSampleStackTop = {0};
AudioAsyncLoad gAsyncLoads[16] = {0};
OSMesgQueue gExternalLoadQueue = {0};
OSMesg gExternalLoadMsg[16] = {0};
OSMesgQueue gPreloadSampleQueue = {0};
OSMesg gPreloadSampleMsg[16] = {0};
OSMesgQueue gCurAudioFrameDmaQueue = {0};
OSMesg gCurAudioFrameDmaMsg[64] = {0};
OSIoMesg gCurAudioFrameDmaIoMsgBuf[64] = {0};
OSMesgQueue gSyncDmaQueue = {0};
OSMesg gSyncDmaMsg[1] = {0};
// 0x4
OSIoMesg gSyncDmaIoMsg = {0};
SampleDma gSampleDmas[0x100] = {0};
u32 gSampleDmaCount = {0};
u32 gSampleDmaListSize1 = {0};
s32 D_80155A50 = {0};
// 0x4
u8 gSampleDmaReuseQueue1[0x100] = {0};
u8 gSampleDmaReuseQueue2[0x100] = {0};
u8 gSampleDmaReuseQueue1RdPos = {0};
u8 gSampleDmaReuseQueue2RdPos = {0};
u8 gSampleDmaReuseQueue1WrPos = {0};
u8 gSampleDmaReuseQueue2WrPos = {0};
AudioTable* gSequenceTable = {0};
AudioTable* gSoundFontTable = {0};
AudioTable* gSampleBankTable = {0};
u8* gSeqFontTable = {0};
s16 gNumSequences = {0};
SoundFont* gSoundFontList = {0};
// 0x4
AudioBufferParameters gAudioBufferParams = {0};
s32 gSampleDmaBuffSize = {0};
s32 gMaxAudioCmds = {0};
s32 gNumNotes = {0};
s16 gMaxTempo = {0};
s8 gAudioSoundMode = {0};
volatile s32 gAudioTaskCountQ = {0};
s32 gCurAudioFrameDmaCount = {0};
s32 gAudioTaskIndexQ = {0};
s32 gCurAiBuffIndex = {0};
Acmd* gAbiCmdBuffs[2] = {0};
Acmd* gCurAbiCmdBuffer = {0};
SPTask* gAudioCurTask = {0};
SPTask gAudioRspTasks[2] = {0};
f32 gMaxTempoTvTypeFactors = {0};
s32 gRefreshRate = {0};
s16* gAiBuffers[3] = {0};
s16 gAiBuffLengths[3] = {0};
u32 gAudioRandom = {0};
u32 gAudioErrorFlags = {0};
volatile u32 gAudioResetTimer = {0};

u64 gAudioContextEnd[2] = {0};

ReverbSettings D_800C74D0[2] = { { 1, 0x30, 0x3000, 0, 0 }, { 1, 0x40, 0x4000, 0xD000, 0x3000 } };
ReverbSettings D_800C74E0[2] = { { 1, 0x30, 0x3000, 0, 0 }, { 1, 0x38, 0x6400, 0x1000, 0x1000 } };
ReverbSettings D_800C74F0[2] = { { 1, 0x30, 0x3000, 0, 0 }, { 1, 0x40, 0x4000, 0, 0 } };
ReverbSettings D_800C7500[2] = { { 1, 0x30, 0x3000, 0, 0 }, { 1, 0x30, 0x5000, 0, 0 } };
ReverbSettings D_800C7510[2] = { { 1, 0x30, 0x3000, 0, 0 }, { 1, 0x40, 0x6000, 0, 0 } };
ReverbSettings D_800C7520[2] = { { 1, 0x30, 0x3000, 0, 0 }, { 1, 0x30, 0x3000, 0, 0 } };
ReverbSettings D_800C7530[2] = { { 1, 0x30, 0x3000, 0, 0 }, { 1, 0x30, 0x5800, 0, 0 } };
ReverbSettings D_800C7540[2] = { { 1, 0x30, 0x3000, 0, 0 }, { 1, 0x30, 0x5000, 0, 0 } };
ReverbSettings D_800C7550[2] = { { 1, 0x30, 0x3000, 0, 0 }, { 1, 0x30, 0x3000, 0, 0 } };
ReverbSettings D_800C7560[2] = { { 1, 0x30, 0x3000, 0, 0 }, { 1, 0x40, 0x6000, 0, 0 } };
ReverbSettings D_800C7570[2] = { { 1, 0x30, 0x3000, 0, 0 }, { 1, 0x30, 0x3000, 0, 0 } };
ReverbSettings D_800C7580[2] = { { 1, 0x30, 0x3000, 0, 0 }, { 1, 0x40, 0x4000, 0, 0 } };
ReverbSettings D_800C7590[2] = { { 1, 0x30, 0x3000, 0, 0 }, { 2, 0x28, 0x5800, 0, 0 } };
ReverbSettings D_800C75A0[2] = { { 1, 0x30, 0x3000, 0, 0 }, { 1, 0x30, 0x3000, 0, 0 } };
ReverbSettings D_800C75B0[2] = { { 1, 0x30, 0x3000, 0, 0 }, { 1, 0x38, 0x5000, 0, 0 } };
ReverbSettings D_800C75C0[3] = {
    { 1, 0x30, 0x3000, 0, 0 },
    { 1, 0x38, 0x3000, 0, 0 },
    { 2, 0x48, 0x6000, 0xC000, 0x4000 },
};
ReverbSettings D_800C75D8[2] = { { 1, 0x30, 0x3000, 0, 0 }, { 1, 0x30, 0x3000, 0, 0 } };
ReverbSettings D_800C75E8[2] = { { 1, 0x30, 0x3000, 0, 0 }, { 1, 0x30, 0x3000, 0, 0 } };
ReverbSettings D_800C75F8[2] = { { 1, 0x30, 0x3000, 0, 0 }, { 1, 0x30, 0x3000, 0, 0 } };
ReverbSettings D_800C7608[2] = { { 1, 0x30, 0x3000, 0, 0 }, { 1, 0x30, 0x3000, 0, 0 } };
ReverbSettings D_800C7618[2] = { { 1, 0x30, 0x3000, 0, 0 }, { 1, 0x30, 0x3000, 0, 0 } };
ReverbSettings D_800C7628[2] = { { 1, 0x30, 0x3000, 0, 0 }, { 1, 0x30, 0x3000, 0, 0 } };
ReverbSettings D_800C7638[2] = { { 1, 0x30, 0x3000, 0, 0 }, { 1, 0x30, 0x6800, 0, 0 } };
ReverbSettings D_800C7648[2] = { { 1, 0x30, 0x3000, 0, 0 }, { 1, 0x18, 0x3000, 0, 0 } };
ReverbSettings D_800C7658[2] = { { 1, 0x30, 0x3000, 0, 0 }, { 2, 0x18, 0x3000, 0, 0 } };
ReverbSettings D_800C7668[2] = { { 1, 0x30, 0x3000, 0, 0 }, { 1, 0x18, 0x3000, 0, 0 } };
ReverbSettings D_800C7678[2] = { { 1, 0x30, 0x3000, 0, 0 }, { 1, 0x30, 0x3000, 0, 0 } };
ReverbSettings D_800C7688[2] = { { 1, 0x30, 0x3000, 0, 0 }, { 1, 0x30, 0x3000, 0, 0 } };
ReverbSettings D_800C76A8[4] = {
    // unused?
    { 1, 0x40, 0x4FFF, 0, 0 },
    { 1, 0x30, 0x4FFF, 0, 0 },
    { 1, 0x30, 0x4FFF, 0, 0 },
    { 1, 0x30, 0x4FFF, 0, 0 },
};
AudioSpec gAudioSpecs[29] = {
    /*  0 */ { 26800, 2, 22, ARRAY_COUNT(D_800C74D0), D_800C74D0, 0x7FFF, 0x1200, 0x1100, 0, 0x5000, 0x2400, 0, 0x1B000,
               0x35000 },
    /*  1 */
    { 26800, 2, 22, ARRAY_COUNT(D_800C74E0), D_800C74E0, 0x7FFF, 0x1200, 0x1100, 0, 0x5000, 0x2400, 0, 0x1B000,
      0x35000 },
    /*  2 */
    { 26800, 2, 22, ARRAY_COUNT(D_800C74F0), D_800C74F0, 0x7FFF, 0x1200, 0x1100, 0, 0x5000, 0x2400, 0, 0x1B000,
      0x35000 },
    /*  3 */
    { 26800, 2, 22, ARRAY_COUNT(D_800C7500), D_800C7500, 0x7FFF, 0x1200, 0x1100, 0, 0x5000, 0x2400, 0, 0x1B000,
      0x35000 },
    /*  4 */
    { 26800, 2, 22, ARRAY_COUNT(D_800C7510), D_800C7510, 0x7FFF, 0x1200, 0x1100, 0, 0x5000, 0x2400, 0, 0x1B000,
      0x35000 },
    /*  5 */
    { 26800, 2, 22, ARRAY_COUNT(D_800C7520), D_800C7520, 0x7FFF, 0x1200, 0x1100, 0, 0x5000, 0x2400, 0, 0x1B000,
      0x35000 },
    /*  6 */
    { 26800, 2, 22, ARRAY_COUNT(D_800C7530), D_800C7530, 0x7FFF, 0x1200, 0x1100, 0, 0x5000, 0x2400, 0, 0x1B000,
      0x35000 },
    /*  7 */
    { 26800, 2, 22, ARRAY_COUNT(D_800C7540), D_800C7540, 0x7FFF, 0x1200, 0x1100, 0, 0x5000, 0x2400, 0, 0x1B000,
      0x35000 },
    /*  8 */
    { 26800, 2, 22, ARRAY_COUNT(D_800C7550), D_800C7550, 0x7FFF, 0x1200, 0x1100, 0, 0x5000, 0x2400, 0, 0x1B000,
      0x35000 },
    /*  9 */
    { 26800, 2, 22, ARRAY_COUNT(D_800C7560), D_800C7560, 0x7FFF, 0x1200, 0x1100, 0, 0x5000, 0x2400, 0, 0x1B000,
      0x35000 },
    /* 10 */
    { 26800, 2, 22, ARRAY_COUNT(D_800C7570), D_800C7570, 0x7FFF, 0x1200, 0x1100, 0, 0x5000, 0x2400, 0, 0x1B000,
      0x35000 },
    /* 11 */
    { 26800, 2, 22, ARRAY_COUNT(D_800C7580), D_800C7580, 0x7FFF, 0x1200, 0x1100, 0, 0x5000, 0x2400, 0, 0x1B000,
      0x35000 },
    /* 12 */
    { 26800, 1, 22, ARRAY_COUNT(D_800C7590), D_800C7590, 0x7FFF, 0x1200, 0x1100, 0, 0x5000, 0x2400, 0, 0x2B000,
      0x35000 },
    /* 13 */
    { 26800, 2, 22, ARRAY_COUNT(D_800C75A0), D_800C75A0, 0x7FFF, 0x1200, 0x1100, 0, 0x5000, 0x2400, 0, 0x1B000,
      0x35000 },
    /* 14 */
    { 26800, 2, 22, ARRAY_COUNT(D_800C75B0), D_800C75B0, 0x7FFF, 0x1200, 0x1100, 0, 0x5000, 0x2400, 0, 0x1B000,
      0x35000 },
    /* 15 */
    { 26800, 2, 22, ARRAY_COUNT(D_800C75C0), D_800C75C0, 0x7FFF, 0x1200, 0x1100, 0, 0x5000, 0x2400, 0, 0x1B000,
      0x34000 },
    /* 16 */
    { 26800, 2, 22, ARRAY_COUNT(D_800C75D8), D_800C75D8, 0x7FFF, 0x1200, 0x1100, 0, 0x5000, 0x2400, 0, 0x1B000,
      0x35000 },
    /* 17 */
    { 26800, 2, 22, ARRAY_COUNT(D_800C75E8), D_800C75E8, 0x7FFF, 0x1200, 0x1100, 0, 0x5000, 0x2400, 0, 0x1B000,
      0x35000 },
    /* 18 */
    { 26800, 2, 22, ARRAY_COUNT(D_800C75F8), D_800C75F8, 0x7FFF, 0x1200, 0x1100, 0, 0x5000, 0x2400, 0, 0x1B000,
      0x35000 },
    /* 19 */
    { 26800, 2, 22, ARRAY_COUNT(D_800C7608), D_800C7608, 0x7FFF, 0x1200, 0x1100, 0, 0x5000, 0x2400, 0, 0x1B000,
      0x35000 },
    /* 20 */
    { 26800, 2, 22, ARRAY_COUNT(D_800C7618), D_800C7618, 0x7FFF, 0x1200, 0x1100, 0, 0x5000, 0x2400, 0, 0x1B000,
      0x35000 },
    /* 21 */
    { 26800, 2, 32, ARRAY_COUNT(D_800C7628), D_800C7628, 0x7FFF, 0x1200, 0x1100, 0, 0x5000, 0x2400, 0, 0x1B000,
      0x00000 },
    /* 22 */
    { 26800, 1, 32, ARRAY_COUNT(D_800C7638), D_800C7638, 0x7FFF, 0x1200, 0x1100, 0, 0x5000, 0x2400, 0, 0x1B000,
      0x00000 },
    /* 23 */
    { 26800, 1, 32, ARRAY_COUNT(D_800C7648), D_800C7648, 0x7FFF, 0x1200, 0xA000, 0, 0x5B00, 0x1D00, 0, 0x00000,
      0x00000 },
    /* 24 */
    { 26800, 1, 22, ARRAY_COUNT(D_800C7658), D_800C7658, 0x7FFF, 0x1200, 0x1100, 0, 0x5000, 0x2400, 0, 0x00000,
      0x60000 },
    /* 25 */
    { 26800, 2, 22, ARRAY_COUNT(D_800C7668), D_800C7668, 0x7FFF, 0x1200, 0x1100, 0, 0x5000, 0x2400, 0, 0x1B000,
      0x00000 },
    /* 26 */
    { 26800, 2, 22, ARRAY_COUNT(D_800C7678), D_800C7678, 0x7FFF, 0x1200, 0x1100, 0, 0x5000, 0x2400, 0, 0x1B000,
      0x00000 },
    /* 27 */
    { 26800, 2, 32, ARRAY_COUNT(D_800C7688), D_800C7688, 0x7FFF, 0x1200, 0x1100, 0, 0x5000, 0x2400, 0, 0x1B000,
      0x00000 },
    /* 28 */
    { 26800, 2, 22, ARRAY_COUNT(D_800C74D0), D_800C74D0, 0x7FFF, 0x1200, 0x1100, 0, 0x5000, 0x2400, 0, 0x1B000,
      0x35000 },
};

s16 gSeqTicksPerBeat = 0x30;
s32 gAudioHeapSize = 0xAFE00;
s32 gInitPoolSize = 0x26000;
u32 gPermanentPoolSize = 0x21000;
u16 gSequenceMedium = 0;
u16 gSoundFontMedium = 0;
u16 gSampleBankMedium = 0;
