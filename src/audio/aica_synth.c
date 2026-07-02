/*
 * AICA hardware-mixed voice driver (Dreamcast) for Star Fox 64.
 *
 * Same EU audio engine as OoT, so this is the OoT driver adapted to SF64's
 * NoteSubEu (panVolLeft/Right, resampleRate, waveSampleAddr) plus the SM64
 * long-sample streamer for samples too long for a single AICA channel.
 * Stock KOS firmware only; snd_sh4_to_aica is the one non-public symbol.
 */

#include "sf64audio_provisional.h"
#include "aica_synth.h"

#include <dc/sound/sound.h>
#include <dc/spu.h>
#include <dc/sound/aica_comm.h>
#include <arch/timer.h>

#include <stdio.h>
#define AICA_DEBUG 0
#define AICA_OCTAVE_LOG 0   /* log ADPCM samples pitched past +1 octave -> FORCE_PCM candidates */
#define AICA_DROP_LOG 1     /* instrument every silent drop (ARAM/cache/stream/channel/resolve); capped per site */

/* Log a dropped voice/stream and WHY. Each call site caps independently so a
   recurring drop can't flood the console; a "<site capped>" marker prints once
   when a site hits its limit. Prefix "AICADROP " makes them greppable on dcload. */
#if AICA_DROP_LOG
#define AICA_DROP(fmt, ...) do { static u32 _n = 0; if (_n < 200) { printf("AICADROP " fmt "\n", ##__VA_ARGS__); if (++_n == 200) printf("AICADROP <site capped>\n"); } } while (0)
#else
#define AICA_DROP(fmt, ...) ((void)0)
#endif

extern int snd_sh4_to_aica(void* packet, uint32_t size);
extern AudioTable* gSampleBankTable;     /* base.romAddr = audio_table device base */
extern NoteSubEu* gNoteSubsEu;
extern Note* gNotes;                     /* source notes; set noteSubEu.finished here */
extern s32 gNumNotes;
extern s16* gWaveSamples[];

#include "aica_sample_table.h"

const unsigned char* gAicaAdpcmPoolBase = NULL;   /* resident adpcm_pool.bin */
static u32 sTblBase = 0;                            /* gSampleBankTable->base.romAddr */

#define NUM_AICA_CHANNELS 64
#define MAX_VOICES 64
#define ARAM_CACHE_ENTRIES 192
#define AICA_LEN_MAX 65534
/* Headroom for full-scale synth waves summing on the AICA mix bus (no HW overflow
   protection). Out of 256 (256 = unity); tune on HW. */
#define SYNTH_VOL_SCALE 192
#define WAVE_SAMPLES 64          /* WAVE_SAMPLE_COUNT */
#define NUM_WAVEFORMS 6          /* saw, tri, sine, square, noise, unk */
#define NUM_HARMONICS 4
#define OUTPUT_RATE 32000
#define KEY_EMPTY 0xFFFFFFFFu
#define SYNTH_KEY(wf, h) (0x80000000u | ((u32)(wf) << 4) | (u32)(h))

typedef struct { u32 key, aram, len; s32 refs; u32 lru; } AramEntry;
static AramEntry sCache[ARAM_CACHE_ENTRIES];
static u32 sTick;

static s8 sChanFree[NUM_AICA_CHANNELS];
static s32 sChanFreeTop;

typedef struct { s8 channel; u8 active; u32 sampleKey; AramEntry* entry;
                 u8 downsampleShift; u32 sentFreq; u8 sentVol, sentPan; } Voice;
static Voice sVoices[MAX_VOICES];
static u32 sWaveAram[NUM_WAVEFORMS][NUM_HARMONICS];

typedef struct { u32 base, type, length, loop, loopstart, loopend; u8 downsample_shift; } Resolved;

/* ---- long-sample streamer (PCM16 ARAM ring, SH4-decoded, timer-paced) ---- */
#define MAX_STREAMS 12           /* every >65534 sample streams; size for concurrency.
                                    Hard ceiling is gNumNotes (22): streams are keyed by
                                    note slot, a note is voice XOR stream, so channel use
                                    stays <= gNumNotes. Each slot costs one 8KB ARAM ring
                                    (allocated at init) + one AICA channel while active. */
