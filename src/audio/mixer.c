#if 1
// #include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#undef bool

#include <PR/ultratypes.h>
//#include <align_asset_macro.h>
#ifdef __GNUC__
#define UNUSED __attribute__((unused))
#else
#define UNUSED
#endif

#include "mixer.h"
void n64_memcpy(void* dst, const void* src, size_t size) {
    uint8_t* bdst = (uint8_t*) dst;
    uint8_t* bsrc = (uint8_t*) src;
    uint32_t* sdst = (uint16_t*) dst;
    uint32_t* ssrc = (uint16_t*) src;
    uint32_t* wdst = (uint32_t*) dst;
    uint32_t* wsrc = (uint32_t*) src;

    int size_to_copy = size;
    int words_to_copy = size_to_copy >> 2;
    int shorts_to_copy = size_to_copy >> 1;
    int bytes_to_copy = size_to_copy - (words_to_copy<<2);
    int sbytes_to_copy = size_to_copy - (shorts_to_copy<<1);

    __builtin_prefetch(bsrc);
    if ((!(((uintptr_t)bdst | (uintptr_t)bsrc) & 3))) {
        while (words_to_copy--) {
            if ((words_to_copy & 3) == 0) {
                __builtin_prefetch(bsrc + 16);
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
/*             case 4:
                goto n64copy4;
            case 5:
                goto n64copy5;
            case 6:
                goto n64copy6;
            case 7:
                goto n64copy7; */
        }
    } else if ((!(((uintptr_t)sdst | (uintptr_t)ssrc) & 1))) {
        while (shorts_to_copy--) {
            *sdst++ = *ssrc++;
        }

        bdst = (uint8_t*) sdst;
        bsrc = (uint8_t*) ssrc;

        if (sbytes_to_copy) {
            goto n64copy1;
        }
    }
    else {
        while (words_to_copy > 0) {
            uint8_t b1, b2, b3, b4;
            b1 = *bsrc++;
            b2 = *bsrc++;
            b3 = *bsrc++;
            b4 = *bsrc++;
#if 1
            asm volatile("" : : : "memory");
#endif
            *bdst++ = b1; //*bsrc++;
            *bdst++ = b2; //*bsrc++;
            *bdst++ = b3; //*bsrc++;
            *bdst++ = b4; //*bsrc++;

            words_to_copy--;
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
/*             case 4:
                goto n64copy4;
            case 5:
                goto n64copy5;
            case 6:
                goto n64copy6;
            case 7:
                goto n64copy7; */
        }
    }

/* n64copy7:
    *bdst++ = *bsrc++;
n64copy6:
    *bdst++ = *bsrc++;
n64copy5:
    *bdst++ = *bsrc++;
n64copy4:
    *bdst++ = *bsrc++; */
n64copy3:
    *bdst++ = *bsrc++;
n64copy2:
    *bdst++ = *bsrc++;
n64copy1:
    *bdst++ = *bsrc++;
    return;
}

#define recip8192 0.00012207f
#define recip2048 0.00048828f
#define recip2560 0.00039062f

#define ROUND_UP_64(v) (((v) + 63) & ~63)
#define ROUND_UP_32(v) (((v) + 31) & ~31)
#define ROUND_UP_16(v) (((v) + 15) & ~15)
#define ROUND_UP_8(v) (((v) + 7) & ~7)

#define ROUND_DOWN_16(v) ((v) & ~15)

#define DMEM_BUF_SIZE 8192
//4096
#define BUF_U8(a) (rspa.buf.as_u8 + (a))
#define BUF_S16(a) (rspa.buf.as_s16 + (a) / sizeof(int16_t))

static struct __attribute__((aligned(32))) {
    union __attribute__((aligned(32))) {
        int16_t __attribute__((aligned(32))) as_s16[DMEM_BUF_SIZE / sizeof(int16_t)];
        uint8_t __attribute__((aligned(32))) as_u8[DMEM_BUF_SIZE];
    } buf;

#if 1
    float
#else
int16_t
#endif
    __attribute__((aligned(32))) adpcm_table[8][2][8];
    uint16_t vol[2];
    uint16_t rate[2];

    int16_t __attribute__((aligned(32))) adpcm_loop_state[16];
    uint16_t in;
    uint16_t out;
    uint16_t nbytes;

    uint16_t rate_wet;
    uint16_t vol_wet;
} rspa = { 0 };

#define MEM_BARRIER() asm volatile("" : : : "memory");
#define MEM_BARRIER_PREF(ptr) asm volatile("pref @%0" : : "r"((ptr)) : "memory")
#if 1
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

#if 0
void aClearBufferImpl(uint16_t addr, int nbytes) {
    memset(BUF_U8(addr), 0, ROUND_UP_16(nbytes));
}

void aLoadBufferImpl(const void* source_addr, uint16_t dest_addr, uint16_t nbytes) {
    n64_memcpy((void *)BUF_U8(dest_addr), source_addr, ROUND_DOWN_16(nbytes));
}


void aSaveBufferImpl(uint16_t source_addr, int16_t* dest_addr, uint16_t nbytes) {
    n64_memcpy((void *)dest_addr, (const void *)BUF_S16(source_addr), ROUND_DOWN_16(nbytes));
}
#endif
void aClearBufferImpl(uint16_t addr, int nbytes) {
    memset(BUF_U8(addr&~3), 0, ROUND_UP_16(nbytes));
}   

void *memcpy32(void *restrict dst, const void *restrict src, size_t bytes);

void aLoadBufferImpl(const void* source_addr, uint16_t dest_addr, uint16_t nbytes) {
    size_t rnb = ROUND_DOWN_16(nbytes);
    if (((((uintptr_t)source_addr) | rnb) & 31) == 0) {
        memcpy32((void *)BUF_U8(dest_addr & ~3), (const void *)((uintptr_t)source_addr), rnb);
    } else {
        n64_memcpy((void *)BUF_U8(dest_addr & ~3), (const void *)((uintptr_t)source_addr&~3), rnb);
    }
}    
    
void aSaveBufferImpl(uint16_t source_addr, int16_t* dest_addr, uint16_t nbytes) {
    size_t rnb = ROUND_DOWN_16(nbytes);
    if (((((uintptr_t)dest_addr) & 31)|rnb) == 0) {
        memcpy32((void *)((uintptr_t)dest_addr), (const void *)BUF_S16(source_addr & ~3), rnb);
    } else {
        n64_memcpy((void *)((uintptr_t)dest_addr&~3), (const void *)BUF_S16(source_addr & ~3), rnb);//ROUND_DOWN_16(nbytes));
    }
}

void aLoadADPCMImpl(int num_entries_times_16, const int16_t* book_source_addr) {
    float* aptr = (float*) rspa.adpcm_table;
    short tmp[8];
    __builtin_prefetch(book_source_addr);

    for (size_t i = 0; i < num_entries_times_16 / 2; i += 8) {
        __builtin_prefetch(&aptr[i]);

        tmp[0] = (short) __builtin_bswap16((uint16_t) book_source_addr[i + 0]);
        tmp[1] = (short) __builtin_bswap16((uint16_t) book_source_addr[i + 1]);
        tmp[2] = (short) __builtin_bswap16((uint16_t) book_source_addr[i + 2]);
        tmp[3] = (short) __builtin_bswap16((uint16_t) book_source_addr[i + 3]);
        tmp[4] = (short) __builtin_bswap16((uint16_t) book_source_addr[i + 4]);
        tmp[5] = (short) __builtin_bswap16((uint16_t) book_source_addr[i + 5]);
        tmp[6] = (short) __builtin_bswap16((uint16_t) book_source_addr[i + 6]);
        tmp[7] = (short) __builtin_bswap16((uint16_t) book_source_addr[i + 7]);

        MEM_BARRIER_PREF(&book_source_addr[i + 8]);

        aptr[i + 0] = recip2048 * (f32) (s32) tmp[0];
        aptr[i + 1] = recip2048 * (f32) (s32) tmp[1];
        aptr[i + 2] = recip2048 * (f32) (s32) tmp[2];
        aptr[i + 3] = recip2048 * (f32) (s32) tmp[3];
        aptr[i + 4] = recip2048 * (f32) (s32) tmp[4];
        aptr[i + 5] = recip2048 * (f32) (s32) tmp[5];
        aptr[i + 6] = recip2048 * (f32) (s32) tmp[6];
        aptr[i + 7] = recip2048 * (f32) (s32) tmp[7];
    }
}

void aSetBufferImpl(UNUSED uint8_t flags, uint16_t in, uint16_t out, uint16_t nbytes) {
    rspa.in = in;
    rspa.out = out;
    rspa.nbytes = nbytes;
}

#if 0
void aInterleaveImpl(uint16_t left, uint16_t right) {
    int count = ROUND_UP_16(rspa.nbytes) / sizeof(int16_t) / 8;
    int16_t* l = BUF_S16(left);
    int16_t* r = BUF_S16(right);
//    int32_t* d = (int32_t*) (((uintptr_t) BUF_S16(rspa.out)) & ~3);
    int16_t* d = (int16_t*)BUF_S16(rspa.out);
    __builtin_prefetch(r);

    while (count > 0) {
        __builtin_prefetch(l);
        int16_t r0 = *r++;
        int16_t r1 = *r++;
        int16_t r2 = *r++;
        int16_t r3 = *r++;
        int16_t r4 = *r++;
        int16_t r5 = *r++;
        int16_t r6 = *r++;
        int16_t r7 = *r++;

        __builtin_prefetch(r);

        int16_t l0 = *l++;
        int16_t l1 = *l++;
        int16_t l2 = *l++;
        int16_t l3 = *l++;
        int16_t l4 = *l++;
        int16_t l5 = *l++;
        int16_t l6 = *l++;
        int16_t l7 = *l++;

#if 1
        asm volatile("" : : : "memory");
#endif
//printf("%08x %08x %08x %08x\n",lr0,lr1,lr2,lr3);
        *d++ = l0;
        *d++ = r0;

        *d++ = l1;
        *d++ = r1;

        *d++ = l2;
        *d++ = r2;

        *d++ = l3;
        *d++ = r3;

        *d++ = l4;
        *d++ = r4;

        *d++ = l5;
        *d++ = r5;

        *d++ = l6;
        *d++ = r6;

        *d++ = l7;
        *d++ = r7;


        __builtin_prefetch(d);

        --count;
    }
}
#else
static int32_t __attribute__((aligned(32))) lrbuf[8];

void aInterleaveImpl(uint16_t left, uint16_t right) {
    int count = ROUND_UP_16(rspa.nbytes) / sizeof(int16_t) / 8;
    int32_t* l = BUF_S16(left&~3);
    int32_t* r = BUF_S16(right&~3);
    int32_t* d = (int32_t*) (((uintptr_t) BUF_S16(rspa.out)) & ~3);
    __builtin_prefetch(r);
        
    while (count > 0) {
        __builtin_prefetch(l);
        int32_t right01 = *r++;
        int32_t right23 = *r++;
        int32_t right45 = *r++;
        int32_t right67 = *r++;

        lrbuf[0] = right01 & 0xffff0000;//((uint16_t) *r++ << 16);
        lrbuf[1] = right01 << 16;//((uint16_t) *r++ << 16);
        lrbuf[2] = right23 & 0xffff0000;//((uint16_t) *r++ << 16);
        lrbuf[3] = right23 << 16;//((uint16_t) *r++ << 16);
        lrbuf[4] = right45 & 0xffff0000;//((uint16_t) *r++ << 16);
        lrbuf[5] = right45 << 16;//((uint16_t) *r++ << 16);
        lrbuf[6] = right67 & 0xffff0000;//((uint16_t) *r++ << 16);
        lrbuf[7] = right67 << 16;//((uint16_t) *r++ << 16);
    
        MEM_BARRIER_PREF(r);
    
        int32_t left01 = *l++;
        int32_t left23 = *l++;
        int32_t left45 = *l++;
        int32_t left67 = *l++;
        lrbuf[0] |= (left01 >> 16) & 0xffff;
        lrbuf[1] |= (left01) & 0xffff;

        lrbuf[2] |= (left23 >> 16) & 0xffff;
        lrbuf[3] |= (left23) & 0xffff;

        lrbuf[4] |= (left45 >> 16) & 0xffff;
        lrbuf[5] |= (left45) & 0xffff;

        lrbuf[6] |= (left67 >> 16) & 0xffff;
        lrbuf[7] |= (left67) & 0xffff;

/*         lr0 |= ((int16_t) *l++ & 0xffff);
        lr1 |= ((int16_t) *l++ & 0xffff);
        lr2 |= ((int16_t) *l++ & 0xffff);
        lr3 |= ((int16_t) *l++ & 0xffff);
        lr4 |= ((int16_t) *l++ & 0xffff);
        lr5 |= ((int16_t) *l++ & 0xffff);
        lr6 |= ((int16_t) *l++ & 0xffff);
        lr7 |= ((int16_t) *l++ & 0xffff); */
#if 1
        asm volatile("" : : : "memory");
#endif
//printf("%08x %08x %08x %08x\n",lr0,lr1,lr2,lr3);
        *d++ = lrbuf[0];
        *d++ = lrbuf[1];
        *d++ = lrbuf[2];
        *d++ = lrbuf[3];
        *d++ = lrbuf[4];
        *d++ = lrbuf[5];
        *d++ = lrbuf[6];
        *d++ = lrbuf[7];
        __builtin_prefetch(d);
        
        --count;
    }
}

#endif

#if 0
void aDMEMMoveImpl(uint16_t in_addr, uint16_t out_addr, int nbytes) {
    memmove(BUF_U8(out_addr), BUF_U8(in_addr), ROUND_UP_16(nbytes));
}

void aDMEMCopyImpl(uint16_t in_addr, uint16_t out_addr, int nbytes) {
    n64_memcpy(BUF_U8(out_addr), BUF_U8(in_addr), ROUND_UP_16(nbytes));
}
#endif

void aDMEMMoveImpl(uint16_t in_addr, uint16_t out_addr, int nbytes) {
    memmove(BUF_U8(out_addr&~3), BUF_U8(in_addr&~3), ROUND_UP_16(nbytes));
}
        
void aDMEMCopyImpl(uint16_t in_addr, uint16_t out_addr, int nbytes) {
    n64_memcpy(BUF_U8(out_addr&~3), BUF_U8(in_addr&~3), ROUND_UP_16(nbytes));
}
        


void aSetLoopImpl(ADPCM_STATE* adpcm_loop_state) {
    for (size_t i = 0; i < 16; i++) {
        rspa.adpcm_loop_state[i] = (int16_t)__builtin_bswap16(adpcm_loop_state[i]);
    }
}

#if 0
// https://fgiesen.wordpress.com/2024/10/23/zero-or-sign-extend/
static inline int extend_nyb(int n) {
    return (n ^ 8) - 8;
}
#endif

#include "sh4zam.h"

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
    // v *= recip2048;
    s32 sv = (s32)v;
    if (sv < -32768) {
        return -32768;
    } else if (sv > 32767) {
        return 32767;
    }
    return (s16)sv;
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
    uint8_t* in = BUF_U8(rspa.in);
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
    float prev1 = out[-1];
    float prev2 = out[-2];

    while (nbytes > 0) {
        const uint8_t si_in = *in++;
        const uint8_t next = *in++;
        MEM_BARRIER_PREF(nyblls_as_floats[next]);
        const uint8_t in_array[2][4] = {
            { next, *in++, *in++, *in++ },
            { *in++, *in++, *in++, *in++ }
        };
        const unsigned table_index = si_in & 0xf; // should be in 0..7
        const float(*tbl)[8] = rspa.adpcm_table[table_index];
        const float shift = shift_to_float_multiplier(si_in >> 4); // should be in 0..12 or 0..14
        float instr[2][8];

        for(int i = 0; i < 2; ++i) {
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
            const float *ins = instr[i];
            shz_vec4_t acc_vec[2];
            float *accf = (float *)acc_vec;
            const shz_vec4_t in_vec = { .x = prev2, .y = prev1, .z = 1.0f };

            shz_xmtrx_load_3x4_rows((const shz_vec4_t*)&tbl[0][0], (const shz_vec4_t*)&tbl[1][0], (const shz_vec4_t*)&ins[0]);
            acc_vec[0] = shz_xmtrx_trans_vec4(in_vec);
            shz_xmtrx_load_3x4_rows((const shz_vec4_t*)&tbl[0][4], (const shz_vec4_t*)&tbl[1][4], (const shz_vec4_t*)&ins[4]);
            acc_vec[1] = shz_xmtrx_trans_vec4(in_vec);

            {
                register float fone asm("fr8")  = 1.0f;
                register float ins0 asm("fr9")  = ins[0];
                register float ins1 asm("fr10") = ins[1];
                register float ins2 asm("fr11") = ins[2];
                accf[2] = shz_dot8f(fone, ins0, ins1, ins2, accf[2], tbl[1][1], tbl[1][0], 0.0f);
                accf[7] = shz_dot8f(fone, ins0, ins1, ins2, accf[7], tbl[1][6], tbl[1][5], tbl[1][4]);
                accf[1] += (tbl[1][0] * ins0);
                shz_xmtrx_load_4x4_cols((const shz_vec4_t*)&accf[3], (const shz_vec4_t*)&tbl[1][2], (const shz_vec4_t*)&tbl[1][1], (const shz_vec4_t*)&tbl[1][0]);
                *(SHZ_ALIASING shz_vec4_t*)&accf[3] =
                    shz_xmtrx_trans_vec4((shz_vec4_t) { .x = fone, .y = ins0, .z = ins1, .w = ins2 });
            }
            {
                register float ins3 asm("fr8")  = ins[3];
                register float ins4 asm("fr9")  = ins[4];
                register float ins5 asm("fr10") = ins[5];
                register float ins6 asm("fr11") = ins[6];
                accf[7] += shz_dot8f(ins3, ins4, ins5, ins6, tbl[1][3], tbl[1][2], tbl[1][1], tbl[1][0]);
                accf[6] += shz_dot8f(ins3, ins4, ins5, ins6, tbl[1][2], tbl[1][1], tbl[1][0], 0.0f);
                accf[5] += (tbl[1][1] * ins3) + (tbl[1][0] * ins4);
                accf[4] += (tbl[1][0] * ins3);
            }

            for (size_t j = 0; j < 6; ++j)
                *out++ = clamp16f(accf[j]);

            prev2  = clamp16f(accf[6]);
            *out++ = prev2;
            prev1  = clamp16f(accf[7]);
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

        wdp = (int32_t *)dp;
        wsp = (int32_t *)sp;

        if ((((uintptr_t)wdp | (uintptr_t)wsp) & 3) == 0) {
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
    __builtin_prefetch(tbl_f);

    dp = in;
    sp = tmp;
    for (l = 0; l < 4; l++)
        *dp++ = *sp++;

    do {
        __builtin_prefetch(out);
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
#else

static int16_t resample_table[64][4] = {
    { 0x0c39, 0x66ad, 0x0d46, 0xffdf }, { 0x0b39, 0x6696, 0x0e5f, 0xffd8 }, { 0x0a44, 0x6669, 0x0f83, 0xffd0 },
    { 0x095a, 0x6626, 0x10b4, 0xffc8 }, { 0x087d, 0x65cd, 0x11f0, 0xffbf }, { 0x07ab, 0x655e, 0x1338, 0xffb6 },
    { 0x06e4, 0x64d9, 0x148c, 0xffac }, { 0x0628, 0x643f, 0x15eb, 0xffa1 }, { 0x0577, 0x638f, 0x1756, 0xff96 },
    { 0x04d1, 0x62cb, 0x18cb, 0xff8a }, { 0x0435, 0x61f3, 0x1a4c, 0xff7e }, { 0x03a4, 0x6106, 0x1bd7, 0xff71 },
    { 0x031c, 0x6007, 0x1d6c, 0xff64 }, { 0x029f, 0x5ef5, 0x1f0b, 0xff56 }, { 0x022a, 0x5dd0, 0x20b3, 0xff48 },
    { 0x01be, 0x5c9a, 0x2264, 0xff3a }, { 0x015b, 0x5b53, 0x241e, 0xff2c }, { 0x0101, 0x59fc, 0x25e0, 0xff1e },
    { 0x00ae, 0x5896, 0x27a9, 0xff10 }, { 0x0063, 0x5720, 0x297a, 0xff02 }, { 0x001f, 0x559d, 0x2b50, 0xfef4 },
    { 0xffe2, 0x540d, 0x2d2c, 0xfee8 }, { 0xffac, 0x5270, 0x2f0d, 0xfedb }, { 0xff7c, 0x50c7, 0x30f3, 0xfed0 },
    { 0xff53, 0x4f14, 0x32dc, 0xfec6 }, { 0xff2e, 0x4d57, 0x34c8, 0xfebd }, { 0xff0f, 0x4b91, 0x36b6, 0xfeb6 },
    { 0xfef5, 0x49c2, 0x38a5, 0xfeb0 }, { 0xfedf, 0x47ed, 0x3a95, 0xfeac }, { 0xfece, 0x4611, 0x3c85, 0xfeab },
    { 0xfec0, 0x4430, 0x3e74, 0xfeac }, { 0xfeb6, 0x424a, 0x4060, 0xfeaf }, { 0xfeaf, 0x4060, 0x424a, 0xfeb6 },
    { 0xfeac, 0x3e74, 0x4430, 0xfec0 }, { 0xfeab, 0x3c85, 0x4611, 0xfece }, { 0xfeac, 0x3a95, 0x47ed, 0xfedf },
    { 0xfeb0, 0x38a5, 0x49c2, 0xfef5 }, { 0xfeb6, 0x36b6, 0x4b91, 0xff0f }, { 0xfebd, 0x34c8, 0x4d57, 0xff2e },
    { 0xfec6, 0x32dc, 0x4f14, 0xff53 }, { 0xfed0, 0x30f3, 0x50c7, 0xff7c }, { 0xfedb, 0x2f0d, 0x5270, 0xffac },
    { 0xfee8, 0x2d2c, 0x540d, 0xffe2 }, { 0xfef4, 0x2b50, 0x559d, 0x001f }, { 0xff02, 0x297a, 0x5720, 0x0063 },
    { 0xff10, 0x27a9, 0x5896, 0x00ae }, { 0xff1e, 0x25e0, 0x59fc, 0x0101 }, { 0xff2c, 0x241e, 0x5b53, 0x015b },
    { 0xff3a, 0x2264, 0x5c9a, 0x01be }, { 0xff48, 0x20b3, 0x5dd0, 0x022a }, { 0xff56, 0x1f0b, 0x5ef5, 0x029f },
    { 0xff64, 0x1d6c, 0x6007, 0x031c }, { 0xff71, 0x1bd7, 0x6106, 0x03a4 }, { 0xff7e, 0x1a4c, 0x61f3, 0x0435 },
    { 0xff8a, 0x18cb, 0x62cb, 0x04d1 }, { 0xff96, 0x1756, 0x638f, 0x0577 }, { 0xffa1, 0x15eb, 0x643f, 0x0628 },
    { 0xffac, 0x148c, 0x64d9, 0x06e4 }, { 0xffb6, 0x1338, 0x655e, 0x07ab }, { 0xffbf, 0x11f0, 0x65cd, 0x087d },
    { 0xffc8, 0x10b4, 0x6626, 0x095a }, { 0xffd0, 0x0f83, 0x6669, 0x0a44 }, { 0xffd8, 0x0e5f, 0x6696, 0x0b39 },
    { 0xffdf, 0x0d46, 0x66ad, 0x0c39 }
};

static inline int16_t clamp16(int32_t v) {
    if (v < -0x8000) {
        return -0x8000;
    } else if (v > 0x7fff) {
        return 0x7fff;
    }
    return (int16_t) v;
}

static inline int32_t clamp32(int64_t v) {
    if (v < -0x7fffffff - 1) {
        return -0x7fffffff - 1;
    } else if (v > 0x7fffffff) {
        return 0x7fffffff;
    }
    return (int32_t) v;
}
void aClearBufferImpl(uint16_t addr, int nbytes) {
    memset(BUF_U8(addr&~3), 0, ROUND_UP_16(nbytes));
}

void aLoadBufferImpl(const void* source_addr, uint16_t dest_addr, uint16_t nbytes) {
    n64_memcpy((void *)BUF_U8(dest_addr & ~3), source_addr, ROUND_DOWN_16(nbytes));
}


void aSaveBufferImpl(uint16_t source_addr, int16_t* dest_addr, uint16_t nbytes) {
    n64_memcpy((void *)dest_addr, (const void *)BUF_S16(source_addr & ~3), ROUND_DOWN_16(nbytes));
}

void aLoadADPCMImpl(int num_entries_times_16, const int16_t* book_source_addr) {
    int16_t *raptr = (int16_t *)rspa.adpcm_table;
//    memcpy(rspa.adpcm_table, book_source_addr, num_entries_times_16);
    for (size_t i = 0; i < num_entries_times_16; i++) {
        raptr[i] = (int16_t)__builtin_bswap16(book_source_addr[i]);
    }

}

void aSetBufferImpl(uint8_t flags, uint16_t in, uint16_t out, uint16_t nbytes) {
    rspa.in = in;
    rspa.out = out;
    rspa.nbytes = nbytes;
}

#if 0
void aInterleaveImpl(uint16_t left, uint16_t right) {
    if (rspa.nbytes == 0) {
        return;
    }

    int count = rspa.nbytes / (2 * sizeof(int16_t));

    int16_t* l = BUF_S16(left);
    int16_t* r = BUF_S16(right);
    int16_t* d = BUF_S16(rspa.out);

        for (int i = 0; i < count; i++) {
            *d++ = *l++;
            *d++ = *r++;
        }
}
#endif

void aInterleaveImpl(uint16_t left, uint16_t right) {
    int count = ROUND_UP_16(rspa.nbytes) / sizeof(int16_t) / 8;
    int16_t* l = BUF_S16(left&~3);
    int16_t* r = BUF_S16(right&~3);
    int32_t* d = (int32_t*) (((uintptr_t) BUF_S16(rspa.out)) & ~3);
    __builtin_prefetch(r);

    while (count > 0) {
        __builtin_prefetch(l);
        int32_t lr0 = ((uint16_t) *r++ << 16);
        int32_t lr1 = ((uint16_t) *r++ << 16);
        int32_t lr2 = ((uint16_t) *r++ << 16);
        int32_t lr3 = ((uint16_t) *r++ << 16);
        int32_t lr4 = ((uint16_t) *r++ << 16);
        int32_t lr5 = ((uint16_t) *r++ << 16);
        int32_t lr6 = ((uint16_t) *r++ << 16);
        int32_t lr7 = ((uint16_t) *r++ << 16);

        __builtin_prefetch(r);

        lr0 |= ((int16_t) *l++ & 0xffff);
        lr1 |= ((int16_t) *l++ & 0xffff);
        lr2 |= ((int16_t) *l++ & 0xffff);
        lr3 |= ((int16_t) *l++ & 0xffff);
        lr4 |= ((int16_t) *l++ & 0xffff);
        lr5 |= ((int16_t) *l++ & 0xffff);
        lr6 |= ((int16_t) *l++ & 0xffff);
        lr7 |= ((int16_t) *l++ & 0xffff);
#if 1
        asm volatile("" : : : "memory");
#endif
//printf("%08x %08x %08x %08x\n",lr0,lr1,lr2,lr3);
        *d++ = lr0;
        *d++ = lr1;
        *d++ = lr2;
        *d++ = lr3;
        *d++ = lr4;
        *d++ = lr5;
        *d++ = lr6;
        *d++ = lr7;
        __builtin_prefetch(d);

        --count;
    }
}

void aDMEMMoveImpl(uint16_t in_addr, uint16_t out_addr, int nbytes) {
    nbytes = ROUND_UP_16(nbytes);
    memmove(BUF_U8(out_addr), BUF_U8(in_addr), nbytes);
}


void aSetLoopImpl(ADPCM_STATE* adpcm_loop_state) {
    for (size_t i = 0; i < 16; i++) {
        rspa.adpcm_loop_state[i] = (int16_t)__builtin_bswap16(adpcm_loop_state[i]);
    }
   
//   memcpy(rspa.adpcm_loop_state,adpcm_loop_state,32);
}

void aADPCMdecImpl(uint8_t flags, ADPCM_STATE state) {
    uint8_t* in = BUF_U8(rspa.in);
    int16_t* out = BUF_S16(rspa.out);
    int nbytes = ROUND_UP_32(rspa.nbytes);
    if (flags & A_INIT) {
        memset(out, 0, 16 * sizeof(int16_t));
    } else if (flags & A_LOOP) {
        n64_memcpy(out, rspa.adpcm_loop_state, 16 * sizeof(int16_t));
    } else {
        n64_memcpy(out, state, 16 * sizeof(int16_t));
    }
    out += 16;

    while (nbytes > 0) {
        int shift = *in >> 4;          // should be in 0..12 or 0..14
        int table_index = *in++ & 0xf; // should be in 0..7
        int16_t(*tbl)[8] = rspa.adpcm_table[table_index];
        int i;

        for (i = 0; i < 2; i++) {
            int16_t ins[8];
            int16_t prev1 = out[-1];
            int16_t prev2 = out[-2];
            int j, k;

/*            if (flags & 4) {
                for (j = 0; j < 2; j++) {
                    ins[j * 4] = (((*in >> 6) << 30) >> 30) << shift;
                    ins[j * 4 + 1] = ((((*in >> 4) & 0x3) << 30) >> 30) << shift;
                    ins[j * 4 + 2] = ((((*in >> 2) & 0x3) << 30) >> 30) << shift;
                    ins[j * 4 + 3] = (((*in++ & 0x3) << 30) >> 30) << shift;
                }
            } else {
*/
                for (j = 0; j < 4; j++) {
                    ins[j * 2] = (((*in >> 4) << 28) >> 28) << shift;
                    ins[j * 2 + 1] = (((*in++ & 0xf) << 28) >> 28) << shift;
                }
/*            }*/
            for (j = 0; j < 8; j++) {
                int32_t acc = tbl[0][j] * prev2 + tbl[1][j] * prev1 + (ins[j] << 11);
                for (k = 0; k < j; k++) {
                    acc += tbl[1][((j - k) - 1)] * ins[k];
                }
                acc >>= 11;
                *out++ = clamp16(acc);
            }
        }
        nbytes -= 16 * sizeof(int16_t);
    }
    n64_memcpy(state, out - 16, 16 * sizeof(int16_t));
}


void aResampleImpl(uint8_t flags, uint16_t pitch, RESAMPLE_STATE state) {
    int16_t tmp[16];
    int16_t* in_initial = BUF_S16(rspa.in);
    int16_t* in = in_initial;
    int16_t* out = BUF_S16(rspa.out);
    int nbytes = ROUND_UP_16(rspa.nbytes);
    uint32_t pitch_accumulator;
    int i;
    int16_t* tbl;
    int32_t sample;
    int16_t *dp, *sp;
    int32_t *wdp, *wsp;
    size_t l;
    if (!(flags & A_INIT)) {
        dp = tmp;
        sp = state;

        wdp = (int32_t *)dp;
        wsp = (int32_t *)sp;

        if ((((uintptr_t)wdp | (uintptr_t)wsp) & 3) == 0) {
            for (l = 0; l < 8; l++)
                *wdp++ = *wsp++;
        } else {
            for (l = 0; l < 16; l++)
                *dp++ = *sp++;
        }
    }

    in -= 4;
    pitch_accumulator = (uint16_t) tmp[4];
    memcpy(in, tmp, 4 * sizeof(int16_t));

    do {
        for (i = 0; i < 8; i++) {
            tbl = resample_table[pitch_accumulator * 64 >> 16];
            sample = ((in[0] * tbl[0] + 0x4000) >> 15) + ((in[1] * tbl[1] + 0x4000) >> 15) +
                     ((in[2] * tbl[2] + 0x4000) >> 15) + ((in[3] * tbl[3] + 0x4000) >> 15);
            *out++ = clamp16(sample);

            pitch_accumulator += (pitch << 1);
            in += pitch_accumulator >> 16;
            pitch_accumulator %= 0x10000;
        }
        nbytes -= 8 * sizeof(int16_t);
    } while (nbytes > 0);

    state[4] = (int16_t) pitch_accumulator;
    memcpy(state, in, 4 * sizeof(int16_t));
    i = (in - in_initial + 4) & 7;
    in -= i;
    if (i != 0) {
        i = -8 - i;
    }
    state[5] = i;
    memcpy(state + 8, in, 8 * sizeof(int16_t));
}
#endif

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

void aEnvMixerImpl(uint16_t in_addr, uint16_t n_samples, bool swap_reverb, bool neg_left,
                    bool neg_right, uint32_t wet_dry_addr, uint32_t haas_temp_addr, uint32_t num_channels,
                   uint32_t cutoff_freq_lfe) {
    int16_t* in = BUF_S16(in_addr);
    size_t n = ROUND_UP_16(n_samples);

    int dry_addr_start = wet_dry_addr & 0xFFFF;
#if 0
    int wet_addr_start = wet_dry_addr >> 16;
#endif
    // Note: max number of samples is 192 (192 * 2 = 384 bytes = 0x180)
    int32_t *drywide[2] = { (int32_t *)BUF_S16(dry_addr_start &~3),(int32_t *) BUF_S16((dry_addr_start + 0x180)&~3) };
    int16_t* dry[2] = { BUF_S16(dry_addr_start &~3), BUF_S16((dry_addr_start + 0x180)&~3) };
#if 0
    int16_t* wet[2] = { BUF_S16(wet_addr_start), BUF_S16(wet_addr_start + 0x180) };
#endif
    uint16_t rates[2] = { rspa.rate[0], rspa.rate[1] };
    uint16_t vols[2] = { rspa.vol[0], rspa.vol[1] };
#if 0
    uint16_t rate_wet = rspa.rate_wet;
    uint16_t vol_wet = rspa.vol_wet;
    int swapped[2] = { swap_reverb ? 1 : 0, swap_reverb ? 0 : 1 };

    // Account for haas effect
    int haas_addr_left = haas_temp_addr >> 16;
    int haas_addr_right = haas_temp_addr & 0xFFFF;
#endif
    __builtin_prefetch(in);

#if 0
    if (haas_addr_left) {
        dry[0] = BUF_S16(haas_addr_left);
    } else if (haas_addr_right) {
        dry[1] = BUF_S16(haas_addr_right);
    }

    int16_t negs[2] = { neg_left ? 0 : 0xFFFF, neg_right ? 0 : 0xFFFF };
#endif

    for (size_t i = 0; i < n; i+=8) {
        // printf("i %d\n",i);
        for (size_t k = 0; k < 4; k++) {

            int16_t sample = *in++;
            int16_t sample2 = *in++;
            int16_t samples[2][2]; 
#if 0
            samples[0] = (sample * vols[0] >> 16) ^ negs[0];
            samples[1] = (sample * vols[1] >> 16) ^ negs[1];
#else
            samples[0][0] = (sample * vols[0] >> 16);
            samples[0][1] = (sample2 * vols[0] >> 16);
            samples[1][0] = (sample * vols[1] >> 16);
            samples[1][1] = (sample2 * vols[1] >> 16);
#endif

            int16_t dsampl1 = clamp16(*dry[0]++ + samples[0][0]);
            int16_t dsampl2 = clamp16(*dry[0]++ + samples[0][1]);

            int16_t dsampl3 = clamp16(*dry[1]++ + samples[1][0]);
            int16_t dsampl4 = clamp16(*dry[1]++ + samples[1][1]);
#if 0
            int32_t wsampl1 = *wet[0] + (samples[swapped[0]] * vol_wet >> 16);
            int32_t wsampl2 = *wet[1] + (samples[swapped[1]] * vol_wet >> 16);
#endif

#if 1
            asm volatile("" : : : "memory");
#endif
            //*dry[0]++ = clamp16(dsampl1);
            *drywide[0]++ = (dsampl2 << 16) | (dsampl1&0xffff);
            *drywide[1]++ = (dsampl4 << 16) | (dsampl3&0xffff);
            //*dry[1]++ = clamp16(dsampl2);
        }
        __builtin_prefetch(in);
        vols[0] += rates[0];
        vols[1] += rates[1];
#if 0
        vol_wet += rate_wet;
#endif
    }
}

void aDMEMMove2Impl(uint8_t t, uint16_t in_addr, uint16_t out_addr, uint16_t count) {
#if 0
    uint8_t* in = BUF_U8(in_addr);
    uint8_t* out = BUF_U8(out_addr);
    int nbytes = ROUND_UP_32(count);

    do {
        // out and in never overlap as called by `synthesis.c`
        n64_memcpy(out, in, nbytes);
        out += nbytes;
    } while (t-- > 0);
#endif
}

void aS8DecImpl(uint8_t flags, ADPCM_STATE state) {
    uint8_t* in = BUF_U8(rspa.in);
    int16_t* out = BUF_S16(rspa.out);
    int nbytes = ROUND_UP_32(rspa.nbytes);
    if (flags & A_INIT) {
        memset(out, 0, 16 * sizeof(int16_t));
    } else if (flags & A_LOOP) {
        n64_memcpy(out, rspa.adpcm_loop_state, 16 * sizeof(int16_t));
    } else {
        n64_memcpy(out, state, 16 * sizeof(int16_t));
    }
    out += 16;

    while (nbytes > 0) {
        *out++ = (int16_t) (*in++ << 8);
        *out++ = (int16_t) (*in++ << 8);
        *out++ = (int16_t) (*in++ << 8);
        *out++ = (int16_t) (*in++ << 8);
        *out++ = (int16_t) (*in++ << 8);
        *out++ = (int16_t) (*in++ << 8);
        *out++ = (int16_t) (*in++ << 8);
        *out++ = (int16_t) (*in++ << 8);
        *out++ = (int16_t) (*in++ << 8);
        *out++ = (int16_t) (*in++ << 8);
        *out++ = (int16_t) (*in++ << 8);
        *out++ = (int16_t) (*in++ << 8);
        *out++ = (int16_t) (*in++ << 8);
        *out++ = (int16_t) (*in++ << 8);
        *out++ = (int16_t) (*in++ << 8);
        *out++ = (int16_t) (*in++ << 8);

        nbytes -= 16 * sizeof(int16_t);
    }

    n64_memcpy(state, out - 16, 16 * sizeof(int16_t));
}



void aResampleZohImpl(uint16_t pitch, uint16_t start_fract) {
    int16_t* in = BUF_S16(rspa.in);
    int16_t* out = BUF_S16(rspa.out);
    int nbytes = ROUND_UP_8(rspa.nbytes);
    uint32_t pos = start_fract;
    uint32_t pitch_add = pitch << 2;

    do {
        *out++ = in[pos >> 17];
        pos += pitch_add;
        *out++ = in[pos >> 17];
        pos += pitch_add;
        *out++ = in[pos >> 17];
        pos += pitch_add;
        *out++ = in[pos >> 17];
        pos += pitch_add;

        nbytes -= 4 * sizeof(int16_t);
    } while (nbytes > 0);
}

void aAddMixerImpl(uint16_t count, uint16_t in_addr, uint16_t out_addr) {
    int16_t* in = BUF_S16(in_addr);
    int16_t* out = BUF_S16(out_addr);
    int nbytes = ROUND_UP_64(ROUND_DOWN_16(count));

    do {
        *out = clamp16(*out + *in++);
        out++;
        *out = clamp16(*out + *in++);
        out++;
        *out = clamp16(*out + *in++);
        out++;
        *out = clamp16(*out + *in++);
        out++;
        *out = clamp16(*out + *in++);
        out++;
        *out = clamp16(*out + *in++);
        out++;
        *out = clamp16(*out + *in++);
        out++;
        *out = clamp16(*out + *in++);
        out++;
        *out = clamp16(*out + *in++);
        out++;
        *out = clamp16(*out + *in++);
        out++;
        *out = clamp16(*out + *in++);
        out++;
        *out = clamp16(*out + *in++);
        out++;
        *out = clamp16(*out + *in++);
        out++;
        *out = clamp16(*out + *in++);
        out++;
        *out = clamp16(*out + *in++);
        out++;
        *out = clamp16(*out + *in++);
        out++;

        nbytes -= 16 * sizeof(int16_t);
    } while (nbytes > 0);
}

void aUnkCmd19Impl(uint8_t f, uint16_t count, uint16_t out_addr, uint16_t in_addr) {
    int nbytes = ROUND_UP_64(count);
    int16_t* in = BUF_S16(in_addr + f);
    int16_t* out = BUF_S16(out_addr);
    int16_t tbl[32];

    n64_memcpy(tbl, in, 32 * sizeof(int16_t));
    do {
        for (size_t i = 0; i < 32; i++) {
            out[i] = clamp16(out[i] * tbl[i]);
        }
        out += 32;
        nbytes -= 32 * sizeof(int16_t);
    } while (nbytes > 0);
}

void aDuplicateImpl(uint16_t count, uint16_t in_addr, uint16_t out_addr) {
    uint8_t* in = BUF_U8(in_addr);
    uint8_t* out = BUF_U8(out_addr);

    // no overlap as called, dont do temp copy
    do {
        n64_memcpy(out, in, 128);
        out += 128;
    } while (count-- > 0);
}
#if 0
// this gets used
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
#else
void aInterlImpl(uint16_t in_addr, uint16_t out_addr, uint16_t n_samples) {
    uint32_t* in  = (uint32_t*)BUF_S16(in_addr&~3);  // two 16-bit samples per word
    uint32_t* out = (uint32_t*)BUF_S16(out_addr&~3);
    int n = ROUND_UP_8(n_samples);
        
    asm volatile("pref @%0" : : "r"(in) : "memory");
        
    do {
        asm volatile("pref @%0" : : "r"(out) : "memory");
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
        asm volatile("pref @%0" : : "r"(in) : "memory");
    
        /* Pack lower half-word from w0 and w1 -> out word0,
           from w2 and w3 -> out word1, ...  */
        *out++ = (w0 & 0xFFFF) | ((w1 & 0xFFFF) << 16);
        *out++ = (w2 & 0xFFFF) | ((w3 & 0xFFFF) << 16);
        *out++ = (w4 & 0xFFFF) | ((w5 & 0xFFFF) << 16);
        *out++ = (w6 & 0xFFFF) | ((w7 & 0xFFFF) << 16);
        
        n -= 8;
    } while (n > 0);
}
#endif

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
    int nbytes = ROUND_UP_32(ROUND_DOWN_16(count << 4));
    int16_t* in = BUF_S16(in_addr);
    int16_t* out = BUF_S16(out_addr);
    int i;
    int32_t sample;

#if 0
    if (gain == -0x8000) {
        while (nbytes > 0) {
            for (i = 0; i < 16; i++) {
                sample = *out - *in++;
                *out++ = clamp16(sample);
            }
            nbytes -= 16 * sizeof(int16_t);
        }
    }

    while (nbytes > 0) {
        for (i = 0; i < 16; i++) {
            sample = ((*out  * 0x7fff + *in++  * gain)  + 0x4000)  >> 15;
            *out++ = clamp16(sample);
        }

        nbytes -= 16 * sizeof(int16_t);
    }
#else
while (nbytes > 0) {
        for (i = 0; i < 16; i++) {
            sample = *out + *in++;
            *out++ = clamp16(sample);
        }

        nbytes -= 16 * sizeof(int16_t);
    }
#endif
}

#if 0
// #include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#undef bool

#include <PR/ultratypes.h>
#include <align_asset_macro.h>
#ifdef __GNUC__
#define UNUSED __attribute__((unused))
#else
#define UNUSED
#endif

#include "mixer.h"

#ifndef __clang__
#pragma GCC optimize("unroll-loops")
#endif

#if defined(__SSE2__) || defined(__aarch64__)
#define SSE2_AVAILABLE
#else
#pragma message("Warning: SSE2 support is not available. Code will not compile")
#endif

#if defined(__SSE2__)
#include <emmintrin.h>
#elif defined(__aarch64__)
#include "sse2neon.h"
#endif

#ifdef SSE2_AVAILABLE
typedef struct {
    __m128i lo, hi;
} m256i;

static m256i m256i_mul_epi16(__m128i a, __m128i b) {
    m256i res;
    res.lo = _mm_mullo_epi16(a, b);
    res.hi = _mm_mulhi_epi16(a, b);

    m256i ret;
    ret.lo = _mm_unpacklo_epi16(res.lo, res.hi);
    ret.hi = _mm_unpackhi_epi16(res.lo, res.hi);
    return ret;
}

static m256i m256i_add_m256i_epi32(m256i a, m256i b) {
    m256i res;
    res.lo = _mm_add_epi32(a.lo, b.lo);
    res.hi = _mm_add_epi32(a.hi, b.hi);
    return res;
}

static m256i m256i_add_m128i_epi32(m256i a, __m128i b) {
    m256i res;
    res.lo = _mm_add_epi32(a.lo, b);
    res.hi = _mm_add_epi32(a.hi, b);
    return res;
}

static m256i m256i_srai(m256i a, int b) {
    m256i res;
    res.lo = _mm_srai_epi32(a.lo, b);
    res.hi = _mm_srai_epi32(a.hi, b);
    return res;
}

static __m128i m256i_clamp_to_m128i(m256i a) {
    return _mm_packs_epi32(a.lo, a.hi);
}
#endif

#define ROUND_UP_64(v) (((v) + 63) & ~63)
#define ROUND_UP_32(v) (((v) + 31) & ~31)
#define ROUND_UP_16(v) (((v) + 15) & ~15)
#define ROUND_UP_8(v) (((v) + 7) & ~7)
#define ROUND_DOWN_16(v) ((v) & ~0xf)

#define DMEM_BUF_SIZE (4096) 
#define BUF_U8(a) (rspa.buf.as_u8 + (a))
#define BUF_S16(a) (rspa.buf.as_s16 + (a) / sizeof(int16_t))

#define SAMPLE_RATE 32000 // Adjusted to match the actual sample rate of 32 kHz

static struct {
    uint16_t in;
    uint16_t out;
    uint16_t nbytes;

    uint16_t vol[6];
    uint16_t rate[6];
    uint16_t vol_wet;
    uint16_t rate_wet;

    ADPCM_STATE* adpcm_loop_state;

    int16_t adpcm_table[8][2][8];

    uint16_t filter_count;
    int16_t filter[8];

union {
        int16_t as_s16[DMEM_BUF_SIZE / sizeof(int16_t)];
        uint8_t as_u8[DMEM_BUF_SIZE];
    } buf;} rspa;

static int16_t resample_table[64][4] = {
    { 0x0c39, 0x66ad, 0x0d46, 0xffdf }, { 0x0b39, 0x6696, 0x0e5f, 0xffd8 }, { 0x0a44, 0x6669, 0x0f83, 0xffd0 },
    { 0x095a, 0x6626, 0x10b4, 0xffc8 }, { 0x087d, 0x65cd, 0x11f0, 0xffbf }, { 0x07ab, 0x655e, 0x1338, 0xffb6 },
    { 0x06e4, 0x64d9, 0x148c, 0xffac }, { 0x0628, 0x643f, 0x15eb, 0xffa1 }, { 0x0577, 0x638f, 0x1756, 0xff96 },
    { 0x04d1, 0x62cb, 0x18cb, 0xff8a }, { 0x0435, 0x61f3, 0x1a4c, 0xff7e }, { 0x03a4, 0x6106, 0x1bd7, 0xff71 },
    { 0x031c, 0x6007, 0x1d6c, 0xff64 }, { 0x029f, 0x5ef5, 0x1f0b, 0xff56 }, { 0x022a, 0x5dd0, 0x20b3, 0xff48 },
    { 0x01be, 0x5c9a, 0x2264, 0xff3a }, { 0x015b, 0x5b53, 0x241e, 0xff2c }, { 0x0101, 0x59fc, 0x25e0, 0xff1e },
    { 0x00ae, 0x5896, 0x27a9, 0xff10 }, { 0x0063, 0x5720, 0x297a, 0xff02 }, { 0x001f, 0x559d, 0x2b50, 0xfef4 },
    { 0xffe2, 0x540d, 0x2d2c, 0xfee8 }, { 0xffac, 0x5270, 0x2f0d, 0xfedb }, { 0xff7c, 0x50c7, 0x30f3, 0xfed0 },
    { 0xff53, 0x4f14, 0x32dc, 0xfec6 }, { 0xff2e, 0x4d57, 0x34c8, 0xfebd }, { 0xff0f, 0x4b91, 0x36b6, 0xfeb6 },
    { 0xfef5, 0x49c2, 0x38a5, 0xfeb0 }, { 0xfedf, 0x47ed, 0x3a95, 0xfeac }, { 0xfece, 0x4611, 0x3c85, 0xfeab },
    { 0xfec0, 0x4430, 0x3e74, 0xfeac }, { 0xfeb6, 0x424a, 0x4060, 0xfeaf }, { 0xfeaf, 0x4060, 0x424a, 0xfeb6 },
    { 0xfeac, 0x3e74, 0x4430, 0xfec0 }, { 0xfeab, 0x3c85, 0x4611, 0xfece }, { 0xfeac, 0x3a95, 0x47ed, 0xfedf },
    { 0xfeb0, 0x38a5, 0x49c2, 0xfef5 }, { 0xfeb6, 0x36b6, 0x4b91, 0xff0f }, { 0xfebd, 0x34c8, 0x4d57, 0xff2e },
    { 0xfec6, 0x32dc, 0x4f14, 0xff53 }, { 0xfed0, 0x30f3, 0x50c7, 0xff7c }, { 0xfedb, 0x2f0d, 0x5270, 0xffac },
    { 0xfee8, 0x2d2c, 0x540d, 0xffe2 }, { 0xfef4, 0x2b50, 0x559d, 0x001f }, { 0xff02, 0x297a, 0x5720, 0x0063 },
    { 0xff10, 0x27a9, 0x5896, 0x00ae }, { 0xff1e, 0x25e0, 0x59fc, 0x0101 }, { 0xff2c, 0x241e, 0x5b53, 0x015b },
    { 0xff3a, 0x2264, 0x5c9a, 0x01be }, { 0xff48, 0x20b3, 0x5dd0, 0x022a }, { 0xff56, 0x1f0b, 0x5ef5, 0x029f },
    { 0xff64, 0x1d6c, 0x6007, 0x031c }, { 0xff71, 0x1bd7, 0x6106, 0x03a4 }, { 0xff7e, 0x1a4c, 0x61f3, 0x0435 },
    { 0xff8a, 0x18cb, 0x62cb, 0x04d1 }, { 0xff96, 0x1756, 0x638f, 0x0577 }, { 0xffa1, 0x15eb, 0x643f, 0x0628 },
    { 0xffac, 0x148c, 0x64d9, 0x06e4 }, { 0xffb6, 0x1338, 0x655e, 0x07ab }, { 0xffbf, 0x11f0, 0x65cd, 0x087d },
    { 0xffc8, 0x10b4, 0x6626, 0x095a }, { 0xffd0, 0x0f83, 0x6669, 0x0a44 }, { 0xffd8, 0x0e5f, 0x6696, 0x0b39 },
    { 0xffdf, 0x0d46, 0x66ad, 0x0c39 }
};

static inline int16_t clamp16(int32_t v) {
    if (v < -0x8000) {
        return -0x8000;
    } else if (v > 0x7fff) {
        return 0x7fff;
    }
    return (int16_t) v;
}

static inline int32_t clamp32(int64_t v) {
    if (v < -0x7fffffff - 1) {
        return -0x7fffffff - 1;
    } else if (v > 0x7fffffff) {
        return 0x7fffffff;
    }
    return (int32_t) v;
}

void aClearBufferImpl(uint16_t addr, int nbytes) {
    nbytes = ROUND_UP_16(nbytes);
    memset(BUF_U8(addr), 0, nbytes);
}

void aLoadBufferImpl(const void* source_addr, uint16_t dest_addr, uint16_t nbytes) {
#if __SANITIZE_ADDRESS__
    for (size_t i = 0; i < ROUND_DOWN_16(nbytes); i++) {
        BUF_U8(dest_addr)[i] = ((const unsigned char*) source_addr)[i];
    }
#else
    memcpy(BUF_U8(dest_addr), source_addr, ROUND_DOWN_16(nbytes));
#endif
}

void aSaveBufferImpl(uint16_t source_addr, int16_t* dest_addr, uint16_t nbytes) {
    // printf("source_addr: %x\n dest_addr; %x\n nbytes: %d\n", source_addr, dest_addr, nbytes);
    // if (nbytes > 704) {nbytes = 704;}
    memcpy(dest_addr, BUF_S16(source_addr), ROUND_DOWN_16(nbytes));
}

void aLoadADPCMImpl(int num_entries_times_16, const int16_t* book_source_addr) {
    int16_t *ptr16 = rspa.adpcm_table;
    memcpy(rspa.adpcm_table, book_source_addr, num_entries_times_16);
    for (int i=0;i<num_entries_times_16;i++) {
        ptr16[i] = __builtin_bswap16(ptr16[i]);
    }
}

void aSetBufferImpl(uint8_t flags, uint16_t in, uint16_t out, uint16_t nbytes) {
    rspa.in = in;
    rspa.out = out;
    rspa.nbytes = nbytes;
}

void aInterleaveImpl(uint16_t left, uint16_t right) {
    int count = ROUND_UP_16(rspa.nbytes) / sizeof(int16_t) / 8;
    int16_t *l = rspa.buf.as_s16 + left / sizeof(int16_t);
    int16_t *r = rspa.buf.as_s16 + right / sizeof(int16_t);
    int16_t *d = rspa.buf.as_s16 + rspa.out / sizeof(int16_t);
    while (count > 0) {
        int16_t l0 = *l++;
        int16_t l1 = *l++;
        int16_t l2 = *l++;
        int16_t l3 = *l++;
        int16_t l4 = *l++;
        int16_t l5 = *l++;
        int16_t l6 = *l++;
        int16_t l7 = *l++;
        int16_t r0 = *r++;
        int16_t r1 = *r++;
        int16_t r2 = *r++;
        int16_t r3 = *r++;
        int16_t r4 = *r++;
        int16_t r5 = *r++;
        int16_t r6 = *r++;
        int16_t r7 = *r++;
        *d++ = l0;
        *d++ = r0;
        *d++ = l1;
        *d++ = r1;
        *d++ = l2;
        *d++ = r2;
        *d++ = l3;
        *d++ = r3;
        *d++ = l4;
        *d++ = r4;
        *d++ = l5;
        *d++ = r5;
        *d++ = l6;
        *d++ = r6;
        *d++ = l7;
        *d++ = r7;
        --count;
    }
}


void aDMEMMoveImpl(uint16_t in_addr, uint16_t out_addr, int nbytes) {
    nbytes = ROUND_UP_16(nbytes);
    memmove(BUF_U8(out_addr), BUF_U8(in_addr), nbytes);
}

void aSetLoopImpl(ADPCM_STATE* adpcm_loop_state) {
    rspa.adpcm_loop_state = adpcm_loop_state;
}

void aADPCMdecImpl(uint8_t flags, ADPCM_STATE state) {
    uint8_t* in = BUF_U8(rspa.in);
    int16_t* out = BUF_S16(rspa.out);
    int nbytes = ROUND_UP_32(rspa.nbytes);
    if (flags & A_INIT) {
        memset(out, 0, 16 * sizeof(int16_t));
    } else if (flags & A_LOOP) {
        memcpy(out, rspa.adpcm_loop_state, 16 * sizeof(int16_t));
    } else {
        memcpy(out, state, 16 * sizeof(int16_t));
    }
    out += 16;

    while (nbytes > 0) {
        int shift = *in >> 4;          // should be in 0..12 or 0..14
        int table_index = *in++ & 0xf; // should be in 0..7
        int16_t(*tbl)[8] = rspa.adpcm_table[table_index];
        int i;

        for (i = 0; i < 2; i++) {
            int16_t ins[8];
            int16_t prev1 = out[-1];
            int16_t prev2 = out[-2];
            int j, k;
            if (flags & 4) {
                for (j = 0; j < 2; j++) {
                    ins[j * 4] = (((*in >> 6) << 30) >> 30) << shift;
                    ins[j * 4 + 1] = ((((*in >> 4) & 0x3) << 30) >> 30) << shift;
                    ins[j * 4 + 2] = ((((*in >> 2) & 0x3) << 30) >> 30) << shift;
                    ins[j * 4 + 3] = (((*in++ & 0x3) << 30) >> 30) << shift;
                }
            } else {
                for (j = 0; j < 4; j++) {
                    ins[j * 2] = (((*in >> 4) << 28) >> 28) << shift;
                    ins[j * 2 + 1] = (((*in++ & 0xf) << 28) >> 28) << shift;
                }
            }
            for (j = 0; j < 8; j++) {
                int32_t acc = tbl[0][j] * prev2 + tbl[1][j] * prev1 + (ins[j] << 11);
                for (k = 0; k < j; k++) {
                    acc += tbl[1][((j - k) - 1)] * ins[k];
                }
                acc >>= 11;
                *out++ = clamp16(acc);
            }
        }
        nbytes -= 16 * sizeof(int16_t);
    }
    memcpy(state, out - 16, 16 * sizeof(int16_t));
}

void aResampleImpl(uint8_t flags, uint16_t pitch, RESAMPLE_STATE state) {
    int16_t tmp[16];
    int16_t* in_initial = BUF_S16(rspa.in);
    int16_t* in = in_initial;
    int16_t* out = BUF_S16(rspa.out);
    int nbytes = ROUND_UP_16(rspa.nbytes);
    uint32_t pitch_accumulator;
    int i;
    int16_t* tbl;
    int32_t sample;

    if (flags & A_INIT) {
        memset(tmp, 0, 5 * sizeof(int16_t));
    } else {
        memcpy(tmp, state, 16 * sizeof(int16_t));
    }
    if (flags & 2) {
        memcpy(in - 8, tmp + 8, 8 * sizeof(int16_t));
        in -= tmp[5] / sizeof(int16_t);
    }
    in -= 4;
    pitch_accumulator = (uint16_t) tmp[4];
    memcpy(in, tmp, 4 * sizeof(int16_t));

    do {
        for (i = 0; i < 8; i++) {
            tbl = resample_table[pitch_accumulator * 64 >> 16];
            sample = ((in[0] * tbl[0] + 0x4000) >> 15) + ((in[1] * tbl[1] + 0x4000) >> 15) +
                     ((in[2] * tbl[2] + 0x4000) >> 15) + ((in[3] * tbl[3] + 0x4000) >> 15);
            *out++ = clamp16(sample);

            pitch_accumulator += (pitch << 1);
            in += pitch_accumulator >> 16;
            pitch_accumulator %= 0x10000;
        }
        nbytes -= 8 * sizeof(int16_t);
    } while (nbytes > 0);

    state[4] = (int16_t) pitch_accumulator;
    memcpy(state, in, 4 * sizeof(int16_t));
    i = (in - in_initial + 4) & 7;
    in -= i;
    if (i != 0) {
        i = -8 - i;
    }
    state[5] = i;
    memcpy(state + 8, in, 8 * sizeof(int16_t));
}

void aEnvSetup1Impl(uint8_t initial_vol_wet, uint16_t rate_wet, uint16_t rate_left, uint16_t rate_right/*
                    uint16_t rate_center, uint16_t rate_lfe, uint16_t rate_rear_left, uint16_t rate_rear_right */) {
    rspa.vol_wet = (uint16_t) (initial_vol_wet << 8);
    rspa.rate_wet = rate_wet;
    rspa.rate[0] = rate_left;
    rspa.rate[1] = rate_right;
/*     rspa.rate[2] = rate_center;
    rspa.rate[3] = rate_lfe;
    rspa.rate[4] = rate_rear_left;
    rspa.rate[5] = rate_rear_right; */
}

void aEnvSetup2Impl(uint16_t initial_vol_left, uint16_t initial_vol_right/* , int16_t initial_vol_center,
                    int16_t initial_vol_lfe, int16_t initial_vol_rear_left, int16_t initial_vol_rear_right */) {
    rspa.vol[0] = initial_vol_left;
    rspa.vol[1] = initial_vol_right;
/*     rspa.vol[2] = initial_vol_center;
    rspa.vol[3] = initial_vol_lfe;
    rspa.vol[4] = initial_vol_rear_left;
    rspa.vol[5] = initial_vol_rear_right; */
}

void aEnvMixerImpl(uint16_t in_addr, uint16_t n_samples, bool swap_reverb, bool neg_left, bool neg_right,
                   uint32_t wet_dry_addr, uint32_t haas_temp_addr, uint32_t num_channels, uint32_t cutoff_freq_lfe) {
    // Note: max number of samples is 192 (192 * 2 = 384 bytes = 0x180)
    int max_num_samples = 192;

    int16_t* in = BUF_S16(in_addr);
    int n = ROUND_UP_16(n_samples);
    if (n > max_num_samples) {
        printf("Warning: n_samples is too large: %d\n", n_samples);
    }

    // All speakers
    int dry_addr_start = wet_dry_addr & 0xFFFF;

    int16_t* dry[2];
    for (int i = 0; i < 2; i++) {
        dry[i] = BUF_S16(dry_addr_start + max_num_samples * i * sizeof(int16_t));
    }

    uint16_t vols[2] = { rspa.vol[0], rspa.vol[1]};

        // Account for haas effect
        int haas_addr_left = haas_temp_addr >> 16;
        int haas_addr_right = haas_temp_addr & 0xFFFF;

        if (haas_addr_left) {
            dry[0] = BUF_S16(haas_addr_left);
        } else if (haas_addr_right) {
            dry[1] = BUF_S16(haas_addr_right);
        }

        for (int i = 0; i < n / 8; i++) {
            for (int k = 0; k < 8; k++) {
                int16_t samples[2] = { 0 };

                samples[0] = *in;
                samples[1] = *in;
                in++;

                // Apply volume
                for (int j = 0; j < 2; j++) {
                    samples[j] = (samples[j] * vols[j] >> 16);
                }

                // Mix dry and wet signals
                for (int j = 0; j < 2; j++) {
                    *dry[j] = clamp16(*dry[j] + samples[j]);
                    dry[j]++;
                }
            }

            for (int j = 0; j < 2; j++) {
                vols[j] += rspa.rate[j];
            }
        }
}

void aMixImpl(uint16_t count, int16_t gain, uint16_t in_addr, uint16_t out_addr) {
    int nbytes = ROUND_UP_32(ROUND_DOWN_16(count << 4));
    int16_t* in = BUF_S16(in_addr);
    int16_t* out = BUF_S16(out_addr);
    int i;
    int32_t sample;

    if (gain == -0x8000) {
        while (nbytes > 0) {
            for (i = 0; i < 16; i++) {
                sample = *out - *in++;
                *out++ = clamp16(sample);
            }
            nbytes -= 16 * sizeof(int16_t);
        }
    }

    while (nbytes > 0) {
        for (i = 0; i < 16; i++) {
            sample = ((*out * 0x7fff + *in++ * gain) + 0x4000) >> 15;
            *out++ = clamp16(sample);
        }

        nbytes -= 16 * sizeof(int16_t);
    }
}

void aS8DecImpl(uint8_t flags, ADPCM_STATE state) {
    uint8_t* in = BUF_U8(rspa.in);
    int16_t* out = BUF_S16(rspa.out);
    int nbytes = ROUND_UP_32(rspa.nbytes);
    if (flags & A_INIT) {
        memset(out, 0, 16 * sizeof(int16_t));
    } else if (flags & A_LOOP) {
        memcpy(out, rspa.adpcm_loop_state, 16 * sizeof(int16_t));
    } else {
        memcpy(out, state, 16 * sizeof(int16_t));
    }
    out += 16;

    while (nbytes > 0) {
        *out++ = (int16_t) (*in++ << 8);
        *out++ = (int16_t) (*in++ << 8);
        *out++ = (int16_t) (*in++ << 8);
        *out++ = (int16_t) (*in++ << 8);
        *out++ = (int16_t) (*in++ << 8);
        *out++ = (int16_t) (*in++ << 8);
        *out++ = (int16_t) (*in++ << 8);
        *out++ = (int16_t) (*in++ << 8);
        *out++ = (int16_t) (*in++ << 8);
        *out++ = (int16_t) (*in++ << 8);
        *out++ = (int16_t) (*in++ << 8);
        *out++ = (int16_t) (*in++ << 8);
        *out++ = (int16_t) (*in++ << 8);
        *out++ = (int16_t) (*in++ << 8);
        *out++ = (int16_t) (*in++ << 8);
        *out++ = (int16_t) (*in++ << 8);

        nbytes -= 16 * sizeof(int16_t);
    }

    memcpy(state, out - 16, 16 * sizeof(int16_t));
}

void aAddMixerImpl(uint16_t count, uint16_t in_addr, uint16_t out_addr) {
    int16_t* in = BUF_S16(in_addr);
    int16_t* out = BUF_S16(out_addr);
    int nbytes = ROUND_UP_64(ROUND_DOWN_16(count));

    do {
        *out = clamp16(*out + *in++);
        out++;
        *out = clamp16(*out + *in++);
        out++;
        *out = clamp16(*out + *in++);
        out++;
        *out = clamp16(*out + *in++);
        out++;
        *out = clamp16(*out + *in++);
        out++;
        *out = clamp16(*out + *in++);
        out++;
        *out = clamp16(*out + *in++);
        out++;
        *out = clamp16(*out + *in++);
        out++;
        *out = clamp16(*out + *in++);
        out++;
        *out = clamp16(*out + *in++);
        out++;
        *out = clamp16(*out + *in++);
        out++;
        *out = clamp16(*out + *in++);
        out++;
        *out = clamp16(*out + *in++);
        out++;
        *out = clamp16(*out + *in++);
        out++;
        *out = clamp16(*out + *in++);
        out++;
        *out = clamp16(*out + *in++);
        out++;

        nbytes -= 16 * sizeof(int16_t);
    } while (nbytes > 0);
}

void aDuplicateImpl(uint16_t count, uint16_t in_addr, uint16_t out_addr) {
    uint8_t* in = BUF_U8(in_addr);
    uint8_t* out = BUF_U8(out_addr);

    uint8_t tmp[128];
    memcpy(tmp, in, 128);
    do {
        memcpy(out, tmp, 128);
        out += 128;
    } while (count-- > 0);
}

void aDMEMMove2Impl(uint8_t t, uint16_t in_addr, uint16_t out_addr, uint16_t count) {
    uint8_t* in = BUF_U8(in_addr);
    uint8_t* out = BUF_U8(out_addr);
    int nbytes = ROUND_UP_32(count);

    do {
        memmove(out, in, nbytes);
        in += nbytes;
        out += nbytes;
    } while (t-- > 0);
}

void aResampleZohImpl(uint16_t pitch, uint16_t start_fract) {
    int16_t* in = BUF_S16(rspa.in);
    int16_t* out = BUF_S16(rspa.out);
    int nbytes = ROUND_UP_8(rspa.nbytes);
    uint32_t pos = start_fract;
    uint32_t pitch_add = pitch << 2;

    do {
        *out++ = in[pos >> 17];
        pos += pitch_add;
        *out++ = in[pos >> 17];
        pos += pitch_add;
        *out++ = in[pos >> 17];
        pos += pitch_add;
        *out++ = in[pos >> 17];
        pos += pitch_add;

        nbytes -= 4 * sizeof(int16_t);
    } while (nbytes > 0);
}

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

void aUnkCmd3Impl(uint16_t a, uint16_t b, uint16_t c) {
}

void aUnkCmd19Impl(uint8_t f, uint16_t count, uint16_t out_addr, uint16_t in_addr) {
    int nbytes = ROUND_UP_64(count);
    int16_t* in = BUF_S16(in_addr + f);
    int16_t* out = BUF_S16(out_addr);
    int16_t tbl[32];

    memcpy(tbl, in, 32 * sizeof(int16_t));
    do {
        for (int i = 0; i < 32; i++) {
            out[i] = clamp16(out[i] * tbl[i]);
        }
        out += 32;
        nbytes -= 32 * sizeof(int16_t);
    } while (nbytes > 0);
}
#endif