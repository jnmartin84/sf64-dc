#include "n64sys.h"
#include "prevent_bss_reordering.h"
#include <math.h>

s32 sSeededRandSeed3;
s32 sRandSeed1;
s32 sRandSeed2;
s32 sRandSeed3;
s32 sSeededRandSeed1;
s32 sSeededRandSeed2;

f32 Math_ModF(f32 value, f32 mod) {
    return fmodf(value, mod);
//        return value - ((s32) (value / mod) * mod);
}
extern volatile OSTime osGetTime(void);
void Rand_Init(void) {
    sRandSeed1 = (s32) osGetTime() % 30000;
    sRandSeed2 = (s32) osGetTime() % 30000;
    sRandSeed3 = (s32) osGetTime() % 30000;
}

f32 Rand_ZeroOne(void) {
    if ((sRandSeed1 + sRandSeed2 + sRandSeed3) == 0){
        Rand_Init();
    }
    
    sRandSeed1 = (sRandSeed1 * 171) % 30269;
    sRandSeed2 = (sRandSeed2 * 172) % 30307;
    sRandSeed3 = (sRandSeed3 * 170) % 30323;

    return fabsf(Math_ModF(((float)sRandSeed1 / 30269.0f) + ((float)sRandSeed2 / 30307.0f) + ((float)sRandSeed3 / 30323.0f), 1.0f));
}

void Rand_SetSeed(s32 seed1, s32 seed2, s32 seed3) {
    sSeededRandSeed1 = seed1;
    sSeededRandSeed2 = seed2;
    sSeededRandSeed3 = seed3;
}

f32 Rand_ZeroOneSeeded(void) {
    sSeededRandSeed1 = (sSeededRandSeed1 * 171) % 30269;
    sSeededRandSeed2 = (sSeededRandSeed2 * 172) % 30307;
    sSeededRandSeed3 = (sSeededRandSeed3 * 170) % 30323;

    return fabsf(
        Math_ModF(((float)sSeededRandSeed1 / 30269.0f) + ((float)sSeededRandSeed2 / 30307.0f) + ((float)sSeededRandSeed3 / 30323.0f), 1.0f));
}

#define F_PI        3.14159265f   /* pi             */
#define F_PI_2      1.57079633f   /* pi/2           */
#define F_PI_4      0.78539816f  /* pi/4           */
#define F_1_PI      0.31830989f  /* 1/pi           */
#define F_2_PI      0.63661977f  /* 2/pi           */

// only works for positive x
#define approx_recip(x) (1.0f / sqrtf((x)*(x)))

#define approx_signed_recip(x) (((x) < 0.0f) ? -(1.0f / sqrtf((x)*(x))) : (1.0f / sqrtf((x)*(x))))


// branch-free, division-free atan2f approximation
// copysignf has a branch but penalty-free
f32 Math_Atan2F(f32 y, f32 x) {
#if 1
	float abs_y = fabsf(y) + 1e-10f;
	float absy_plus_absx = abs_y + fabsf(x);
	float inv_absy_plus_absx = approx_recip(absy_plus_absx);
	float angle = F_PI_2 - copysignf(F_PI_4, x);
	float r = (x - copysignf(abs_y, x)) * inv_absy_plus_absx;
	angle += (0.1963f * r * r - 0.9817f) * r;
	return copysignf(angle, y);
#else
    if ((y == 0.0f) && (x == 0.0f)) {
        return 0.0f;
    }

    if (x == 0.0f) {
        if (y < 0.0f) {
            return -F_PI_2;
        } else {
            return F_PI_2;
        }
    }

    if (x < 0.0f) {
        if (y < 0.0f) {
            return -(F_PI - Math_FAtanF(fabsf(y / x)));
        } else {
            return F_PI - Math_FAtanF(fabsf(y / x));
        }
    } else {
        return Math_FAtanF(y / x);
    }
#endif
}

f32 Math_Atan2F_XY(f32 x, f32 y) {

    float recipy = approx_signed_recip(y);

    if ((x == 0.0f) && (y == 0.0f)) {
        return 0.0f;
    }

    if (x == 0.0f) {
        if (y < 0.0f) {
            return -F_PI_2;
        } else {
            return F_PI_2;
        }
    }

    if (y == 0.0f) {
        if (x > 0.0f) {
            return 0.0f;
        } else {
            return F_PI;
        }
    }

    if (x < 0.0f) {
        if (y < 0.0f) {
            return -(F_PI - Math_FAtanF(fabsf(x * recipy /* / y */)));
        } else {
            return F_PI - Math_FAtanF(fabsf(x * recipy));
        }
    } else {
        return Math_FAtanF(x *recipy);//// y);
    }
}

f32 Math_Atan2F_XYAlt(f32 x, f32 y) {
    if ((x == 0.0f) && (y == 0.0f)) {
        return 0.0f;
    }

    if (x == 0.0f) {
        if (y < 0.0f) {
            return -F_PI_2;
        }
        return F_PI_2;
    }

    if (y == 0.0f) {
        return 0.0f;
    }
    float recipy = approx_signed_recip(y);


    return -Math_FAtanF(x * recipy);
}

#if 0
f32 Math_FactorialF(f32 n) {
    f32 out = 1.0f;
    s32 i;

    for (i = (s32) n; i > 1; i--) {
        out *= i;
    }

    return out;
}

f32 D_800C45E0[] = { 1.0f, 1.0f, 2.0f, 6.0f, 
    24.0f, 120.0f, 720.0f, 5040.0f, 40320.0f, 
    362880.0f, 3628800.0f, 39916800.0f, 479001600.0f };

f32 Math_Factorial(s32 n) {
    f32 out;
    s32 i;

    if (n > 12) {
        out = 1.0f;

        for (i = n; i > 1; i--) {
            out *= i;
        }
    } else {
        out = D_800C45E0[n];
    }
    return out;
}

f32 Math_PowF(f32 base, s32 exp) {
    f32 out = 1.0f;

    while (exp > 0) {
        exp--;
        out *= base;
    }
    return out;
}
#endif

void Math_MinMax(s32* min, s32* max, s32 val1, s32 val2, s32 val3) {
    if (val1 < val2) {
        if (val2 < val3) {
            *min = val1;
            *max = val3;
            return;
        }
        *max = val2;

        if (val1 < val3) {
            *min = val1;
            return;
        }
        *min = val3;
        return;
    }

    if (val1 < val3) {
        *min = val2;
        *max = val3;
        return;
    }
    *max = val1;

    if (val2 < val3) {
        *min = val2;
        return;
    }
    *min = val3;
}
