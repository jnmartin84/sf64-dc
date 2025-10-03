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
// Sample rate for the AICA (32kHz)
#define DC_AUDIO_FREQUENCY (26800) 
#define RING_BUFFER_MAX_BYTES (16384*2)

// --- Global State for Dreamcast Audio Backend ---
// Handle for the sound stream
static volatile snd_stream_hnd_t shnd = SND_STREAM_INVALID; 
// The main audio buffer
static uint8_t cb_buf_internal[2][RING_BUFFER_MAX_BYTES/2] __attribute__((aligned(32))); 
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

static size_t cb_write_data(int N, const void *src, size_t n) {
    uint32_t head = r[N]->head;
    uint32_t tail = r[N]->tail;
    uint32_t free = r[N]->cap - (head - tail);
    if (n > free)
        return 0;
    uint32_t idx = head & (r[N]->cap - 1);
    uint32_t first = MIN(n, r[N]->cap - idx);
//    if (first)
        n64_memcpy(r[N]->buf + idx, src, first);
//    if (n-first) {
//        printf("hit stupid wrapround write\n");
        n64_memcpy(r[N]->buf, (uint8_t*)src + first, n - first);
//    }
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
//    if (first) {
//        spu_memload_dma((uintptr_t)dst & 0x000FFFFF, r[N]->buf + idx, first);// {
        n64_memcpy(dst, r[N]->buf + idx, first);
//    }
//    if (n - first) {
//                printf("hit stupid wrapround read\n");

//        spu_memload_dma(((uintptr_t)dst & 0x000FFFFF) + first, r[N]->buf, n - first);// {
//        n64_memcpy((uint8_t*)dst + first, r[N]->buf, n - first);
//    }
    r[N]->tail = tail + n;
    return n;
}

// Calculates and returns the number of bytes currently in the ring buffer
static size_t cb_get_used(int N) {
    // Atomically load both head and tail to get a consistent snapshot.
    // The order of loads might matter in some weak memory models,
    // but for head-tail diff, generally not.
    uint32_t head = r[N]->head;
    uint32_t tail = r[N]->tail;
    
    // The number of used bytes is simply the difference between head and tail.
    // This works because head and tail are continuously incrementing indices,
    // and effectively handle wrap-around due to the (head - tail) arithmetic.
    return head - tail;
}

// --- KOS Stream Audio Callback (Consumer): Called by KOS when the AICA needs more data ---
#define NUM_BUFFER_BLOCKS (2)
#define TEMP_BUF_SIZE ((8192/* *2 */ /*  / 2 */) * NUM_BUFFER_BLOCKS)
static uint8_t __attribute__((aligned(32))) temp_buf[2][TEMP_BUF_SIZE];
static unsigned int temp_buf_sel[2] = {0};

void mute_stream(void) {
    snd_stream_volume(shnd, 0); // Set maximum volume
}

void unmute_stream(void) {
    snd_stream_volume(shnd, 160); // Set maximum volume
}

static size_t audio_cb(UNUSED snd_stream_hnd_t hnd, uintptr_t l, uintptr_t r, size_t req) {
        // Correct atomic read

    cb_read_data(0, (void*)l , req/2);
    cb_read_data(1, (void*)r , req/2);
    return req;
}

#if 0
void *audio_callback(UNUSED snd_stream_hnd_t hnd, int samples_requested_bytes, int *samples_returned_bytes) {
    size_t samples_requested = samples_requested_bytes / 4;
    size_t samples_avail_bytes = cb_read_data(temp_buf + ((4096) * temp_buf_sel) , samples_requested_bytes);
    
    *samples_returned_bytes = samples_requested_bytes;
    size_t samples_returned = samples_avail_bytes / 4;
    
    /*@Note: This is more correct, fill with empty audio */
    if (samples_avail_bytes < (unsigned)samples_requested_bytes) {
        memset(temp_buf + ((4096) * temp_buf_sel) + samples_avail_bytes, 0, (samples_requested_bytes - samples_avail_bytes));
    }
    
    temp_buf_sel += 1;
    if (temp_buf_sel >= NUM_BUFFER_BLOCKS) {
        temp_buf_sel = 0;
    }
    
    return (void*)(temp_buf + ((4096) * temp_buf_sel));
}
#endif

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
    sq_clr(temp_buf, sizeof(temp_buf));
    if (!cb_init(0,8960)) { //RING_BUFFER_MAX_BYTES/2)) {
        printf("CB INIT FAILURE!\n");
        return false;
    }
    if (!cb_init(1,8960)) { //RING_BUFFER_MAX_BYTES/2)) {
        printf("CB INIT FAILURE!\n");
        return false;
    }

    printf("Dreamcast Audio: Initialized. Ring buffer size: %u bytes.\n",
           (unsigned int)RING_BUFFER_MAX_BYTES);
#if 1
    // Allocate the sound stream with KOS
    shnd = snd_stream_alloc(NULL, 4096);
    if (shnd == SND_STREAM_INVALID) {
        printf("SND: Stream allocation failure!\n");
        snd_stream_destroy(shnd);
        return false;
    }
    snd_stream_set_callback_direct(shnd, audio_cb);

// Set maximum volume
    snd_stream_volume(shnd, 160); 

    printf("Sound init complete!\n");
#endif
    return true;
}

static int audio_dc_buffered(void) {
    return 1088;
}

static int audio_dc_get_desired_buffered(void) {
    return 1100;
}

static void audio_dc_play(uint8_t *bufL, uint8_t *bufR, size_t len) {
//    size_t ring_data_available = cb_get_used();
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
