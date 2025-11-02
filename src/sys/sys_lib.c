#include "n64sys.h"
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>

u32 osMemSize = (4 * 1048576);

s32 Lib_vsPrintf(char* dst, const char* fmt, va_list args) {
    char *retdst = vsprintf(dst, fmt, args);
    //printf("vsprintf: %s\n", retdst);
    return retdst;
}

void Lib_vTable(s32 index, void (**table)(s32, s32), s32 arg0, s32 arg1) {
    void (*func)(s32, s32) = table[index];

    func(arg0, arg1);
}

void Lib_SwapBuffers(u8* buf1, u8* buf2, s32 len) {
    s32 i;
    u8 temp;

    for (i = 0; i < len; i++) {
        temp = buf2[i];
        buf2[i] = buf1[i];
        buf1[i] = temp;
    }
}

void Lib_QuickSort(u8* first, u32 length, u32 size, CompareFunc cFunc) {
    u32 splitIdx;
    u8* last;
    u8* right;
    u8* left;

    while (true) {
        last = first + (length - 1) * size;

        if (length == 2) {
            if (cFunc(first, last) > 0) {
                Lib_SwapBuffers(first, last, size);
            }
            return;
        }
        if (size && size && size) {} //! FAKE: must be here with at least 3 && operands.
        left = first;
        right = last - size;

        while (true) {
            while (cFunc(left, last) < 0) {
                left += size;
            }
            while ((cFunc(right, last) >= 0) && (left < right)) {
                right -= size;
            }
            if (left >= right) {
                break;
            }
            Lib_SwapBuffers(left, right, size);
            left += size;
            right -= size;
        }
        Lib_SwapBuffers(last, left, size);
        splitIdx = (left - first) / size;
        if (length / 2 < splitIdx) {
            if ((length - splitIdx) > 2) {
                Lib_QuickSort(left + size, length - splitIdx - 1, size, cFunc);
            }

            if (splitIdx < 2) {
                return;
            }
            left = first;
            length = splitIdx;
        } else {
            if (splitIdx >= 2) {
                Lib_QuickSort(first, splitIdx, size, cFunc);
            }

            if ((length - splitIdx) <= 2) {
                return;
            }

            first = left + size;
            length -= splitIdx + 1;
        }
    }
}
#include <stdio.h>
void Lib_InitPerspective(Gfx** dList) {
    u16 norm;
    guPerspective(gGfxMtx, &norm, gFovY, (f32) SCREEN_WIDTH / SCREEN_HEIGHT, gProjectNear, gProjectFar, 1.0f);
//    gSPPerspNormalize((*dList)++, norm);
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

float fill_r=0, fill_g=0, fill_b=0;

void Lib_FillScreen(u8 setFill) {
    s32 i;

    gFillScreenColor |= 1;

    u8 r5 = (gFillScreenColor >> 11) & 0x1F;         // 5 bits red
    u8 g5 = (((gFillScreenColor >> 5) & 0x3F) >> 1) & 0x1F;          // 5 bits green
    u8 b5 = gFillScreenColor & 0x1F;                 // 5 bits blue

    fill_r = (float)r5 / 32.0f;
    fill_g = (float)r5 / 32.0f;
    fill_b = (float)r5 / 32.0f;

    if (setFill == 1) {
        if (gFillScreen == 0) {
            if (gFillScreenColor == 1) {
                vid_set_enabled(0);
            } else {
                for (i = 0; i < 3 * SCREEN_WIDTH; i++) {
                    gFillBuffer[i] = gFillScreenColor;
                }
            }
            gFillScreen = 1;
        }
    } else if (gFillScreen == 1) {
        vid_set_enabled(1);
        gFillScreen = 0;
    }
}
