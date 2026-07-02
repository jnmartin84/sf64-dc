#include "n64sys.h"
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>

u32 osMemSize = (4 * 1048576);

#include <stdio.h>
void Lib_InitPerspective(Gfx** dList) {
    u16 norm;
    guPerspective(gGfxMtx, &norm, gFovY, (f32) SCREEN_WIDTH / SCREEN_HEIGHT, gProjectNear, gProjectFar, 1.0f);
    gSPMatrix((*dList)++, gGfxMtx++, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
//    guLookAt(gGfxMtx, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -12800.0f, 0.0f, 1.0f, 0.0f);
//    gSPMatrix((*dList)++, gGfxMtx++, G_MTX_NOPUSH | G_MTX_MUL | G_MTX_PROJECTION);
    Matrix_Copy(gGfxMatrix, &gIdentityMatrix);
}

void Lib_InitOrtho(Gfx** dList) {
    guOrtho(gGfxMtx, -SCREEN_WIDTH * 0.5f, SCREEN_WIDTH * 0.5f, -SCREEN_HEIGHT * 0.5f, SCREEN_HEIGHT * 0.5f, gProjectNear,
            gProjectFar, 1.0f);
    gSPMatrix((*dList)++, gGfxMtx++, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
//    guLookAt(gGfxMtx, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -12800.0f, 0.0f, 1.0f, 0.0f);
//    gSPMatrix((*dList)++, gGfxMtx++, G_MTX_NOPUSH | G_MTX_MUL | G_MTX_PROJECTION);
    Matrix_Copy(gGfxMatrix, &gIdentityMatrix);
}

#undef bool
#include <kos.h>

void Lib_FillScreen(u8 setFill) {
    s32 i;

    gFillScreenColor |= 1;

    if (setFill == 1) {
        if (gFillScreen == 0) {
            if (gFillScreenColor == 1) {
                vid_set_enabled(0);
            }
            gFillScreen = 1;
        }
    } else if (gFillScreen == 1) {
        vid_set_enabled(1);
        gFillScreen = 0;
    }
}