#define STREAM_RING_SAMPLES 4096u
#define STREAM_GUARD 256u
static const s32 ADPCM_DIFF[16] = { 1,3,5,7,9,11,13,15,-1,-3,-5,-7,-9,-11,-13,-15 };
static const s32 ADPCM_SCALE[8] = { 0xE6,0xE6,0xE6,0xE6,0x133,0x199,0x200,0x266 };
typedef struct {
    s32 noteIndex, channel; u32 ringAram, key;
    const u8* src; u32 nsamples, loopStart, loopEnd; u8 loopFlag;
    s32 cur, quant; u32 srcPos; s32 loopCur, loopQuant; u32 haveLoopSnap;
    u32 written, freq; u64 startUs; int done;
    u32 sentFreq; u8 sentVol, sentPan;   /* last freq/vol/pan pushed to AICA; gate re-push */
} Stream;
static Stream sStreams[MAX_STREAMS];


static int sample_lookup(u32 key, const AicaSampleDesc** out) {
    s32 lo = 0, hi = AICA_SAMPLE_COUNT - 1;
    while (lo <= hi) {
        s32 mid = (lo + hi) >> 1;
        u32 k = gAicaSampleTable[mid].src_offset;
        if (k == key) { *out = &gAicaSampleTable[mid]; return 1; }
        if (k < key) lo = mid + 1; else hi = mid - 1;
    }
    return 0;
}

static AramEntry* cache_find(u32 key) {
    s32 i;
    for (i = 0; i < ARAM_CACHE_ENTRIES; i++) if (sCache[i].key == key) return &sCache[i];
    return NULL;
}

static AramEntry* cache_acquire(u32 key, u32 pool_offset, u32 byte_len) {
    AramEntry* e = cache_find(key);
    s32 i;
    if (e) { e->refs++; e->lru = sTick; return e; }
    u32 aram = (u32)snd_mem_malloc(byte_len);
    /* ARAM full: evict the LRU unreferenced entry and retry, looping until the
       alloc fits or nothing more can be freed. One eviction isn't enough when the
       new sample is larger than the freed block or ARAM is fragmented -- exactly
       the case for big voice/speech samples. Referenced (playing) entries are
       never touched, so a busy frame can still legitimately fail (-> drop+log). */
    while (aram == 0) {
        AramEntry* victim = NULL;
        for (i = 0; i < ARAM_CACHE_ENTRIES; i++)
            if (sCache[i].key != KEY_EMPTY && sCache[i].refs == 0)
                if (!victim || sCache[i].lru < victim->lru) victim = &sCache[i];
        if (!victim) { AICA_DROP("ARAMFULL key=%X len=%u (no evictable entry left)", (unsigned)key, (unsigned)byte_len); return NULL; }
        snd_mem_free(victim->aram); victim->key = KEY_EMPTY; victim->aram = 0;
        aram = (u32)snd_mem_malloc(byte_len);
    }
    spu_memload_sq(aram, (void*)(gAicaAdpcmPoolBase + pool_offset), (byte_len + 31) & ~31);
    e = cache_find(KEY_EMPTY);
    if (!e) {
        /* TABLE full -- a DIFFERENT resource from the ARAM-full loop above. The ARAM alloc already
           succeeded, so that loop never ran and never freed a slot; without this the 192-slot table
           just fills and never recycles (accumulates across levels -> CACHETBLFULL forever). Evict
           the LRU UNREFERENCED entry to reclaim its slot (frees its ARAM too -- harmless, we keep the
           block we just allocated). Referenced (playing) entries are never touched. */
        AramEntry* victim = NULL;
        for (i = 0; i < ARAM_CACHE_ENTRIES; i++)
            if (sCache[i].key != KEY_EMPTY && sCache[i].refs == 0)
                if (!victim || sCache[i].lru < victim->lru) victim = &sCache[i];
        if (!victim) { snd_mem_free(aram); AICA_DROP("CACHETBLFULL key=%X (all %d entries resident)", (unsigned)key, ARAM_CACHE_ENTRIES); return NULL; }
        snd_mem_free(victim->aram); victim->key = KEY_EMPTY; victim->aram = 0;
        e = victim;
    }
    e->key = key; e->aram = aram; e->len = byte_len; e->refs = 1; e->lru = sTick;
    return e;
}

