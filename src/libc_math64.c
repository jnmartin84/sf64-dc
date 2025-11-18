#include "n64sys.h"
#include <math.h>

#ifndef F_PI
#define F_PI        3.1415926f   /* pi             */
#endif


f32 Math_FAtanF(f32 x) {
#if 1
    return shz_atanf(x);
#else
    s32 sector;
    s32 i;
    f32 sq;
    f32 conv = 0.0f;
    f32 z;

    if (x > 1.0f) {
        sector = 1;
        x = shz_fast_invf(x);//1.0f / x;
    } else if (x < -1.0f) {
        sector = -1;
        x = shz_fast_invf(x);//1.0f / x;
    } else {
        sector = 0;
    }

    sq = SQ(x);

    for (z = i = 24; i != 0; i--) {
        float recipdenom =  shz_fast_invf(2.0f * z + 1.0f + conv);
        conv = SQ(z) * sq * recipdenom; // / (2.0f * z + 1.0f + conv);
        z -= 1.0f;
    }

    float recip1pconv = shz_fast_invf(1.0f + conv);

    if (sector > 0) {
        return F_PI_2 - (x * recip1pconv);/// (1.0f + conv));
    } else if (sector < 0) {
        return -F_PI_2 - (x * recip1pconv);/// (1.0f + conv));
    } else {
        return x * recip1pconv;/// (1.0f + conv);
    }
#endif
}
