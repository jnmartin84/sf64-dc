// #include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#undef bool

#include <PR/ultratypes.h>
#ifdef __GNUC__
#define UNUSED __attribute__((unused))
#else
#define UNUSED
#endif

#include "mixer.h"

#include "sh4zam.h"

#define MEM_BARRIER() asm volatile("" : : : "memory");
#define MEM_BARRIER_PREF(ptr) asm volatile("pref @%0" : : "r"((ptr)) : "memory")

void n64_memcpy(void* dst, const void* src, size_t size) {
    if (!size)
        return;
    uint8_t* bdst = (uint8_t*) dst;
    uint8_t* bsrc = (uint8_t*) src;
    uint16_t* sdst = (uint16_t*) dst;
    uint16_t* ssrc = (uint16_t*) src;
    uint32_t* wdst = (uint32_t*) dst;
    uint32_t* wsrc = (uint32_t*) src;

    int size_to_copy = size;
    int words_to_copy = size_to_copy >> 2;
    int shorts_to_copy = size_to_copy >> 1;
    int bytes_to_copy = size_to_copy - (words_to_copy << 2);
    int sbytes_to_copy = size_to_copy - (shorts_to_copy << 1);

    SHZ_PREFETCH(bsrc);
    if ((!(((uintptr_t) bdst | (uintptr_t) bsrc) & 3))) {
        while (words_to_copy--) {
            if ((words_to_copy & 3) == 0) {
                SHZ_PREFETCH(bsrc + 16);
            }
            *wdst++ = *wsrc++;
        }

        bdst = (uint8_t*) wdst;
        bsrc = (uint8_t*) wsrc;

        switch (bytes_to_copy) {
            case 0:
                return;
            case 1:
                goto n64copy1;
            case 2:
                goto n64copy2;
            case 3:
                goto n64copy3;
            default:
                return;
        }
    } else if ((!(((uintptr_t) sdst | (uintptr_t) ssrc) & 1))) {
        while (shorts_to_copy--) {
            *sdst++ = *ssrc++;
        }

        bdst = (uint8_t*) sdst;
        bsrc = (uint8_t*) ssrc;

        if (sbytes_to_copy) {
            goto n64copy1;
        }

        return;
    } else {
        while (words_to_copy-- > 0) {
            uint8_t b1, b2, b3, b4;
            b1 = *bsrc++;
            b2 = *bsrc++;
            b3 = *bsrc++;
            b4 = *bsrc++;

            MEM_BARRIER();

            *bdst++ = b1;
            *bdst++ = b2;
            *bdst++ = b3;
            *bdst++ = b4;
        }

        switch (bytes_to_copy) {
            case 0:
                return;
            case 1:
                goto n64copy1;
            case 2:
                goto n64copy2;
            case 3:
                goto n64copy3;
            default:
                return;
        }
    }

n64copy3:
    *bdst++ = *bsrc++;
n64copy2:
    *bdst++ = *bsrc++;
n64copy1:
    *bdst = *bsrc;
    return;
}

#define recip8192 0.00012207f
#define recip2048 0.00048828f
#define recip2560 0.00039062f

#define ROUND_UP_64(v) (((v) + 63) & ~63)
#define ROUND_UP_32(v) (((v) + 31) & ~31)
#define ROUND_UP_16(v) (((v) + 15) & ~15)
#define ROUND_UP_8(v) (((v) + 7) & ~7)

#define DMEM_BUF_SIZE 4096
#define BUF_U8(a) (rspa.buf.as_u8 + (a))
#define BUF_S16(a) (rspa.buf.as_s16 + (a) / sizeof(int16_t))

static struct __attribute__((aligned(32))) {
    union __attribute__((aligned(32))) {
        int16_t __attribute__((aligned(32))) as_s16[DMEM_BUF_SIZE / sizeof(int16_t)];
        uint8_t __attribute__((aligned(32))) as_u8[DMEM_BUF_SIZE];
    } buf;

    float __attribute__((aligned(32))) adpcm_table[8][2][8];
    uint16_t vol[2];
    uint16_t rate[2];

    int16_t __attribute__((aligned(32))) adpcm_loop_state[16];

    u16* loaded_buffer;
    uint16_t in;
    uint16_t out;
    uint16_t nbytes;

    uint16_t rate_wet;
    uint16_t vol_wet;
} rspa = { 0 };