static void cache_release(AramEntry* e) { if (e && e->refs > 0) e->refs--; }

/* Clean-slate hook for scene/memory resets (called from nuke_everything). Frees every RESIDENT but
   UNREFERENCED cache entry so a previous level's samples don't linger in the 192-slot table + ARAM
   pool and starve the next level (the accumulation that was hitting CACHETBLFULL). Currently-playing
   voices (refs>0) are left intact, so this is safe to call at any time -- the on-demand LRU eviction
   in cache_acquire reclaims the rest as needed. */
void AicaSynth_ClearSampleCache(void) {
    s32 i;
    for (i = 0; i < ARAM_CACHE_ENTRIES; i++) {
        if (sCache[i].key != KEY_EMPTY && sCache[i].refs == 0) {
            if (sCache[i].aram) snd_mem_free(sCache[i].aram);
            sCache[i].key = KEY_EMPTY; sCache[i].aram = 0;
        }
    }
}

/* KOS aica_freq firmware octave-gap fix (see MK64/SM64). */
static u32 fix_aica_freq_gap(u32 freq) {
    u32 base = 5644800; s32 hi = 7;
    while (freq < base && hi > -8) { base >>= 1; hi--; }
    if (base != 0 && freq >= 2 * base) freq = 2 * base - 1;
    return freq;
}

/* resampleRate is rate*32768 (input/output ratio); freq = rate * OUTPUT_RATE. */
static u32 calc_freq(NoteSubEu* sub, u8 shift) {
    u32 freq = ((u32)sub->resampleRate * (u32)OUTPUT_RATE) >> 15;
    if (sub->bitField1.hasTwoParts) freq <<= 1;
    freq >>= shift;
    if (freq == 0) freq = 1;
    return fix_aica_freq_gap(freq);
}

static u32 calc_vol(NoteSubEu* sub) {
    u32 l = sub->panVolLeft, r = sub->panVolRight;
    u32 m = (l > r) ? l : r;
    u32 v = m >> 4;
    if (v > 255) v = 255;
    /* Headroom for full-scale synth waves summing without clipping. */
    if (sub->bitField1.isSyntheticWave) v = (v * SYNTH_VOL_SCALE) >> 8;
    return v;
}

static u32 calc_pan(NoteSubEu* sub) {
    s32 l = sub->panVolLeft, r = sub->panVolRight;
    s32 hi, lo, amount; int left;
    if (l == r) return 128;
    if (l > r) { hi = l; lo = r; left = 1; } else { hi = r; lo = l; left = 0; }
    if (lo <= 0) amount = 15;
    else { amount = 0; while (amount < 15 && (s64)lo * 1000 < (s64)hi * 708) { lo = (s32)(((s64)lo * 1000) / 708); amount++; } }
    return left ? (u32)(127 - amount * 8) : (u32)(128 + amount * 8);
}

