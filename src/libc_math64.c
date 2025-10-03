#include "n64sys.h"
#include <math.h>

#ifndef F_PI
#define F_PI        3.1415926f   /* pi             */
#endif

#define approx_recip(x) (1.0f / sqrtf((x)*(x)))
#define approx_signed_recip(x) (((x) < 0.0f) ? -(1.0f / sqrtf((x)*(x))) : (1.0f / sqrtf((x)*(x))))

f32 Math_FAtanF(f32 x) {
    s32 sector;
    s32 i;
    f32 sq;
    f32 conv = 0.0f;
    f32 z;

    if (x > 1.0f) {
        sector = 1;
        x = approx_recip(x);//1.0f / x;
    } else if (x < -1.0f) {
        sector = -1;
        x = -(approx_recip(x));//1.0f / x;
    } else {
        sector = 0;
    }

    sq = SQ(x);

    for (z = i = 24; i != 0; i--) {
        float recipdenom =  approx_signed_recip(2.0f * z + 1.0f + conv);
        conv = SQ(z) * sq * recipdenom; // / (2.0f * z + 1.0f + conv);
        z -= 1.0f;
    }

    float recip1pconv = approx_signed_recip(1.0f + conv);

    if (sector > 0) {
        return F_PI_2 - (x * recip1pconv);/// (1.0f + conv));
    } else if (sector < 0) {
        return -F_PI_2 - (x * recip1pconv);/// (1.0f + conv));
    } else {
        return x * recip1pconv;/// (1.0f + conv);
    }
}
#if 0

f32 Math_FAtan2F(f32 y, f32 x) {
    if ((y == 0.0f) && (x == 0.0f)) {
        return 0.0f;
    }
    if (x == 0.0f) {
        if (y < 0.0f) {
            return -F_PI / 2.0f;
        }
        return F_PI / 2.0f;
    }
    if (x < 0.0f) {
        if (y < 0.0f) {
            return -(F_PI - Math_FAtanF(fabsf(y / x)));
        }
        return F_PI - Math_FAtanF(fabsf(y / x));
    }
    return Math_FAtanF(y / x);
}

f32 Math_FAsinF(f32 x) {
    return Math_FAtan2F(x, sqrtf(1.0f - SQ(x)));
}

f32 Math_FAcosF(f32 x) {
    return F_PI / 2.0f - Math_FAsinF(x);
}
#endif