static float __attribute__((aligned(32))) resample_table[64][4] = {
    {
        (f32) 3129,
        (f32) 26285,
        (f32) 3398,
        (f32) -33,
    },
    {
        (f32) 2873,
        (f32) 26262,
        (f32) 3679,
        (f32) -40,
    },
    {
        (f32) 2628,
        (f32) 26217,
        (f32) 3971,
        (f32) -48,
    },
    {
        (f32) 2394,
        (f32) 26150,
        (f32) 4276,
        (f32) -56,
    },
    {
        (f32) 2173,
        (f32) 26061,
        (f32) 4592,
        (f32) -65,
    },
    {
        (f32) 1963,
        (f32) 25950,
        (f32) 4920,
        (f32) -74,
    },
    {
        (f32) 1764,
        (f32) 25817,
        (f32) 5260,
        (f32) -84,
    },
    {
        (f32) 1576,
        (f32) 25663,
        (f32) 5611,
        (f32) -95,
    },
    {
        (f32) 1399,
        (f32) 25487,
        (f32) 5974,
        (f32) -106,
    },
    {
        (f32) 1233,
        (f32) 25291,
        (f32) 6347,
        (f32) -118,
    },
    {
        (f32) 1077,
        (f32) 25075,
        (f32) 6732,
        (f32) -130,
    },
    {
        (f32) 932,
        (f32) 24838,
        (f32) 7127,
        (f32) -143,
    },
    {
        (f32) 796,
        (f32) 24583,
        (f32) 7532,
        (f32) -156,
    },
    {
        (f32) 671,
        (f32) 24309,
        (f32) 7947,
        (f32) -170,
    },
    {
        (f32) 554,
        (f32) 24016,
        (f32) 8371,
        (f32) -184,
    },
    {
        (f32) 446,
        (f32) 23706,
        (f32) 8804,
        (f32) -198,
    },
    {
        (f32) 347,
        (f32) 23379,
        (f32) 9246,
        (f32) -212,
    },
    {
        (f32) 257,
        (f32) 23036,
        (f32) 9696,
        (f32) -226,
    },
    {
        (f32) 174,
        (f32) 22678,
        (f32) 10153,
        (f32) -240,
    },
    {
        (f32) 99,
        (f32) 22304,
        (f32) 10618,
        (f32) -254,
    },
    {
        (f32) 31,
        (f32) 21917,
        (f32) 11088,
        (f32) -268,
    },
    {
        (f32) -30,
        (f32) 21517,
        (f32) 11564,
        (f32) -280,
    },
    {
        (f32) -84,
        (f32) 21104,
        (f32) 12045,
        (f32) -293,
    },
    {
        (f32) -132,
        (f32) 20679,
        (f32) 12531,
        (f32) -304,
    },
    {
        (f32) -173,
        (f32) 20244,
        (f32) 13020,
        (f32) -314,
    },
    {
        (f32) -210,
        (f32) 19799,
        (f32) 13512,
        (f32) -323,
    },
    {
        (f32) -241,
        (f32) 19345,
        (f32) 14006,
        (f32) -330,
    },
    {
        (f32) -267,
        (f32) 18882,
        (f32) 14501,
        (f32) -336,
    },
    {
        (f32) -289,
        (f32) 18413,
        (f32) 14997,
        (f32) -340,
    },
    {
        (f32) -306,
        (f32) 17937,
        (f32) 15493,
        (f32) -341,
    },
    {
        (f32) -320,
        (f32) 17456,
        (f32) 15988,
        (f32) -340,
    },
    {
        (f32) -330,
        (f32) 16970,
        (f32) 16480,
        (f32) -337,
    },
    {
        (f32) -337,
        (f32) 16480,
        (f32) 16970,
        (f32) -330,
    },
    {
        (f32) -340,
        (f32) 15988,
        (f32) 17456,
        (f32) -320,
    },
    {
        (f32) -341,
        (f32) 15493,
        (f32) 17937,
        (f32) -306,
    },
    {
        (f32) -340,
        (f32) 14997,
        (f32) 18413,
        (f32) -289,
    },
    {
        (f32) -336,
        (f32) 14501,
        (f32) 18882,
        (f32) -267,
    },
    {
        (f32) -330,
        (f32) 14006,
        (f32) 19345,
        (f32) -241,
    },
    {
        (f32) -323,
        (f32) 13512,
        (f32) 19799,
        (f32) -210,
    },
    {
        (f32) -314,
        (f32) 13020,
        (f32) 20244,
        (f32) -173,
    },
    {
        (f32) -304,
        (f32) 12531,
        (f32) 20679,
        (f32) -132,
    },
    {
        (f32) -293,
        (f32) 12045,
        (f32) 21104,
        (f32) -84,
    },
    {
        (f32) -280,
        (f32) 11564,
        (f32) 21517,
        (f32) -30,
    },
    {
        (f32) -268,
        (f32) 11088,
        (f32) 21917,
        (f32) 31,
    },
    {
        (f32) -254,
        (f32) 10618,
        (f32) 22304,
        (f32) 99,
    },
    {
        (f32) -240,
        (f32) 10153,
        (f32) 22678,
        (f32) 174,
    },
    {
        (f32) -226,
        (f32) 9696,
        (f32) 23036,
        (f32) 257,
    },
    {
        (f32) -212,
        (f32) 9246,
        (f32) 23379,
        (f32) 347,
    },
    {
        (f32) -198,
        (f32) 8804,
        (f32) 23706,
        (f32) 446,
    },
    {
        (f32) -184,
        (f32) 8371,
        (f32) 24016,
        (f32) 554,
    },
    {
        (f32) -170,
        (f32) 7947,
        (f32) 24309,
        (f32) 671,
    },
    {
        (f32) -156,
        (f32) 7532,
        (f32) 24583,
        (f32) 796,
    },
    {
        (f32) -143,
        (f32) 7127,
        (f32) 24838,
        (f32) 932,
    },
    {
        (f32) -130,
        (f32) 6732,
        (f32) 25075,
        (f32) 1077,
    },
    {
        (f32) -118,
        (f32) 6347,
        (f32) 25291,
        (f32) 1233,
    },
    {
        (f32) -106,
        (f32) 5974,
        (f32) 25487,
        (f32) 1399,
    },
    {
        (f32) -95,
        (f32) 5611,
        (f32) 25663,
        (f32) 1576,
    },
    {
        (f32) -84,
        (f32) 5260,
        (f32) 25817,
        (f32) 1764,
    },
    {
        (f32) -74,
        (f32) 4920,
        (f32) 25950,
        (f32) 1963,
    },
    {
        (f32) -65,
        (f32) 4592,
        (f32) 26061,
        (f32) 2173,
    },
    {
        (f32) -56,
        (f32) 4276,
        (f32) 26150,
        (f32) 2394,
    },
    {
        (f32) -48,
        (f32) 3971,
        (f32) 26217,
        (f32) 2628,
    },
    {
        (f32) -40,
        (f32) 3679,
        (f32) 26262,
        (f32) 2873,
    },
    {
        (f32) -33,
        (f32) 3398,
        (f32) 26285,
        (f32) 3129,
    },
};