/* Returns 0 skip, 1 normal voice, 2 streamed long sample (*outDesc filled). */
static int resolve(NoteSubEu* sub, u32* outKey, Resolved* res, AramEntry** outEntry, const AicaSampleDesc** outDesc) {
    *outEntry = NULL; *outDesc = NULL; res->downsample_shift = 0;

    if (sub->bitField1.isSyntheticWave) {
        s16* addr = sub->waveSampleAddr;
        s32 wf = -1; u32 h = 0, w;
        for (w = 0; w < NUM_WAVEFORMS; w++)
            if (addr >= gWaveSamples[w] && addr < gWaveSamples[w] + NUM_HARMONICS * WAVE_SAMPLES) {
                wf = (s32)w; h = (u32)(addr - gWaveSamples[w]) / WAVE_SAMPLES; break;
            }
        if (wf < 0) return 0;
        if (h >= NUM_HARMONICS) h = NUM_HARMONICS - 1;
        *outKey = SYNTH_KEY(wf, h);
        res->base = sWaveAram[wf][h]; res->type = AICA_SM_16BIT;
        res->length = WAVE_SAMPLES; res->loop = 1; res->loopstart = 0; res->loopend = WAVE_SAMPLES;
        return 1;
    }

    if (gAicaAdpcmPoolBase == NULL || sub->waveSampleAddr == NULL) return 0;
    {
        Sample* s = *((Sample**) sub->waveSampleAddr);
        const AicaSampleDesc* d;
        u32 key;
        AramEntry* e;
        if (s == NULL || s->sampleAddr == NULL) return 0;
        key = (u32)s->sampleAddr - sTblBase;
        if (!sample_lookup(key, &d)) return 0;
        if (d->stream) { *outDesc = d; *outKey = key; return 2; }
        e = cache_acquire(key, d->pool_offset, d->byte_len);
        if (!e) return 0;
        *outEntry = e; *outKey = key;
        res->base = e->aram; res->type = d->fmt;   /* AICA_SM_16BIT/8BIT/ADPCM, chosen offline */
        res->length = d->nsamples > AICA_LEN_MAX ? AICA_LEN_MAX : d->nsamples;
        res->loop = d->loop_flag; res->loopstart = d->loop_start;
        res->loopend = d->loop_end ? d->loop_end : res->length;
        res->downsample_shift = d->downsample_shift;
        return 1;
    }
}

static void chan_start(s32 ch, const Resolved* r, u32 freq, u32 vol, u32 pan) {
    AICA_CMDSTR_CHANNEL(tmp, cmd, chan);
    cmd->cmd = AICA_CMD_CHAN; cmd->timestamp = 0; cmd->size = AICA_CMDSTR_CHANNEL_SIZE; cmd->cmd_id = ch;
    chan->cmd = AICA_CH_CMD_START;
    chan->base = r->base; chan->type = r->type; chan->length = r->length;
    chan->loop = r->loop; chan->loopstart = r->loopstart; chan->loopend = r->loopend;
    chan->freq = freq; chan->vol = vol; chan->pan = pan;
    snd_sh4_to_aica(tmp, cmd->size);
}

static void chan_update(s32 ch, u32 freq, u32 vol, u32 pan) {
    AICA_CMDSTR_CHANNEL(tmp, cmd, chan);
    cmd->cmd = AICA_CMD_CHAN; cmd->timestamp = 0; cmd->size = AICA_CMDSTR_CHANNEL_SIZE; cmd->cmd_id = ch;
    chan->cmd = AICA_CH_CMD_UPDATE | AICA_CH_UPDATE_SET_FREQ | AICA_CH_UPDATE_SET_VOL | AICA_CH_UPDATE_SET_PAN;
    chan->freq = freq; chan->vol = vol; chan->pan = pan;
    snd_sh4_to_aica(tmp, cmd->size);
}

static void chan_stop(s32 ch) {
    AICA_CMDSTR_CHANNEL(tmp, cmd, chan);
    cmd->cmd = AICA_CMD_CHAN; cmd->timestamp = 0; cmd->size = AICA_CMDSTR_CHANNEL_SIZE; cmd->cmd_id = ch;
    chan->cmd = AICA_CH_CMD_STOP;
    snd_sh4_to_aica(tmp, cmd->size);
}

static s32 chan_alloc(void) { return sChanFreeTop ? sChanFree[--sChanFreeTop] : -1; }
static void chan_release(s32 ch) {
    if (sChanFreeTop < NUM_AICA_CHANNELS) sChanFree[sChanFreeTop++] = (s8)ch;
    else AICA_DROP("CHANOVERRELEASE ch=%d top=%d (double-release/bookkeeping bug)", (int)ch, (int)sChanFreeTop);
}

