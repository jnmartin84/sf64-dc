#include "n64sys.h"
#include <math.h>

f32 Math_FAtanF(f32 x) {
    s32 sector;
    s32 i;
    f32 sq;
    f32 conv = 0.0f;
    f32 z;

    if (x > 1.0f) {
        sector = 1;
        x = shz_invf(x);
    } else if (x < -1.0f) {
        sector = -1;
        x = shz_invf(x);
    } else {
        sector = 0;
    }

    sq = SQ(x);

    for (z = i = 24; i != 0; i--) {
        float recipdenom = shz_invf(2.0f * z + 1.0f + conv);
        conv = SQ(z) * sq * recipdenom;
        z -= 1.0f;
    }

    float recip1pconv = shz_invf(1.0f + conv);

    if (sector > 0) {
        return F_PI_2 - (x * recip1pconv);
    } else if (sector < 0) {
        return -F_PI_2 - (x * recip1pconv);
    } else {
        return x * recip1pconv;
    }
}