static inline int16_t clamp16(int32_t v) {
    if (v < -0x8000) {
        return -0x8000;
    } else if (v > 0x7fff) {
        return 0x7fff;
    }
    return (int16_t) v;
}

void aClearBufferImpl(uint16_t addr, int nbytes) {
    memset(BUF_U8(addr), 0, ROUND_UP_16(nbytes));
}

void aLoadBufferPointerImpl(const void* source_addr) {
    rspa.loaded_buffer = (u16*) source_addr;
}

void aLoadBufferImpl(const void* source_addr, uint16_t dest_addr, uint16_t nbytes) {
    rspa.loaded_buffer = (u16*) source_addr;
}

void aSaveBufferImpl(uint16_t source_addr, int16_t* dest_addr, uint16_t nbytes) {
    size_t rnb = ROUND_UP_16(nbytes);
    n64_memcpy((void*) ((uintptr_t) dest_addr), (const void*) BUF_S16(source_addr), rnb);
}

static short __attribute__((aligned(32))) adpcm_tmp[8];

void aLoadADPCMImpl(int num_entries_times_16, const int16_t* book_source_addr) {
    float* aptr = (float*) rspa.adpcm_table;
    SHZ_PREFETCH(book_source_addr);

    for (size_t i = 0; i < num_entries_times_16 / 2; i += 8) {
        SHZ_PREFETCH(&aptr[i]);

        adpcm_tmp[0] = (short) __builtin_bswap16(/* (uint16_t) */book_source_addr[i + 0]);
        adpcm_tmp[1] = (short) __builtin_bswap16(/* (uint16_t) */book_source_addr[i + 1]);
        adpcm_tmp[2] = (short) __builtin_bswap16(/* (uint16_t) */book_source_addr[i + 2]);
        adpcm_tmp[3] = (short) __builtin_bswap16(/* (uint16_t) */book_source_addr[i + 3]);
        adpcm_tmp[4] = (short) __builtin_bswap16(/* (uint16_t) */book_source_addr[i + 4]);
        adpcm_tmp[5] = (short) __builtin_bswap16(/* (uint16_t) */book_source_addr[i + 5]);
        adpcm_tmp[6] = (short) __builtin_bswap16(/* (uint16_t) */book_source_addr[i + 6]);
        adpcm_tmp[7] = (short) __builtin_bswap16(/* (uint16_t) */book_source_addr[i + 7]);

        MEM_BARRIER_PREF(&book_source_addr[i + 8]);

        aptr[i + 0] = /* recip2048 * */ (f32) (s32) adpcm_tmp[0];
        aptr[i + 1] = /* recip2048 * */ (f32) (s32) adpcm_tmp[1];
        aptr[i + 2] = /* recip2048 * */ (f32) (s32) adpcm_tmp[2];
        aptr[i + 3] = /* recip2048 * */ (f32) (s32) adpcm_tmp[3];
        aptr[i + 4] = /* recip2048 * */ (f32) (s32) adpcm_tmp[4];
        aptr[i + 5] = /* recip2048 * */ (f32) (s32) adpcm_tmp[5];
        aptr[i + 6] = /* recip2048 * */ (f32) (s32) adpcm_tmp[6];
        aptr[i + 7] = /* recip2048 * */ (f32) (s32) adpcm_tmp[7];
    }
}

void aSetBufferImpl(UNUSED uint8_t flags, uint16_t in, uint16_t out, uint16_t nbytes) {
    rspa.in = in;
    rspa.out = out;
    rspa.nbytes = nbytes;
}

void aDMEMMoveImpl(uint16_t in_addr, uint16_t out_addr, int nbytes) {
    memmove(BUF_U8(out_addr), BUF_U8(in_addr), ROUND_UP_16(nbytes));
}

void aDMEMCopyImpl(uint16_t in_addr, uint16_t out_addr, int nbytes) {
    n64_memcpy(BUF_U8(out_addr), BUF_U8(in_addr), ROUND_UP_16(nbytes));
}

void aSetLoopImpl(ADPCM_STATE* adpcm_loop_state) {
    int16_t *alpp = (int16_t *)adpcm_loop_state;
    for (size_t i = 0; i < 16; i++) {
        rspa.adpcm_loop_state[i] = (int16_t) __builtin_bswap16(alpp[i]);
    }
}

inline static void shz_xmtrx_load_3x4_rows(const shz_vec4_t* r1, const shz_vec4_t* r2, const shz_vec4_t* r3) {
    asm volatile(R"(
        pref    @%0
        frchg

        fldi0   fr12
        fldi0   fr13
        fldi0   fr14
        fldi1   fr15

        pref    @%1
        fmov.s  @%0+, fr0
        fmov.s  @%0+, fr1
        fmov.s  @%0+, fr2
        fmov.s  @%0,  fr3

        pref    @%2
        fmov.s  @%1+, fr4
        fmov.s  @%1+, fr5
        fmov.s  @%1+, fr6
        fmov.s  @%1,  fr7

        fmov.s  @%2+, fr8
        fmov.s  @%2+, fr9
        fmov.s  @%2+, fr10
        fmov.s  @%2,  fr11

        frchg
    )"
                 : "+&r"(r1), "+&r"(r2), "+&r"(r3));
}