static void voice_stop(s32 i) {
    Voice* v = &sVoices[i];
    if (v->channel >= 0) { chan_stop(v->channel); chan_release(v->channel); }
    cache_release(v->entry);
    v->channel = -1; v->active = 0; v->entry = NULL; v->sampleKey = KEY_EMPTY;
    v->downsampleShift = 0; v->sentFreq = 0; v->sentVol = 0; v->sentPan = 0;
}

/* ---- streamer ---- */
static s16 stream_decode_one(Stream* s) {
    s32 code, m, delta, c, q; u8 byte;
    if (!s->loopFlag && s->srcPos >= s->nsamples) return 0;
    if (s->loopFlag && !s->haveLoopSnap && s->srcPos == s->loopStart) { s->loopCur = s->cur; s->loopQuant = s->quant; s->haveLoopSnap = 1; }
    byte = s->src[s->srcPos >> 1];
    code = (s->srcPos & 1) ? ((byte >> 4) & 0xF) : (byte & 0xF);
    m = code & 7;
    delta = (s->quant * ADPCM_DIFF[code]) / 8;
    c = s->cur + delta; if (c < -32768) c = -32768; else if (c > 32767) c = 32767; s->cur = c;
    q = (s->quant * ADPCM_SCALE[m]) >> 8; if (q < 127) q = 127; else if (q > 24576) q = 24576; s->quant = q;
    s->srcPos++;
    if (s->loopFlag && s->srcPos >= s->loopEnd) { s->cur = s->loopCur; s->quant = s->loopQuant; s->srcPos = s->loopStart; }
    return (s16)c;
}

static void stream_fill(Stream* s, u32 target) {
    u32 cap = s->written + 4 * STREAM_RING_SAMPLES;
    while (s->written < target && s->written < cap) {
        s16 tmp[16]; u32 off, k;
        for (k = 0; k < 16; k++) tmp[k] = stream_decode_one(s);
        off = s->written & (STREAM_RING_SAMPLES - 1);
        spu_memload_sq(s->ringAram + off * 2, tmp, 32);
        s->written += 16;
    }
}

static void stream_free(Stream* s) {
    if (s->channel >= 0) { chan_stop(s->channel); chan_release(s->channel); }
    s->channel = -1; s->noteIndex = -1; s->key = KEY_EMPTY;
}

static Stream* stream_for_note(s32 noteIndex) {
    s32 i;
    for (i = 0; i < MAX_STREAMS; i++) if (sStreams[i].noteIndex == noteIndex) return &sStreams[i];
    return NULL;
}

static void stream_start(s32 noteIndex, const AicaSampleDesc* d, u32 freq, u32 vol, u32 pan) {
    Stream* s = NULL; s32 i, ch;
    for (i = 0; i < MAX_STREAMS; i++) if (sStreams[i].noteIndex < 0) { s = &sStreams[i]; break; }
    if (!s) {
        s32 nDone = 0, k;
        for (k = 0; k < MAX_STREAMS; k++) if (sStreams[k].done) nDone++;
        AICA_DROP("STREAMSLOTFULL note=%d key=%X (all %d busy, %d done-squatting)", (int)noteIndex, (unsigned)d->src_offset, MAX_STREAMS, (int)nDone);
        return;
    }
    ch = chan_alloc(); if (ch < 0) { AICA_DROP("STREAMCHANFULL note=%d key=%X", (int)noteIndex, (unsigned)d->src_offset); return; }
    s->noteIndex = noteIndex; s->channel = ch; s->key = d->src_offset;
    s->src = gAicaAdpcmPoolBase + d->pool_offset;
    s->nsamples = d->nsamples; s->loopFlag = d->loop_flag;
    s->loopStart = d->loop_start; s->loopEnd = d->loop_end ? d->loop_end : d->nsamples;
    s->cur = 0; s->quant = 127; s->srcPos = 0; s->haveLoopSnap = 0; s->written = 0; s->freq = freq;
    s->done = 0;
    stream_fill(s, STREAM_RING_SAMPLES);
    {
        AICA_CMDSTR_CHANNEL(tmp, cmd, chan);
        cmd->cmd = AICA_CMD_CHAN; cmd->timestamp = 0; cmd->size = AICA_CMDSTR_CHANNEL_SIZE; cmd->cmd_id = ch;
        chan->cmd = AICA_CH_CMD_START;
        chan->base = s->ringAram; chan->type = AICA_SM_16BIT; chan->length = STREAM_RING_SAMPLES;
        chan->loop = 1; chan->loopstart = 0; chan->loopend = STREAM_RING_SAMPLES;
        chan->freq = freq; chan->vol = vol; chan->pan = pan;
        snd_sh4_to_aica(tmp, cmd->size);
    }
    /* the START command above already carried freq/vol/pan; seed the sent-state so
       the first stream_service doesn't re-push identical values. */
    s->sentFreq = freq; s->sentVol = (u8)vol; s->sentPan = (u8)pan;
    s->startUs = timer_us_gettime64();
}

