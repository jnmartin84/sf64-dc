#include "PR/ultratypes.h"
#include "PR/gbi.h"
#include "PR/gu.h"
#include <math.h>
#include <stdint.h>
#include "sh4zam.h"
void guLookAtF(float mf[4][4], float xEye, float yEye, float zEye, float xAt, float yAt, float zAt, float xUp,
               float yUp, float zUp) {
    float len, xLook, yLook, zLook, xRight, yRight, zRight;

    xLook = xAt - xEye;
    yLook = yAt - yEye;
    zLook = zAt - zEye;

    /* Negate because positive Z is behind us: */
    shz_vec3_t lookOut = shz_vec3_normalize((shz_vec3_t){xLook,yLook,zLook});
    xLook = -lookOut.x;
    yLook = -lookOut.y;
    zLook = -lookOut.z;

    /* Right = Up x Look */

    xRight = yUp * zLook - zUp * yLook;
    yRight = zUp * xLook - xUp * zLook;
    zRight = xUp * yLook - yUp * xLook;

    shz_vec3_t rightOut = shz_vec3_normalize((shz_vec3_t){xRight,yRight,zRight});
    xRight = rightOut.x;
    yRight = rightOut.y;
    zRight = rightOut.z;

    /* Up = Look x Right */

    xUp = yLook * zRight - zLook * yRight;
    yUp = zLook * xRight - xLook * zRight;
    zUp = xLook * yRight - yLook * xRight;

    shz_vec3_t upOut = shz_vec3_normalize((shz_vec3_t){xUp,yUp,zUp});
    xUp = upOut.x;
    yUp = upOut.y;
    zUp = upOut.z;

    mf[0][0] = xRight;
    mf[1][0] = yRight;
    mf[2][0] = zRight;
    mf[3][0] = -(xEye * xRight + yEye * yRight + zEye * zRight);

    mf[0][1] = xUp;
    mf[1][1] = yUp;
    mf[2][1] = zUp;
    mf[3][1] = -(xEye * xUp + yEye * yUp + zEye * zUp);

    mf[0][2] = xLook;
    mf[1][2] = yLook;
    mf[2][2] = zLook;
    mf[3][2] = -(xEye * xLook + yEye * yLook + zEye * zLook);

    mf[0][3] = 0.0f;
    mf[1][3] = 0.0f;
    mf[2][3] = 0.0f;
    mf[3][3] = 1.0f;
}

#ifndef GBI_FLOATS
void guLookAt(Mtx* m, float xEye, float yEye, float zEye, float xAt, float yAt, float zAt, float xUp, float yUp,
              float zUp) {
    float mf[4][4];

    guLookAtF(mf, xEye, yEye, zEye, xAt, yAt, zAt, xUp, yUp, zUp);

    guMtxF2L(mf, m);
}
#else
void guLookAt(Mtx* m, float xEye, float yEye, float zEye, float xAt, float yAt, float zAt, float xUp, float yUp,
              float zUp) {
    guLookAtF(m->m, xEye, yEye, zEye, xAt, yAt, zAt, xUp, yUp, zUp);
}
#endif