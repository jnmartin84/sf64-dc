#ifndef MACROS_H
#define MACROS_H
#include <stdio.h>
#include "alignment.h"

extern long unsigned int gSegments[16];

#if !MMU_SEGMENTED
extern void* segmented_to_virtual(const void* addr);
#endif

#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240
#define SCREEN_MARGIN 0

#define TIME_IN_SECONDS(x) (x * 30);

#define CLAMP(x, min, max) ((x) < (min) ? (min) : (x) > (max) ? (max) : (x))
#define CLAMP_MAX(x, max) ((x) > (max) ? (max) : (x))
#define CLAMP_MIN(x, min) ((x) < (min) ? (min) : (x))

#define RAND_FLOAT(max) (Rand_ZeroOne()*(max))
#define RAND_INT(max) ((s32)(Rand_ZeroOne()*(max)))
#define RAND_FLOAT_CENTERED(width) ((Rand_ZeroOne()-0.5f)*(width))
#define RAND_DOUBLE_CENTERED(width) ((Rand_ZeroOne()-0.5f)*(width))
#define RAND_RANGE(min, max) (((max) - (min)) * (Rand_ZeroOne() - (min) / ((min) - (max))))

#define RAND_FLOAT_SEEDED(max) (Rand_ZeroOneSeeded()*(max))
#define RAND_INT_SEEDED(max) ((s32)(Rand_ZeroOneSeeded()*(max)))
#define RAND_FLOAT_CENTERED_SEEDED(width) ((Rand_ZeroOneSeeded()-0.5f)*(width))

#if MMU_SEGMENTED
#define SEGMENTED_TO_VIRTUAL(segment) (segment)
#define SEGMENTED_TO_VIRTUAL_JP(segment) (segment)
#else
#define SEGMENTED_TO_VIRTUAL(segment) segmented_to_virtual((segment))
#define SEGMENTED_TO_VIRTUAL_JP(segment) segmented_to_virtual((segment))
#endif

#define ARRAY_COUNT(arr) (s32)(sizeof(arr) / sizeof(arr[0]))
#define ARRAY_COUNTU(arr) (u32)(sizeof(arr) / sizeof(arr[0]))
#define SIGN_OF(x) (((x) > 0) ? 1 : ((x) == 0) ? 0 : -1)
#define SQ(x) ((x) * (x))
#define CUBE(x) ((x) * (x) * (x))

#include "sh4zam.h"

#define DOT_XYZ(v1Ptr, v2Ptr) shz_dot6f((f32)((v1Ptr)->x), (f32)((v1Ptr)->y), (f32)((v1Ptr)->z), (f32)((v2Ptr)->x), (f32)((v2Ptr)->y), (f32)((v2Ptr)->z))
#define VEC3F_MAG(vecPtr) shz_sqrtf_fsrra(DOT_XYZ(vecPtr, vecPtr))

#define ABS(x) ((x) >= 0 ? (x) : -(x))
#define ABSF(x) ((x) >= 0.0f ? (x) : -(x))
#define ROUND(float) ((s32)((float)+0.5f))

#ifndef F_PI
#define F_PI        3.1415926f   /* pi             */
#define F_PI_2      1.57079633f /* pi / 2*/
#define F_PI_4      0.78539816f /* pi / 2*/
#define F_3PI_2     4.71238898f
#define F_3PI_4     2.35619449f
#define F_2PI       6.28318531f
#define F_PI_18     0.17453293f
#endif

#define RAD_TO_DEG(radians) (M_RTOD*(radians))
#define DEG_TO_RAD(degrees) (M_DTOR*(degrees))

#define SIN_DEG(angle) sinf((M_DTOR)*(angle))
#define COS_DEG(angle) cosf((M_DTOR)*(angle))

#define USEC_TO_CYCLES(n) (((u64)(n)*(osClockRate/15625LL))/(1000000LL/15625LL))
#define MSEC_TO_CYCLES(n) (USEC_TO_CYCLES((n) * 1000LL))

#define CYCLES_TO_USEC(c) (((u64)(c)*(1000000LL/15625LL))/(osClockRate/15625LL))
#define CYCLES_TO_MSEC(c) ((s32)CYCLES_TO_USEC(c)/1000)

#define UNPACK_BYTE(data, bytenum) (((data) & (0xFF << ((bytenum) * 8))) >> ((bytenum) * 8))

/*
 * Macros for libultra
 */

#if defined(__sgi)
#define PRINTF
#else
#define PRINTF(...)
#endif

#define ALIGNED(x) __attribute__((aligned(x)))
#define ARRLEN(x) ((s32)(sizeof(x) / sizeof(x[0])))
#define STUBBED_PRINTF(x) ((void)(x))
#define UNUSED __attribute__((unused))

#ifndef __GNUC__
#define __attribute__(x)
#endif

#endif // MACROS_H