static int stream_service(Stream* s, u32 vol, u32 pan) {
    u64 now = timer_us_gettime64();
    u32 consumed = (u32)(((u64)s->freq * (now - s->startUs)) / 1000000ULL);
    u32 target;
    if (!s->loopFlag && consumed >= s->nsamples) return 0;
    target = (consumed + STREAM_RING_SAMPLES - STREAM_GUARD) & ~15u;
    if (target > consumed + STREAM_RING_SAMPLES) target = consumed + STREAM_RING_SAMPLES;
    stream_fill(s, target);
    /* Gate the re-push: only touch the SH4->AICA queue when freq/vol/pan actually
       changed since we last sent them. On a stream-heavy level this cuts a per-tick
       command per active stream (and keeps catch-up bursts nearly free for streams,
       since the wall-clock fill above is already a no-op on a re-run). */
    if (s->freq != s->sentFreq || (u8)vol != s->sentVol || (u8)pan != s->sentPan) {
        chan_update(s->channel, s->freq, vol, pan);
        s->sentFreq = s->freq; s->sentVol = (u8)vol; s->sentPan = (u8)pan;
    }
    return 1;
}

void AicaSynth_Init(void) {
    s32 i, ch; u32 w, h;
    for (i = 0; i < ARAM_CACHE_ENTRIES; i++) { sCache[i].key = KEY_EMPTY; sCache[i].aram = 0; sCache[i].refs = 0; }
    for (i = 0; i < MAX_VOICES; i++) { sVoices[i].channel = -1; sVoices[i].active = 0; sVoices[i].entry = NULL; sVoices[i].sampleKey = KEY_EMPTY; sVoices[i].downsampleShift = 0; sVoices[i].sentFreq = 0; sVoices[i].sentVol = 0; sVoices[i].sentPan = 0; }
    sChanFreeTop = 0;
    for (ch = NUM_AICA_CHANNELS - 1; ch >= 0; ch--) sChanFree[sChanFreeTop++] = (s8)ch;

    sTblBase = (u32)gSampleBankTable->base.romAddr;

    for (w = 0; w < NUM_WAVEFORMS; w++) {
        u32 bytes = NUM_HARMONICS * WAVE_SAMPLES * sizeof(s16);
        u32 aram = (u32)snd_mem_malloc(bytes);
        if (aram == 0) AICA_DROP("INITWAVEFAIL w=%u bytes=%u (synth wavetable not staged)", (unsigned)w, (unsigned)bytes);
        spu_memload_sq(aram, (void*)gWaveSamples[w], (bytes + 31) & ~31);
        for (h = 0; h < NUM_HARMONICS; h++) sWaveAram[w][h] = aram + h * WAVE_SAMPLES * sizeof(s16);
    }
    for (i = 0; i < MAX_STREAMS; i++) {
        sStreams[i].noteIndex = -1; sStreams[i].channel = -1; sStreams[i].key = KEY_EMPTY;
        sStreams[i].ringAram = (u32)snd_mem_malloc(STREAM_RING_SAMPLES * sizeof(s16));
    }
}

