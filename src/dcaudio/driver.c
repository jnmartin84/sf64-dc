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
#if USE_16KHZ
#define DC_AUDIO_FREQUENCY (16000)
#else
#define DC_AUDIO_FREQUENCY (26800)
#endif
#endif

#define RING_BUFFER_MAX_BYTES (16384)

// --- Global State for Dreamcast Audio Backend ---
// Handle for the sound stream
static volatile snd_stream_hnd_t shnd = SND_STREAM_INVALID; 
// The main audio buffer
static uint8_t __attribute__((aligned(4096))) cb_buf_internal[2][RING_BUFFER_MAX_BYTES]; 
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

#define CB_LEFT_ADDR   0x10000000
#define CB_RIGHT_ADDR  0x20000000

static bool cb_init(int N, size_t capacity) {
    if (N == 0) {
    // 4 4kb pages
    // map:
    // cb_buf[0][0] to 10000000
    mmu_page_map_static((uintptr_t)0x10000000, ((uintptr_t)cb_buf_internal[0] - 0x80000000), PAGE_SIZE_4K, MMU_ALL_RDWR, true);
    // cb_buf[0][4096] to 10001000
    mmu_page_map_static((uintptr_t)0x10001000, ((uintptr_t)cb_buf_internal[0] - 0x80000000) + 4096, PAGE_SIZE_4K, MMU_ALL_RDWR, true);
    // cb_buf[0][8192] to 10002000
    mmu_page_map_static((uintptr_t)0x10002000, ((uintptr_t)cb_buf_internal[0] - 0x80000000) + (4096*2), PAGE_SIZE_4K, MMU_ALL_RDWR, true);
    // cb_buf[0][12288] to 10003000
    mmu_page_map_static((uintptr_t)0x10003000, ((uintptr_t)cb_buf_internal[0] - 0x80000000) + (4096*3), PAGE_SIZE_4K, MMU_ALL_RDWR, true);

    // cb_buf[0][0] to 10004000
    mmu_page_map_static((uintptr_t)0x10004000, ((uintptr_t)cb_buf_internal[0] - 0x80000000), PAGE_SIZE_4K, MMU_ALL_RDWR, true);
    // cb_buf[0][4096] to 10005000
    mmu_page_map_static((uintptr_t)0x10005000, ((uintptr_t)cb_buf_internal[0] - 0x80000000) + 4096, PAGE_SIZE_4K, MMU_ALL_RDWR, true);
    // cb_buf[0][8192] to 10006000
    mmu_page_map_static((uintptr_t)0x10006000, ((uintptr_t)cb_buf_internal[0] - 0x80000000) + (4096*2), PAGE_SIZE_4K, MMU_ALL_RDWR, true);
    // cb_buf[0][12288] to 10007000
    mmu_page_map_static((uintptr_t)0x10007000, ((uintptr_t)cb_buf_internal[0] - 0x80000000) + (4096*3), PAGE_SIZE_4K, MMU_ALL_RDWR, true);


    // cb_buf[1][0] to 20000000
    mmu_page_map_static((uintptr_t)0x20000000, ((uintptr_t)cb_buf_internal[1] - 0x80000000), PAGE_SIZE_4K, MMU_ALL_RDWR, true);
    // cb_buf[1][4096] to 20001000
    mmu_page_map_static((uintptr_t)0x20001000, ((uintptr_t)cb_buf_internal[1] - 0x80000000) + 4096, PAGE_SIZE_4K, MMU_ALL_RDWR, true);
    // cb_buf[1][8192] to 20002000
    mmu_page_map_static((uintptr_t)0x20002000, ((uintptr_t)cb_buf_internal[1] - 0x80000000) + (4096*2), PAGE_SIZE_4K, MMU_ALL_RDWR, true);
    // cb_buf[1][12288] to 20003000
    mmu_page_map_static((uintptr_t)0x20003000, ((uintptr_t)cb_buf_internal[1] - 0x80000000) + (4096*3), PAGE_SIZE_4K, MMU_ALL_RDWR, true);

    // cb_buf[1][0] to 20004000
    mmu_page_map_static((uintptr_t)0x20004000, ((uintptr_t)cb_buf_internal[1] - 0x80000000), PAGE_SIZE_4K, MMU_ALL_RDWR, true);
    // cb_buf[1][4096] to 20005000
    mmu_page_map_static((uintptr_t)0x20005000, ((uintptr_t)cb_buf_internal[1] - 0x80000000) + 4096, PAGE_SIZE_4K, MMU_ALL_RDWR, true);
    // cb_buf[1][8192] to 20006000
    mmu_page_map_static((uintptr_t)0x20006000, ((uintptr_t)cb_buf_internal[1] - 0x80000000) + (4096*2), PAGE_SIZE_4K, MMU_ALL_RDWR, true);
    // cb_buf[1][12288] to 20007000
    mmu_page_map_static((uintptr_t)0x20007000, ((uintptr_t)cb_buf_internal[1] - 0x80000000) + (4096*3), PAGE_SIZE_4K, MMU_ALL_RDWR, true);
    }
    // round capacity up to power of two
    r[N]->cap = 1u << (32 - __builtin_clz(capacity - 1));

    if (N == 0) r[N]->buf = 0x10000000;
    else r[N]->buf = 0x20000000;

    if (!r[N]->buf)
        return false;
    r[N]->head = 0;
    r[N]->tail = 0;
    return true;
}
void n64_memcpy(void* dst, const void* src, size_t size);

static size_t cb_write_data(int N, const void *src, size_t n) {
    __builtin_prefetch(src);
    uint32_t head = r[N]->head;
    uint32_t tail = r[N]->tail;
    uint32_t free = r[N]->cap - (head - tail);
    if (n > free)
        return 0;
    uint32_t idx = head & (r[N]->cap - 1);
    n64_memcpy(r[N]->buf + idx, src, n);
    r[N]->head = head + n;
    return n;
}

static size_t cb_read_data(int N, void *dst, size_t n) {
    uint32_t tail = r[N]->tail;
    uint32_t idx = tail & (r[N]->cap - 1);
    __builtin_prefetch(r[N]->buf + idx);
    uint32_t head = r[N]->head;
    uint32_t avail = head - tail;
    if (n > avail) return 0;
    n64_memcpy(dst, r[N]->buf + idx, n);
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
    if (snd_stream_init()) {
        printf("AICA INIT FAILURE!\n");
        return false;
    }

    // without increasing scheduler frequency, things don't work nicely
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
#if USE_16KHZ
    shnd = snd_stream_alloc(NULL, 2048);
#else
    shnd = snd_stream_alloc(NULL, 4096);
#endif
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
