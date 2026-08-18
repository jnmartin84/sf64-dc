#include "n64sys.h"
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>

u32 osMemSize = (4 * 1048576);

#include <stdio.h>
extern GameState gGameState;
extern s32 sCutsceneState;   // title cutscene phase (TitleCsStates, fox_title.h); only valid in GSTATE_TITLE
void Lib_InitPerspective(Gfx** dList) {
    u16 norm;
    // Fog is authored for the stock near=10 and needs it to read correctly. near=1 exists ONLY to keep
    // the close-up Arwing nose from hard-clipping in the take-off / fly-by cinematics. So: near=10 in
    // gameplay (correct fog), and near=1 elsewhere by default (nose safe) — EXCEPT the title's
    // "team running" hallway scene (TITLE_CS_TEAM_RUNNING == 2), a black-fog scene that must stay
    // bright, so it keeps near=10 too. Add more title sCutsceneState values here if any other
    // non-Arwing title scene reads too dark. (short-circuit guards the stale sCutsceneState off-title.)
    int near10 = (gGameState == GSTATE_PLAY) ||
                 (gGameState == GSTATE_TITLE && sCutsceneState == 2 /* TITLE_CS_TEAM_RUNNING */);
    gProjectNear = near10 ? 10.0f : 1.0f;
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