/* Re-push freq/vol/pan for active (normal, non-stream) voices at sub-tick `tick`,
   reading that tick's gNoteSubsEu slice, so fast pan/tremolo/vibrato keep sub-frame
   resolution instead of being snapped to the last tick once per frame. Lifecycle +
   streams stay in AicaSynth_Update. Gated on change. Safe before init (active=0). */
void AicaSynth_RefreshActive(s32 tick) {
    s32 numNotes = gNumNotes;
    s32 base = tick * gNumNotes;
    s32 i;
    if (gAicaAdpcmPoolBase == NULL) return;
    if (numNotes > MAX_VOICES) numNotes = MAX_VOICES;

    for (i = 0; i < numNotes; i++) {
        Voice* v = &sVoices[i];
        NoteSubEu* sub = &gNoteSubsEu[base + i];
        u32 freq, vol, pan;
        if (!v->active || v->channel < 0) continue;
        if (!sub->bitField0.enabled || sub->bitField0.finished) continue;
        freq = calc_freq(sub, v->downsampleShift);
        vol = calc_vol(sub);
        pan = calc_pan(sub);
        if (freq != v->sentFreq || (u8)vol != v->sentVol || (u8)pan != v->sentPan) {
            chan_update(v->channel, freq, vol, pan);
            v->sentFreq = freq; v->sentVol = (u8)vol; v->sentPan = (u8)pan;
        }
    }
}