SHZ_FORCE_INLINE void shz_copy_16_shorts(void* restrict dst, const void* restrict src) {
    asm volatile(R"(
        mov.w   @%[s]+, r0
        mov.w   @%[s]+, r1
        mov.w   @%[s]+, r2
        mov.w   @%[s]+, r3
        mov.w   @%[s]+, r4
        mov.w   @%[s]+, r5
        mov.w   @%[s]+, r6
        mov.w   @%[s]+, r7
        add     #16, %[d]
        mov.w   r7, @-%[d]
        mov.w   r6, @-%[d]
        mov.w   r5, @-%[d]
        mov.w   r4, @-%[d]
        mov.w   r3, @-%[d]
        mov.w   r2, @-%[d]
        mov.w   r1, @-%[d]
        mov.w   r0, @-%[d]
        mov.w   @%[s]+, r0
        mov.w   @%[s]+, r1
        mov.w   @%[s]+, r2
        mov.w   @%[s]+, r3
        mov.w   @%[s]+, r4
        mov.w   @%[s]+, r5
        mov.w   @%[s]+, r6
        mov.w   @%[s]+, r7
        add     #32, %[d]
        mov.w   r7, @-%[d]
        mov.w   r6, @-%[d]
        mov.w   r5, @-%[d]
        mov.w   r4, @-%[d]
        mov.w   r3, @-%[d]
        mov.w   r2, @-%[d]
        mov.w   r1, @-%[d]
        mov.w   r0, @-%[d]
    )"
                 : [d] "+r"(dst), [s] "+r"(src)
                 :
                 : "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "memory");
}

SHZ_FORCE_INLINE void shz_zero_16_shorts(void* dst) {
    asm volatile(R"(
        xor     r0, r0
        add     #32 %0
        mov.w   r0, @-%0
        mov.w   r0, @-%0
        mov.w   r0, @-%0
        mov.w   r0, @-%0
        mov.w   r0, @-%0
        mov.w   r0, @-%0
        mov.w   r0, @-%0
        mov.w   r0, @-%0
        mov.w   r0, @-%0
        mov.w   r0, @-%0
        mov.w   r0, @-%0
        mov.w   r0, @-%0
        mov.w   r0, @-%0
        mov.w   r0, @-%0
        mov.w   r0, @-%0
        mov.w   r0, @-%0
    )"
                 :
                 : "r"(dst)
                 : "r0", "memory");
}

static inline s16 clamp16f(float v) {
    s32 sv = (s32) v;
    if (sv < -32768) {
        return -32768;
    } else if (sv > 32767) {
        return 32767;
    }
    return (s16) sv;
}

static inline float shift_to_float_multiplier(uint8_t shift) {
    const static float
        __attribute__((aligned(32))) shift_to_float[16] = { 1.0f,    2.0f,    4.0f,     8.0f,    16.0f,   32.0f,
                                                            64.0f,   128.0f,  256.0f,   512.0f,  1024.0f, 2048.0f,
                                                            4096.0f, 8192.0f, 16364.0f, 32768.0f };
    return shift_to_float[shift];
}

