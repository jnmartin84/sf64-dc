
/**************************************************************************
 *									  *
 *		 Copyright (C) 1994, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#include "PR/ultratypes.h"
#include "PR/guint.h"
#include <math.h>
#include <stdint.h>
#include "sh4zam.h"
void guPerspectiveF(float mf[4][4], u16* perspNorm, float fovy, float aspect, float near, float far, float scale) {
    float yscale;
    int row;
    int col;
    f32 recip_aspect = shz_fast_invf(aspect);
    f32 recip_nsubf = shz_fast_invf(near - far);

    guMtxIdentF(mf);
    fovy *= 0.01745329f;// 3.1415926f / 180.0f;
    yscale = cosf(fovy * 0.5f) / sinf(fovy * 0.5f);
    mf[0][0] = yscale * recip_aspect;
    mf[1][1] = yscale;
    mf[2][2] = (near + far)  * recip_nsubf;
    mf[2][3] = -1.0f;
    mf[3][2] = 2.0f * near * far  * recip_nsubf;
    mf[3][3] = 0.0f;
    for (row = 0; row < 4; row++) {
        for (col = 0; col < 4; col++) {
            mf[row][col] *= scale;
        }
    }
#if 0
    if (perspNorm != NULL) {
        if (near + far <= 2.0f) {
            *perspNorm = 65535;
        } else {
            *perspNorm = (float) (1 << 17) / (near + far);
            if (*perspNorm <= 0) {
                *perspNorm = 1;
            }
        }
    }
#endif
}

void guPerspective(Mtx* m, u16* perspNorm, float fovy, float aspect, float near, float far, float scale) {
#ifndef GBI_FLOATS
    float mat[4][4];
    guPerspectiveF(mat, perspNorm, fovy, aspect, near, far, scale);
    guMtxF2L(mat, m);
#else
    guPerspectiveF(m->m, perspNorm, fovy, aspect, near, far, scale);
#endif
}