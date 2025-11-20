#include "n64sys.h"
#include "prevent_bss_reordering.h"
#include <math.h>
#include "sh4zam.h"
s32 sSeededRandSeed3;
s32 sRandSeed1;
s32 sRandSeed2;
s32 sRandSeed3;
s32 sSeededRandSeed1;
s32 sSeededRandSeed2;

f32 Math_ModF(f32 value, f32 mod) {
    return value - ((s32) (value * shz_fast_invf(mod)) * mod);
}
extern volatile OSTime osGetTime(void);
void Rand_Init(void) {
    sRandSeed1 = (s32) osGetTime() % 30000;
    sRandSeed2 = (s32) osGetTime() % 30000;
    sRandSeed3 = (s32) osGetTime() % 30000;
}

#define recip30269 0.00003304f
#define recip30307 0.000033f
#define recip30323 0.00003298f

f32 Rand_ZeroOne(void) {
//    if ((sRandSeed1 + sRandSeed2 + sRandSeed3) == 0){
//        Rand_Init();
//    }
//    sRandSeed1 = (sRandSeed1 * 171) % 30269;
//    sRandSeed2 = (sRandSeed2 * 172) % 30307;
//    sRandSeed3 = (sRandSeed3 * 170) % 30323;
    const float f30269 = 30269.0f;
    const float f30307 = 30307.0f;
    const float f30323 = 30323.0f;

    f32 sr1 = (float)sRandSeed1;
    f32 sr2 = (float)sRandSeed2;
    f32 sr3 = (float)sRandSeed3;

    sr1 = Math_ModF((sr1 * 171.0f), f30269);
    sr2 = Math_ModF((sr2 * 172.0f), f30307);
    sr3 = Math_ModF((sr3 * 170.0f), f30323);

    sRandSeed1 = (s32)sr1;
    sRandSeed2 = (s32)sr2;
    sRandSeed3 = (s32)sr3;

//    return fabsf(Math_ModF(shz_divf(sr1, f30269) + shz_divf(sr2, f30307) + shz_divf(sr3, f30323), 1.0f));
    return fabsf(Math_ModF(shz_dot6f(sr1, sr2, sr3, recip30269, recip30307, recip30323), 1.0f));
}

void Rand_SetSeed(s32 seed1, s32 seed2, s32 seed3) {
    sSeededRandSeed1 = seed1;
    sSeededRandSeed2 = seed2;
    sSeededRandSeed3 = seed3;
}

f32 Rand_ZeroOneSeeded(void) {
#if 0
    sSeededRandSeed1 = (sSeededRandSeed1 * 171) % 30269;
    sSeededRandSeed2 = (sSeededRandSeed2 * 172) % 30307;
    sSeededRandSeed3 = (sSeededRandSeed3 * 170) % 30323;

    return fabsf(
        Math_ModF(((float)sSeededRandSeed1 * recip30269) + ((float)sSeededRandSeed2 * recip30307) + ((float)sSeededRandSeed3 * recip30323), 1.0f));
#endif
    const float f30269 = 30269.0f;
    const float f30307 = 30307.0f;
    const float f30323 = 30323.0f;

    f32 sr1 = (float)sSeededRandSeed1;
    f32 sr2 = (float)sSeededRandSeed2;
    f32 sr3 = (float)sSeededRandSeed3;

    sr1 = Math_ModF((sr1 * 171.0f), f30269);
    sr2 = Math_ModF((sr2 * 172.0f), f30307);
    sr3 = Math_ModF((sr3 * 170.0f), f30323);

    sSeededRandSeed1 = (s32)sr1;
    sSeededRandSeed2 = (s32)sr2;
    sSeededRandSeed3 = (s32)sr3;

//    return fabsf(Math_ModF(shz_divf(sr1, f30269) + shz_divf(sr2, f30307) + shz_divf(sr3, f30323), 1.0f));
    return fabsf(Math_ModF(shz_dot6f(sr1, sr2, sr3, recip30269, recip30307, recip30323), 1.0f));
}

#define F_PI_2      1.57079633f   /* pi/2           */
#define F_PI_4      0.78539816f  /* pi/4           */
#define F_1_PI      0.31830989f  /* 1/pi           */
#define F_2_PI      0.63661977f  /* 2/pi           */

// only works for positive x


// branch-free, division-free atan2f approximation
// copysignf has a branch but penalty-free
f32 Math_Atan2F(f32 y, f32 x) {
#if 1
    if (y == 0.0f && x == 0.0f) {
        return 0.0f;
    }

    float abs_y = fabsf(y);
	float absy_plus_absx = abs_y + fabsf(x);
	float inv_absy_plus_absx = shz_fast_invf(absy_plus_absx);
	float angle = F_PI_2 - shz_copysignf(F_PI_4, x);
	float r = (x - shz_copysignf(abs_y, x)) * inv_absy_plus_absx;
	angle += (0.1963f * r * r - 0.9817f) * r;
	return shz_copysignf(angle, y);
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

    float recipx = shz_fast_invf(x);

    if (x < 0.0f) {
        if (y < 0.0f) {
            return -(F_PI - Math_FAtanF(fabsf(y * recipx)));
        } else {
            return F_PI - Math_FAtanF(fabsf(y * recipx));
        }
    } else {
        return Math_FAtanF(y * recipx);
    }
#endif
}

f32 Math_Atan2F_XY(f32 x, f32 y) {
    if ((x == 0.0f) && (y == 0.0f)) {
        return 0.0f;
    }

    if (y == 0.0f) {
        if (x > 0.0f) {
            return 0.0f;
        } else {
            return F_PI;
        }
    }

    if (x == 0.0f) {
        if (y < 0.0f) {
            return -F_PI_2;
        } else {
            return F_PI_2;
        }
    }

    float xovery = shz_divf(x,y);// x * shz_fast_invf(y);

    if (x < 0.0f) {
        float res = F_PI - Math_FAtanF(fabsf(xovery));
        if (y < 0.0f) {
            return -res;
        } else {
            return res;
        }
    } else {
        return Math_FAtanF(xovery);
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
    float recipy = shz_fast_invf(y);


    return -Math_FAtanF(x * recipy);
}

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