static const float __attribute__((aligned(32))) nyblls_as_floats[256][2] = {
    { 0.0f, 0.0f },   { 0.0f, 1.0f },   { 0.0f, 2.0f },   { 0.0f, 3.0f },   { 0.0f, 4.0f },   { 0.0f, 5.0f },
    { 0.0f, 6.0f },   { 0.0f, 7.0f },   { 0.0f, -8.0f },  { 0.0f, -7.0f },  { 0.0f, -6.0f },  { 0.0f, -5.0f },
    { 0.0f, -4.0f },  { 0.0f, -3.0f },  { 0.0f, -2.0f },  { 0.0f, -1.0f },  { 1.0f, 0.0f },   { 1.0f, 1.0f },
    { 1.0f, 2.0f },   { 1.0f, 3.0f },   { 1.0f, 4.0f },   { 1.0f, 5.0f },   { 1.0f, 6.0f },   { 1.0f, 7.0f },
    { 1.0f, -8.0f },  { 1.0f, -7.0f },  { 1.0f, -6.0f },  { 1.0f, -5.0f },  { 1.0f, -4.0f },  { 1.0f, -3.0f },
    { 1.0f, -2.0f },  { 1.0f, -1.0f },  { 2.0f, 0.0f },   { 2.0f, 1.0f },   { 2.0f, 2.0f },   { 2.0f, 3.0f },
    { 2.0f, 4.0f },   { 2.0f, 5.0f },   { 2.0f, 6.0f },   { 2.0f, 7.0f },   { 2.0f, -8.0f },  { 2.0f, -7.0f },
    { 2.0f, -6.0f },  { 2.0f, -5.0f },  { 2.0f, -4.0f },  { 2.0f, -3.0f },  { 2.0f, -2.0f },  { 2.0f, -1.0f },
    { 3.0f, 0.0f },   { 3.0f, 1.0f },   { 3.0f, 2.0f },   { 3.0f, 3.0f },   { 3.0f, 4.0f },   { 3.0f, 5.0f },
    { 3.0f, 6.0f },   { 3.0f, 7.0f },   { 3.0f, -8.0f },  { 3.0f, -7.0f },  { 3.0f, -6.0f },  { 3.0f, -5.0f },
    { 3.0f, -4.0f },  { 3.0f, -3.0f },  { 3.0f, -2.0f },  { 3.0f, -1.0f },  { 4.0f, 0.0f },   { 4.0f, 1.0f },
    { 4.0f, 2.0f },   { 4.0f, 3.0f },   { 4.0f, 4.0f },   { 4.0f, 5.0f },   { 4.0f, 6.0f },   { 4.0f, 7.0f },
    { 4.0f, -8.0f },  { 4.0f, -7.0f },  { 4.0f, -6.0f },  { 4.0f, -5.0f },  { 4.0f, -4.0f },  { 4.0f, -3.0f },
    { 4.0f, -2.0f },  { 4.0f, -1.0f },  { 5.0f, 0.0f },   { 5.0f, 1.0f },   { 5.0f, 2.0f },   { 5.0f, 3.0f },
    { 5.0f, 4.0f },   { 5.0f, 5.0f },   { 5.0f, 6.0f },   { 5.0f, 7.0f },   { 5.0f, -8.0f },  { 5.0f, -7.0f },
    { 5.0f, -6.0f },  { 5.0f, -5.0f },  { 5.0f, -4.0f },  { 5.0f, -3.0f },  { 5.0f, -2.0f },  { 5.0f, -1.0f },
    { 6.0f, 0.0f },   { 6.0f, 1.0f },   { 6.0f, 2.0f },   { 6.0f, 3.0f },   { 6.0f, 4.0f },   { 6.0f, 5.0f },
    { 6.0f, 6.0f },   { 6.0f, 7.0f },   { 6.0f, -8.0f },  { 6.0f, -7.0f },  { 6.0f, -6.0f },  { 6.0f, -5.0f },
    { 6.0f, -4.0f },  { 6.0f, -3.0f },  { 6.0f, -2.0f },  { 6.0f, -1.0f },  { 7.0f, 0.0f },   { 7.0f, 1.0f },
    { 7.0f, 2.0f },   { 7.0f, 3.0f },   { 7.0f, 4.0f },   { 7.0f, 5.0f },   { 7.0f, 6.0f },   { 7.0f, 7.0f },
    { 7.0f, -8.0f },  { 7.0f, -7.0f },  { 7.0f, -6.0f },  { 7.0f, -5.0f },  { 7.0f, -4.0f },  { 7.0f, -3.0f },
    { 7.0f, -2.0f },  { 7.0f, -1.0f },  { -8.0f, 0.0f },  { -8.0f, 1.0f },  { -8.0f, 2.0f },  { -8.0f, 3.0f },
    { -8.0f, 4.0f },  { -8.0f, 5.0f },  { -8.0f, 6.0f },  { -8.0f, 7.0f },  { -8.0f, -8.0f }, { -8.0f, -7.0f },
    { -8.0f, -6.0f }, { -8.0f, -5.0f }, { -8.0f, -4.0f }, { -8.0f, -3.0f }, { -8.0f, -2.0f }, { -8.0f, -1.0f },
    { -7.0f, 0.0f },  { -7.0f, 1.0f },  { -7.0f, 2.0f },  { -7.0f, 3.0f },  { -7.0f, 4.0f },  { -7.0f, 5.0f },
    { -7.0f, 6.0f },  { -7.0f, 7.0f },  { -7.0f, -8.0f }, { -7.0f, -7.0f }, { -7.0f, -6.0f }, { -7.0f, -5.0f },
    { -7.0f, -4.0f }, { -7.0f, -3.0f }, { -7.0f, -2.0f }, { -7.0f, -1.0f }, { -6.0f, 0.0f },  { -6.0f, 1.0f },
    { -6.0f, 2.0f },  { -6.0f, 3.0f },  { -6.0f, 4.0f },  { -6.0f, 5.0f },  { -6.0f, 6.0f },  { -6.0f, 7.0f },
    { -6.0f, -8.0f }, { -6.0f, -7.0f }, { -6.0f, -6.0f }, { -6.0f, -5.0f }, { -6.0f, -4.0f }, { -6.0f, -3.0f },
    { -6.0f, -2.0f }, { -6.0f, -1.0f }, { -5.0f, 0.0f },  { -5.0f, 1.0f },  { -5.0f, 2.0f },  { -5.0f, 3.0f },
    { -5.0f, 4.0f },  { -5.0f, 5.0f },  { -5.0f, 6.0f },  { -5.0f, 7.0f },  { -5.0f, -8.0f }, { -5.0f, -7.0f },
    { -5.0f, -6.0f }, { -5.0f, -5.0f }, { -5.0f, -4.0f }, { -5.0f, -3.0f }, { -5.0f, -2.0f }, { -5.0f, -1.0f },
    { -4.0f, 0.0f },  { -4.0f, 1.0f },  { -4.0f, 2.0f },  { -4.0f, 3.0f },  { -4.0f, 4.0f },  { -4.0f, 5.0f },
    { -4.0f, 6.0f },  { -4.0f, 7.0f },  { -4.0f, -8.0f }, { -4.0f, -7.0f }, { -4.0f, -6.0f }, { -4.0f, -5.0f },
    { -4.0f, -4.0f }, { -4.0f, -3.0f }, { -4.0f, -2.0f }, { -4.0f, -1.0f }, { -3.0f, 0.0f },  { -3.0f, 1.0f },
    { -3.0f, 2.0f },  { -3.0f, 3.0f },  { -3.0f, 4.0f },  { -3.0f, 5.0f },  { -3.0f, 6.0f },  { -3.0f, 7.0f },
    { -3.0f, -8.0f }, { -3.0f, -7.0f }, { -3.0f, -6.0f }, { -3.0f, -5.0f }, { -3.0f, -4.0f }, { -3.0f, -3.0f },
    { -3.0f, -2.0f }, { -3.0f, -1.0f }, { -2.0f, 0.0f },  { -2.0f, 1.0f },  { -2.0f, 2.0f },  { -2.0f, 3.0f },
    { -2.0f, 4.0f },  { -2.0f, 5.0f },  { -2.0f, 6.0f },  { -2.0f, 7.0f },  { -2.0f, -8.0f }, { -2.0f, -7.0f },
    { -2.0f, -6.0f }, { -2.0f, -5.0f }, { -2.0f, -4.0f }, { -2.0f, -3.0f }, { -2.0f, -2.0f }, { -2.0f, -1.0f },
    { -1.0f, 0.0f },  { -1.0f, 1.0f },  { -1.0f, 2.0f },  { -1.0f, 3.0f },  { -1.0f, 4.0f },  { -1.0f, 5.0f },
    { -1.0f, 6.0f },  { -1.0f, 7.0f },  { -1.0f, -8.0f }, { -1.0f, -7.0f }, { -1.0f, -6.0f }, { -1.0f, -5.0f },
    { -1.0f, -4.0f }, { -1.0f, -3.0f }, { -1.0f, -2.0f }, { -1.0f, -1.0f }
};