void AicaSynth_Update(void) {
    s32 numNotes = gNumNotes;
    s32 base, i;
    if (gAicaAdpcmPoolBase == NULL) return;
    sTick++;
    if (numNotes > MAX_VOICES) numNotes = MAX_VOICES;
    base = gNumNotes * (gAudioBufferParams.ticksPerUpdate - 1);

#if AICA_DEBUG
    static u32 sDbgFrame = 0;
    s32 nEn = 0, nNorm = 0, nStream = 0, nSynth = 0, nFail = 0;
    u32 vMax = 0, vMin = 99999;
#endif

    for (i = 0; i < numNotes; i++) {
        NoteSubEu* sub = &gNoteSubsEu[base + i];
        Voice* v = &sVoices[i];
        u32 key, freq, vol, pan;
        Resolved res;
        AramEntry* entry;
        const AicaSampleDesc* desc;
        Stream* st;
        int retrigger, rc;

        if (!sub->bitField0.enabled || sub->bitField0.finished) {
            if (v->active) voice_stop(i);
            st = stream_for_note(i); if (st) stream_free(st);
            continue;
        }
#if AICA_DEBUG
        nEn++; if (sub->bitField1.isSyntheticWave) nSynth++;
#endif
        rc = resolve(sub, &key, &res, &entry, &desc);
        if (rc == 0) {
#if AICA_DEBUG
            nFail++;
#endif
            AICA_DROP("RESOLVE0 note=%d synth=%d (sample unresolved/ARAM/pool)", (int)i, (int)sub->bitField1.isSyntheticWave);
            if (v->active) voice_stop(i);
            st = stream_for_note(i); if (st) stream_free(st);
            continue;
        }

        freq = calc_freq(sub, res.downsample_shift);
        vol = calc_vol(sub);
        pan = calc_pan(sub);
#if AICA_DEBUG
        if (rc == 2) nStream++; else nNorm++;
        if (vol > vMax) vMax = vol;
        if (vol < vMin) vMin = vol;
#endif

        if (rc == 2) {
            if (v->active) voice_stop(i);
            st = stream_for_note(i);
            /* genuine re-trigger (new sound on this note) -> restart from scratch */
            if (st && (st->key != desc->src_offset || sub->bitField0.needsInit)) { stream_free(st); st = NULL; }
            if (!st) {
                stream_start(i, desc, freq, vol, pan);
            } else if (st->done) {
                /* one-shot already played out: hold the (freed) slot so it is NOT
                   restarted from the beginning while the game still holds the note.
                   Freed on note-off (gate) or on a genuine re-trigger above. */
            } else if (!stream_service(st, vol, pan)) {
                /* played out: stop the channel, keep the slot to block restart,
                   and signal the SOURCE note finished so the engine releases it
                   (the slice's finished bit gets overwritten by the prepass). */
                if (st->channel >= 0) { chan_stop(st->channel); chan_release(st->channel); st->channel = -1; }
                st->done = 1;
                gNotes[i].noteSubEu.bitField0.finished = 1;
            }
            continue;
        }

        retrigger = (!v->active) || (v->sampleKey != key) || sub->bitField0.needsInit;
        if (retrigger) {
            s32 chn;
            if (v->active) voice_stop(i);
            chn = chan_alloc();
            if (chn < 0) { cache_release(entry); AICA_DROP("VOICECHANFULL note=%d key=%X (no free AICA channel)", (int)i, (unsigned)key); continue; }
            v->channel = chn; v->active = 1; v->sampleKey = key; v->entry = entry;
            v->downsampleShift = res.downsample_shift;
            chan_start(chn, &res, freq, vol, pan);
            v->sentFreq = freq; v->sentVol = (u8)vol; v->sentPan = (u8)pan;
#if AICA_OCTAVE_LOG
            /* Flag real ADPCM samples pitched past +1 octave (ratio = resampleRate/32768,
               x2 if hasTwoParts > 2.0). AICA ADPCM pitch-clamps there -> add `key`
               (src_offset) to FORCE_PCM_KEYS. One print per sample per new high; capped. */
            if (!sub->bitField1.isSyntheticWave && res.type == AICA_SM_ADPCM) {
                u32 rate = sub->resampleRate;
                if (sub->bitField1.hasTwoParts) rate <<= 1;
                if (rate > 2u * 32768u) {
                    static u32 oKey[128]; static u32 oMax[128]; static s32 oN = 0;
                    s32 j, hit = -1;
                    for (j = 0; j < oN; j++) if (oKey[j] == key) { hit = j; break; }
                    if (hit < 0 && oN < 128) { hit = oN++; oKey[hit] = key; oMax[hit] = 0; }
                    if (hit >= 0 && rate > oMax[hit]) {
                        oMax[hit] = rate;
                        printf("OCTAVE key=%X ratio=%u.%03u\n", (unsigned)key,
                               (unsigned)(rate >> 15), (unsigned)(((rate & 0x7FFFu) * 1000u) >> 15));
                    }
                }
            }
#endif
#if AICA_DEBUG
            {
                static s32 dbgStarts = 0;
                if (dbgStarts < 40) {
                    printf("AICAstart ch=%d key=%X synth=%d type=%u len=%u loop=%u freq=%u vol=%u pan=%u pvL=%u pvR=%u rr=%u\n",
                           (int)chn, (unsigned)key, sub->bitField1.isSyntheticWave,
                           (unsigned)res.type, (unsigned)res.length, (unsigned)res.loop,
                           (unsigned)freq, (unsigned)vol, (unsigned)pan,
                           (unsigned)sub->panVolLeft, (unsigned)sub->panVolRight, (unsigned)sub->resampleRate);
                    dbgStarts++;
                }
            }
#endif
        } else {
            if (entry) cache_release(entry);
            if (freq != v->sentFreq || (u8)vol != v->sentVol || (u8)pan != v->sentPan) {
                chan_update(v->channel, freq, vol, pan);
                v->sentFreq = freq; v->sentVol = (u8)vol; v->sentPan = (u8)pan;
            }
        }
    }
#if AICA_DEBUG
    if ((++sDbgFrame & 0x3F) == 0 && nEn) {
        printf("AICAdbg en=%d synth=%d norm=%d strm=%d fail=%d vol[%u..%u]\n",
               (int)nEn, (int)nSynth, (int)nNorm, (int)nStream, (int)nFail,
               (unsigned)(vMin == 99999 ? 0 : vMin), (unsigned)vMax);
    }
#endif
}
