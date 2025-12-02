/*
 * File: dc_audio_kos.c
 * Project: sm64-port
 * Author: Hayden Kowalchuk (hayden@hkowsoftware.com)
 * -----
 * Copyright (c) 2025 Hayden Kowalchuk
 */

/*
 * Modifications by jnmartin84
 */

#include <kos.h>
#include <dc/sound/stream.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdatomic.h>

#include "audio_dc.h"
#include "macros.h"

// --- Configuration ---
// Stereo
#define DC_AUDIO_CHANNELS (2) 
#define DC_STEREO_AUDIO ( DC_AUDIO_CHANNELS == 2)

#if USE_32KHZ
// Sample rate for the AICA (32kHz)
#define DC_AUDIO_FREQUENCY (32000) 
#else
#define DC_AUDIO_FREQUENCY (26800)
#endif

#define RING_BUFFER_MAX_BYTES (16384)

// --- Global State for Dreamcast Audio Backend ---
// Handle for the sound stream
static volatile snd_stream_hnd_t shnd = SND_STREAM_INVALID; 
// The main audio buffer
static uint8_t __attribute__((aligned(32))) cb_buf_internal[2][RING_BUFFER_MAX_BYTES]; 
static void *const cb_buf[2] = {cb_buf_internal[0],cb_buf_internal[1]};
static bool audio_started = false;

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))

typedef struct {
    uint8_t *buf;
    uint32_t cap; // power-of-two
    uint32_t head; // next write pos
    uint32_t tail; // next read pos
} ring_t;

static ring_t cb_ring[2];
static ring_t *r[2] = {&cb_ring[0],&cb_ring[1]};
file_t streamout;
extern int stream_dump ;
extern int stream_no ;
int last_stream_dump = 0;

static bool cb_init(int N, size_t capacity) {
    // round capacity up to power of two
    r[N]->cap = 1u << (32 - __builtin_clz(capacity - 1));
    r[N]->buf = cb_buf[N];
    if (!r[N]->buf)
        return false;
    r[N]->head = 0;
    r[N]->tail = 0;
    return true;
}
void n64_memcpy(void* dst, const void* src, size_t size);

void *cb_next_left(void) {
    uint32_t head = r[0]->head;
    uint32_t tail = r[0]->tail;
    uint32_t free = r[0]->cap - (head - tail);
    uint32_t idx = head & (r[0]->cap - 1);
    r[0]->head = head + (448*2);
    return (void*)r[0]->buf + idx;
}

void *cb_next_right(void) {
    uint32_t head = r[1]->head;
    uint32_t tail = r[1]->tail;
    uint32_t free = r[1]->cap - (head - tail);
    uint32_t idx = head & (r[1]->cap - 1);
    r[1]->head = head + (448*2);

    return (void*)r[1]->buf + idx;
}


static size_t cb_write_data(int N, const void *src, size_t n) {
    uint32_t head = r[N]->head;
    uint32_t tail = r[N]->tail;
    uint32_t free = r[N]->cap - (head - tail);
    if (n > free)
        return 0;
    uint32_t idx = head & (r[N]->cap - 1);
    uint32_t first = MIN(n, r[N]->cap - idx);
    if (first)
        n64_memcpy(r[N]->buf + idx, src, first);
    if (n-first)
        n64_memcpy(r[N]->buf, (uint8_t*)src + first, n - first);
    r[N]->head = head + n;
    return n;
}

static size_t cb_read_data(int N, void *dst, size_t n) {
    uint32_t head = r[N]->head;
    uint32_t tail = r[N]->tail;//atomic_load(&r->tail);
    uint32_t avail = head - tail;
    if (n > avail) return 0;
    uint32_t idx = tail & (r[N]->cap - 1);
    __builtin_prefetch(r[N]->buf + idx);
    uint32_t first = MIN(n, r[N]->cap - idx);
    n64_memcpy(dst, r[N]->buf + idx, first);
    r[N]->tail = tail + n;
    return n;
}

// --- KOS Stream Audio Callback (Consumer): Called by KOS when the AICA needs more data ---
#define NUM_BUFFER_BLOCKS (2)

void mute_stream(void) {
    snd_stream_volume(shnd, 0); // Set maximum volume
}

void unmute_stream(void) {
    snd_stream_volume(shnd, 160); // Set maximum volume
}

static size_t audio_cb(UNUSED snd_stream_hnd_t hnd, uintptr_t l, uintptr_t r, size_t req) {
    cb_read_data(0, (void*)l , req/2);
    cb_read_data(1, (void*)r , req/2);
    return req;
}

static bool audio_dc_init(void) {
#if 1
    if (snd_stream_init()) {
        printf("AICA INIT FAILURE!\n");
        return false;
    }
#endif
    thd_set_hz(300);

    // --- Initial Pre-fill of Ring Buffer with Silence ---
    sq_clr(cb_buf_internal, sizeof(cb_buf_internal));

    if (!cb_init(0,RING_BUFFER_MAX_BYTES)) {
        printf("CB INIT FAILURE!\n");
        return false;
    }
    if (!cb_init(1,RING_BUFFER_MAX_BYTES)) {
        printf("CB INIT FAILURE!\n");
        return false;
    }

    printf("Dreamcast Audio: Initialized. %d Hz, ring buffer size: %u bytes per channel.\n",
           DC_AUDIO_FREQUENCY, (unsigned int)RING_BUFFER_MAX_BYTES);

    // Allocate the sound stream with KOS
#if USE_32KHZ
    shnd = snd_stream_alloc(NULL, 8192);
#else
    shnd = snd_stream_alloc(NULL, 4096);
#endif
    if (shnd == SND_STREAM_INVALID) {
        printf("SND: Stream allocation failure!\n");
        snd_stream_destroy(shnd);
        return false;
    }
    snd_stream_set_callback_direct(shnd, audio_cb);

    // Set maximum volume
    snd_stream_volume(shnd, 160); 

    printf("Sound init complete!\n");

    return true;
}

static int audio_dc_buffered(void) {
    return 1088;
}

static int audio_dc_get_desired_buffered(void) {
    return 1100;
}

static void audio_dc_play(uint8_t *bufL, uint8_t *bufR, size_t len) {
    size_t writtenL = cb_write_data(0, bufL, len/2);
    size_t writtenR = cb_write_data(1, bufR, len/2);

    if ((!audio_started)) {
        audio_started = true;
        snd_stream_start(shnd, DC_AUDIO_FREQUENCY, DC_STEREO_AUDIO);
    }
    
    if (audio_started) {
        snd_stream_poll(shnd);
    }
}

struct AudioAPI audio_dc = {
    audio_dc_init,
    audio_dc_buffered,
    audio_dc_get_desired_buffered,
    audio_dc_play
};