static inline void extend_nyblls_to_floats(uint8_t nybll, float* fp1, float* fp2) {
    const float* fpair = nyblls_as_floats[nybll];
    *fp1 = fpair[0];
    *fp2 = fpair[1];
}

void aADPCMdecImpl(uint8_t flags, ADPCM_STATE state) {
    int16_t* out = BUF_S16(rspa.out);
    MEM_BARRIER_PREF(out);
    uint8_t* in = (uint8_t*) ((u8*) rspa.loaded_buffer + rspa.in); // BUF_U8(rspa.in);
    int nbytes = ROUND_UP_32(rspa.nbytes);
    if (flags & A_INIT) {
        shz_zero_16_shorts(out);
    } else if (flags & A_LOOP) {
        shz_copy_16_shorts(out, rspa.adpcm_loop_state);
    } else {
        shz_copy_16_shorts(out, state);
    }
    MEM_BARRIER_PREF(in);
    out += 16;
    float prev1 = (float)out[-1];
    float prev2 = (float)out[-2];

    while (nbytes > 0) {
        const uint8_t si_in = *in++;
        const uint8_t next = *in++;
        MEM_BARRIER_PREF(nyblls_as_floats[next]);
        const uint8_t in_array[2][4] = { { next, *in++, *in++, *in++ }, { *in++, *in++, *in++, *in++ } };
        const unsigned table_index = si_in & 0x7; // should be in 0..7
        const float (*tbl)[8] = rspa.adpcm_table[table_index];
        const float shift = shift_to_float_multiplier(si_in >> 4); // should be in 0..12 or 0..14
        float instr[2][8];

        for (int i = 0; i < 2; ++i) {
            {
                MEM_BARRIER_PREF(nyblls_as_floats[in_array[i][1]]);
                extend_nyblls_to_floats(in_array[i][0], &instr[i][0], &instr[i][1]);
                instr[i][0] *= shift;
                instr[i][1] *= shift;
                MEM_BARRIER_PREF(nyblls_as_floats[in_array[i][2]]);
                extend_nyblls_to_floats(in_array[i][1], &instr[i][2], &instr[i][3]);
                instr[i][2] *= shift;
                instr[i][3] *= shift;
            }
            {
                MEM_BARRIER_PREF(nyblls_as_floats[in_array[i][3]]);
                extend_nyblls_to_floats(in_array[i][2], &instr[i][4], &instr[i][5]);
                instr[i][4] *= shift;
                instr[i][5] *= shift;
                MEM_BARRIER_PREF(&tbl[i][0]);
                extend_nyblls_to_floats(in_array[i][3], &instr[i][6], &instr[i][7]);
                instr[i][6] *= shift;
                instr[i][7] *= shift;
            }
        }
        MEM_BARRIER_PREF(in);

        for (size_t i = 0; i < 2; i++) {
            const float* ins = instr[i];
            shz_vec4_t acc_vec[2];
            float* accf = (float*) acc_vec;
            const shz_vec4_t in_vec = { .x = prev2, .y = prev1, .z = 2048.0f };

            shz_xmtrx_load_3x4_rows((const shz_vec4_t*) &tbl[0][0], (const shz_vec4_t*) &tbl[1][0],
                                    (const shz_vec4_t*) &ins[0]);
            acc_vec[0] = shz_xmtrx_trans_vec4(in_vec);
            shz_xmtrx_load_3x4_rows((const shz_vec4_t*) &tbl[0][4], (const shz_vec4_t*) &tbl[1][4],
                                    (const shz_vec4_t*) &ins[4]);
            acc_vec[1] = shz_xmtrx_trans_vec4(in_vec);

            {
                register float fone asm("fr8") = 1.0f;
                register float ins0 asm("fr9") = ins[0];
                register float ins1 asm("fr10") = ins[1];
                register float ins2 asm("fr11") = ins[2];
                accf[2] = shz_dot8f(fone, ins0, ins1, ins2, accf[2], tbl[1][1], tbl[1][0], 0.0f);
                accf[7] = shz_dot8f(fone, ins0, ins1, ins2, accf[7], tbl[1][6], tbl[1][5], tbl[1][4]);
                accf[1] += (tbl[1][0] * ins0);
                shz_xmtrx_load_4x4_cols((const shz_vec4_t*) &accf[3], (const shz_vec4_t*) &tbl[1][2],
                                        (const shz_vec4_t*) &tbl[1][1], (const shz_vec4_t*) &tbl[1][0]);
                *(SHZ_ALIASING shz_vec4_t*) &accf[3] =
                    shz_xmtrx_trans_vec4((shz_vec4_t) { .x = fone, .y = ins0, .z = ins1, .w = ins2 });
            }
            {
                register float ins3 asm("fr8") = ins[3];
                register float ins4 asm("fr9") = ins[4];
                register float ins5 asm("fr10") = ins[5];
                register float ins6 asm("fr11") = ins[6];
                accf[7] += shz_dot8f(ins3, ins4, ins5, ins6, tbl[1][3], tbl[1][2], tbl[1][1], tbl[1][0]);
                accf[6] += shz_dot8f(ins3, ins4, ins5, ins6, tbl[1][2], tbl[1][1], tbl[1][0], 0.0f);
                accf[5] += (tbl[1][1] * ins3) + (tbl[1][0] * ins4);
                accf[4] += (tbl[1][0] * ins3);
            }

            for (size_t j = 0; j < 6; ++j)
                *out++ = clamp16f(accf[j]*recip2048);

            prev2 = clamp16f(accf[6]*recip2048);
            *out++ = prev2;
            prev1 = clamp16f(accf[7]*recip2048);
            *out++ = prev1;
        }
        MEM_BARRIER_PREF(out);
        nbytes -= 16 * sizeof(int16_t);
    }

    shz_copy_16_shorts(state, (out - 16));
}

void aResampleImpl(uint8_t flags, uint16_t pitch, RESAMPLE_STATE state) {
    int16_t __attribute__((aligned(32))) tmp[32] = { 0 };
    int16_t* in_initial = BUF_S16(rspa.in);
    int16_t* in = in_initial;
    MEM_BARRIER_PREF(in);
    int16_t* out = BUF_S16(rspa.out);
    int nbytes = ROUND_UP_16(rspa.nbytes);
    uint32_t pitch_accumulator = 0;
    int i = 0;
    float* tbl_f = NULL;
    float sample_f = 0;
    size_t l;

    int16_t *dp, *sp;
    int32_t *wdp, *wsp;

    if (!(flags & A_INIT)) {
        dp = tmp;
        sp = state;

        wdp = (int32_t*) dp;
        wsp = (int32_t*) sp;

        if ((((uintptr_t) wdp | (uintptr_t) wsp) & 3) == 0) {
            for (l = 0; l < 8; l++)
                *wdp++ = *wsp++;
        } else {
            for (l = 0; l < 16; l++)
                *dp++ = *sp++;
        }
    }

    in -= 4;
    pitch_accumulator = (uint16_t) tmp[4];
    tbl_f = resample_table[pitch_accumulator >> 10];
    SHZ_PREFETCH(tbl_f);

    dp = in;
    sp = tmp;
    for (l = 0; l < 4; l++)
        *dp++ = *sp++;

    do {
        SHZ_PREFETCH(out);
        for (i = 0; i < 8; i++) {

            float in_f[4] = { (float) (int) in[0], (float) (int) in[1], (float) (int) in[2], (float) (int) in[3] };

            sample_f =
                shz_dot8f(in_f[0], in_f[1], in_f[2], in_f[3], tbl_f[0], tbl_f[1], tbl_f[2], tbl_f[3]) * 0.00003052f;

            MEM_BARRIER();
            pitch_accumulator += (pitch << 1);
            in += pitch_accumulator >> 16;
            MEM_BARRIER_PREF(in);
            pitch_accumulator %= 0x10000;
            MEM_BARRIER();
            *out++ = clamp16f((sample_f));
            MEM_BARRIER();
            tbl_f = resample_table[pitch_accumulator >> 10];
            MEM_BARRIER_PREF(tbl_f);
        }
        nbytes -= 8 * sizeof(int16_t);
    } while (nbytes > 0);

    state[4] = (int16_t) pitch_accumulator;
    dp = (int16_t*) (state);
    sp = in;
    for (l = 0; l < 4; l++)
        *dp++ = *sp++;

    i = (in - in_initial + 4) & 7;
    in -= i;
    if (i != 0) {
        i = -8 - i;
    }
    state[5] = i;
    dp = (int16_t*) (state + 8);
    sp = in;
    for (l = 0; l < 8; l++)
        *dp++ = *sp++;
}

void aEnvSetup1Impl(uint8_t initial_vol_wet, uint16_t rate_wet, uint16_t rate_left, uint16_t rate_right) {
    rspa.rate[0] = rate_left;
    rspa.rate[1] = rate_right;
    rspa.vol_wet = (uint16_t) (initial_vol_wet << 8);
    rspa.rate_wet = rate_wet;
}

void aEnvSetup2Impl(uint16_t initial_vol_left, uint16_t initial_vol_right) {
    rspa.vol[0] = initial_vol_left;
    rspa.vol[1] = initial_vol_right;
}
static int16_t __attribute__((aligned(32))) em_samples[2][2];

void aEnvMixerImpl(uint16_t in_addr, uint16_t n_samples,
                   uint16_t dry_addr_start) {
    int32_t* inwide = (int32_t *)BUF_S16(in_addr);
    size_t n = ROUND_UP_16(n_samples);

    // Note: max number of samples is 192 (192 * 2 = 384 bytes = 0x180)
    int32_t* drywide[2] = { (int32_t*) BUF_S16(dry_addr_start),
                            (int32_t*) BUF_S16((dry_addr_start + 0x180)) };

    uint16_t rates[2] = { rspa.rate[0], rspa.rate[1] };
    uint16_t vols[2] = { rspa.vol[0], rspa.vol[1] };

    SHZ_PREFETCH(inwide);

    for (size_t i = 0; i < n; i += 8) {
        for (size_t k = 0; k < 4; k++) {
            int32_t in12 = *inwide++;

            int16_t sample = (int16_t)(in12 >> 16);
            int16_t sample2 = (int16_t)(in12 & 0xffff);

            em_samples[0][0] = (sample * vols[0] >> 16);
            em_samples[0][1] = (sample2 * vols[0] >> 16);
            em_samples[1][0] = (sample * vols[1] >> 16);
            em_samples[1][1] = (sample2 * vols[1] >> 16);

            MEM_BARRIER();

            int32 dsampl12 = *drywide[0];
            int32 dsampl34 = *drywide[1];

            int16_t dsampl1 = clamp16((int16_t)(dsampl12 >> 16) + em_samples[0][0]);
            int16_t dsampl2 = clamp16((int16_t)(dsampl12 & 0xffff) + em_samples[0][1]);

            int16_t dsampl3 = clamp16((int16_t)(dsampl34 >> 16) + em_samples[1][0]);
            int16_t dsampl4 = clamp16((int16_t)(dsampl34 & 0xffff) + em_samples[1][1]);

            MEM_BARRIER();

            *drywide[0]++ = (dsampl1 << 16) | (dsampl2 & 0xffff);
            *drywide[1]++ = (dsampl3 << 16) | (dsampl4 & 0xffff);
        }
        SHZ_PREFETCH(inwide);
        vols[0] += rates[0];
        vols[1] += rates[1];
    }
}

void aDMEMMove2Impl(uint8_t t, uint16_t in_addr, uint16_t out_addr, uint16_t count) {
    ;
}

void aS8DecImpl(uint8_t flags, ADPCM_STATE state) {
    ;
}

void aResampleZohImpl(uint16_t pitch, uint16_t start_fract) {
    ;
}

void aAddMixerImpl(uint16_t count, uint16_t in_addr, uint16_t out_addr) {
    ;
}

void aUnkCmd19Impl(uint8_t f, uint16_t count, uint16_t out_addr, uint16_t in_addr) {
    ;
}

#define DMEM_UNCOMPRESSED_NOTE 0x600
void aDuplicateImpl(uint16_t count, uint16_t in_addr, uint16_t out_addr) {
    uint8_t* in = (uint8_t*) ((u8*) rspa.loaded_buffer + in_addr);
    uint8_t* real_in = BUF_U8(DMEM_UNCOMPRESSED_NOTE);
    uint8_t* out = BUF_U8(out_addr);
    n64_memcpy(real_in, in, 128);
    // no overlap as called, dont do temp copy
    do {
        n64_memcpy(out, real_in, 128);
        out += 128;
    } while (count-- > 0);
}

#if 0
void aInterlImpl(uint16_t in_addr, uint16_t out_addr, uint16_t n_samples) {
    uint32_t* in = (uint32_t*) BUF_S16(in_addr); // two 16-bit samples per word
    uint32_t* out = (uint32_t*) BUF_S16(out_addr);
    int n = ROUND_UP_8(n_samples);

    // asm volatile("pref @%0" : : "r"(in) : "memory");
    MEM_BARRIER_PREF(in);

    do {
        // asm volatile("pref @%0" : : "r"(out) : "memory");
        MEM_BARRIER_PREF(out);
        /* For 8 output samples:
           read 8 words from in (16 samples), extract every other halfword,
           write 4 packed words to out.
         */
        uint32_t w0 = *in++; // samples in[0],in[1]
        uint32_t w1 = *in++; // samples in[2],in[3]
        uint32_t w2 = *in++;
        uint32_t w3 = *in++;
        uint32_t w4 = *in++;
        uint32_t w5 = *in++;
        uint32_t w6 = *in++;
        uint32_t w7 = *in++;

        MEM_BARRIER_PREF(in);

        /* Pack lower half-word from w0 and w1 -> out word0,
           from w2 and w3 -> out word1, ...  */
        *out++ = (w0 & 0xFFFF) | ((w1 & 0xFFFF) << 16);
        *out++ = (w2 & 0xFFFF) | ((w3 & 0xFFFF) << 16);
        *out++ = (w4 & 0xFFFF) | ((w5 & 0xFFFF) << 16);
        *out++ = (w6 & 0xFFFF) | ((w7 & 0xFFFF) << 16);

        n -= 8;
    } while (n > 0);
}
#else
void aInterlImpl(uint16_t in_addr, uint16_t out_addr, uint16_t n_samples) {
    int16_t* in = BUF_S16(in_addr);
    int16_t* out = BUF_S16(out_addr);
    int n = ROUND_UP_8(n_samples);

    do {
        *out++ = *in++;
        in++;
        *out++ = *in++;
        in++;
        *out++ = *in++;
        in++;
        *out++ = *in++;
        in++;
        *out++ = *in++;
        in++;
        *out++ = *in++;
        in++;
        *out++ = *in++;
        in++;
        *out++ = *in++;
        in++;

        n -= 8;
    } while (n > 0);
}
#endif
#if 0
void aHiLoGainImpl(uint8_t g, uint16_t count, uint16_t addr) {
    int32_t* samples = (int32_t*) ((uintptr_t) BUF_S16(addr));
    int nbytes = ROUND_UP_32(count);

    do {
        uint32_t s1, s2, s3, s4;
        s1 = samples[0];
        s2 = (s1 >> 16) & 0xffff;
        s1 &= 0xffff;
        s3 = samples[1];
        s4 = (s3 >> 16) & 0xffff;
        s3 &= 0xffff;

        MEM_BARRIER();
        s1 = clamp16(s1 * g) >> 4;
        s2 = clamp16(s2 * g) >> 4;
        s3 = clamp16(s3 * g) >> 4;
        s4 = clamp16(s4 * g) >> 4;

        MEM_BARRIER();

        *samples++ = (s2 << 16) | s1;
        *samples++ = (s3 << 16) | s2;
        nbytes -= 4;
    } while (nbytes > 0);
}
#else
void aHiLoGainImpl(uint8_t g, uint16_t count, uint16_t addr) {
    int16_t* samples = BUF_S16(addr);
    int nbytes = ROUND_UP_32(count);

    do {
        *samples = clamp16((*samples * g) >> 4);
        samples++;
        *samples = clamp16((*samples * g) >> 4);
        samples++;
        *samples = clamp16((*samples * g) >> 4);
        samples++;
        *samples = clamp16((*samples * g) >> 4);
        samples++;
        *samples = clamp16((*samples * g) >> 4);
        samples++;
        *samples = clamp16((*samples * g) >> 4);
        samples++;
        *samples = clamp16((*samples * g) >> 4);
        samples++;
        *samples = clamp16((*samples * g) >> 4);
        samples++;

        nbytes -= 8;
    } while (nbytes > 0);
}
#endif

void aMixImpl(uint16_t count, int16_t gain, uint16_t in_addr, uint16_t out_addr) {
    ;
}