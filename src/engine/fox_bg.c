#include "global.h"
#include "assets/ast_katina.h"
#include "assets/ast_venom_1.h"
#include "assets/ast_venom_2.h"
#include "assets/ast_fortuna.h"
#include "assets/ast_bg_planet.h"
#include "assets/ast_versus.h"
#include "assets/ast_corneria.h"
#include "assets/ast_meteo.h"
#include "assets/ast_training.h"
#include "assets/ast_sector_x.h"
#include "assets/ast_sector_y.h"
#include "assets/ast_sector_z.h"
#include "assets/ast_aquas.h"
#include "assets/ast_bolse.h"
#include "assets/ast_titania.h"
#include "assets/ast_macbeth.h"
#include "assets/ast_andross.h"
#include "assets/ast_solar.h"
#include "assets/ast_warp_zone.h"
#include "assets/ast_area_6.h"
#include "assets/ast_zoness.h"

#include "prevent_bss_reordering2.h"
// #include "prevent_bss_reordering3.h"

f32 gWarpZoneBgAlpha;
u8 gDrawAquasSurfaceWater;
f32 D_bg_8015F968; // heat shimmer effect for SO and TI?
f32 D_bg_8015F96C; // unused. some sort of starfield motion blur for meteo?
f32 gSurfaceWaterYPos;

// RGBA during the level complete cutscene
s32 gAquasSurfaceColorR;
s32 gAquasSurfaceColorG;
s32 gAquasSurfaceColorB;
s32 gAquasSurfaceAlpha2;

f32 gArea6BackdropScale;
UNK_TYPE D_bg_8015F988[0x683]; // Unused? Close to being [13][0x80]
f32 sAndrossUnkBrightness;

f32 gAndrossUnkAlpha = 0.0f;
u16 gBolseDynamicGround = true;
s32 D_bg_800C9C38 = 0; // unused?
u16 gStarColors[16] = {
    0x108B, 0x108B, 0x1087, 0x1089, 0x39FF, 0x190D, 0x108B, 0x1089,
    0x294B, 0x18DF, 0x294B, 0x1085, 0x39FF, 0x108B, 0x18CD, 0x108B,
};
Gfx* sSunDLs[13] = {
    aBallDL, aBallDL, aBallDL, aBallDL, aBallDL, D_BG_PLANET_20112C0,
    aBallDL, aBallDL, aBallDL, aBallDL, aBallDL, D_BG_PLANET_20112C0,
    aBallDL,
};
Gfx* sKaSunDLs[13] = {
    aBallDL, aBallDL, aBallDL, aBallDL, aBallDL, aBallDL, aBallDL, aBallDL, aBallDL, aBallDL, aBallDL, aBallDL, aBallDL,
};
f32 sSunShifts[13] = {
    0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 10.0f, 13.0f, 20.0f, 35.0f, 40.0f, 50.0f, 50.0f, 70.0f,
};
f32 sKaSunShifts[13] = {
    0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 5.0f, 10.0f, 16.0f, 20.0f, 30.0f, 45.0f, 60.0f, 70.0f,
};
f32 sSunScales[13] = {
    0.7f, 1.0f, 1.2f, 1.4f, 1.8f, 2.0f, 0.4f, 0.6f, 0.8f, 1.7f, 0.8f, 4.0f, 2.0f,
};
f32 sKaSunScales[13] = {
    0.525f, 0.75f, 0.90000004f, 1.05f, 1.3499999f, 0.15f, 0.25f, 0.35f, 1.0f, 0.6f, 0.35f, 0.9f, 1.0f,
};
Color_RGB8 sSunColors[13] = {
    { 255, 255, 255 }, { 255, 255, 192 }, { 255, 255, 128 }, { 255, 255, 96 }, { 255, 255, 64 },
    { 255, 255, 64 },  { 255, 255, 64 },  { 255, 255, 64 },  { 255, 255, 64 }, { 255, 255, 64 },
    { 255, 255, 64 },  { 255, 255, 64 },  { 255, 255, 64 },
};
Color_RGB8 sKaSunColors[13] = {
    { 255, 128, 64 },  { 255, 128, 64 }, { 255, 128, 64 }, { 255, 128, 64 }, { 255, 128, 64 },
    { 255, 255, 64 },  { 255, 128, 64 }, { 255, 128, 64 }, { 255, 255, 64 }, { 128, 128, 128 },
    { 128, 128, 255 }, { 255, 255, 64 }, { 255, 128, 64 },
};
s32 sSunAlphas[13] = {
    255, 80, 64, 48, 32, 12, 32, 44, 32, 42, 36, 12, 38,
};
s32 sKaSunAlphas[13] = {
    80, 60, 40, 20, 10, 20, 30, 20, 15, 30, 20, 27, 20,
};
s32 sSunGlareAlphaStep[2] = { 10, 4 };
s32 sSunGlareMaxAlpha[2] = { 140, 40 };
f32 sLensFlareAlphaMod[2] = { 1.2f, 0.5f };
f32 sGroundPositions360x[4] = {
    6000.0f,
    -6000.0f,
    6000.0f,
    -6000.0f,
};
f32 sGroundPositions360z[4] = {
    6000.0f,
    6000.0f,
    -6000.0f,
    -6000.0f,
};



#define gSPStarfield(pkt)                                       \
    {                                                                                   \
        Gfx* _g = (Gfx*) (pkt);                                                         \
                                                                                        \
        _g->words.w0 = 0x424C4E44; \
        _g->words.w1 = 0x46554370;                                           \
    }
#define LOWRES 0
void Background_DrawStarfield(void) {
    f32 by;
    f32 bx;
    s16 vy;
    s16 vx;
    s32 i;
    s32 starCount;
    f32 zCos;
    f32 zSin;
    f32 xField;
    f32 yField;
    f32* xStar;
    f32* yStar;
    u32* color;
    gSPStarfield(gMasterDisp++);
    //gDPPipeSync(gMasterDisp++);
    gDPSetCycleType(gMasterDisp++, G_CYC_FILL);
    gDPSetCombineMode(gMasterDisp++, G_CC_SHADE, G_CC_SHADE);
    gDPSetRenderMode(gMasterDisp++, G_RM_OPA_SURF, G_RM_OPA_SURF2);

    starCount = gStarCount;
    if (starCount != 0) {
        if (gStarfieldX >= 1.5f * SCREEN_WIDTH) {
            gStarfieldX -= 1.5f * SCREEN_WIDTH;
        }
        if (gStarfieldY >= 1.5f * SCREEN_HEIGHT) {
            gStarfieldY -= 1.5f * SCREEN_HEIGHT;
        }
        if (gStarfieldX < 0.0f) {
            gStarfieldX += 1.5f * SCREEN_WIDTH;
        }
        if (gStarfieldY < 0.0f) {
            gStarfieldY += 1.5f * SCREEN_HEIGHT;
        }
        xField = gStarfieldX;
        yField = gStarfieldY;

        xStar = &gStarOffsetsX[1];
        yStar = &gStarOffsetsY[1];
        color = &gStarFillColors[0];

        if (gGameState != GSTATE_PLAY) {
            starCount = 1000;
        }

        zCos = cosf(gStarfieldRoll);
        zSin = sinf(gStarfieldRoll);

//        for (i = 0; i < starCount; i++, yStar++, xStar++, color++) {
        for (i = 0; i < starCount; i+=2, yStar+=2, xStar+=2, color+=2) {
            bx = *xStar + xField;
            by = *yStar + yField;
            if (bx >= 1.25f * SCREEN_WIDTH) {
                bx -= 1.5f * SCREEN_WIDTH;
            }
            bx -= SCREEN_WIDTH / 2.0f;

            if (by >= 1.25f * SCREEN_HEIGHT) {
                by -= 1.5f * SCREEN_HEIGHT;
            }
            by -= SCREEN_HEIGHT / 2.0f;

            vx = (zCos * bx) + (zSin * by) + SCREEN_WIDTH / 2.0f;
            vy = (-zSin * bx) + (zCos * by) + SCREEN_HEIGHT / 2.0f;
            if ((vx >= 0) && (vx < SCREEN_WIDTH-1) && (vy > 0) && (vy < SCREEN_HEIGHT-1)) {
                //gDPPipeSync(gMasterDisp++);
                gDPSetFillColor(gMasterDisp++, *color);
#if LOWRES
                gDPFillRectangle(gMasterDisp++, vx, vy, vx+1, vy+1);
#else
                gDPFillRectangle(gMasterDisp++, vx, vy, vx, vy);
#endif
            }
        }
    }
    //gDPPipeSync(gMasterDisp++);
    gDPSetColorDither(gMasterDisp++, G_CD_MAGICSQ);
    gSPStarfield(gMasterDisp++);
}

void Background_DrawPartialStarfield(s32 yMin, s32 yMax) {
    f32 by;
    f32 bx;
    s16 vy;
    s16 vx;
    s32 i;
    s32 var_s2;
    f32 cos;
    f32 sin;
    f32 spf68;
    f32 spf64;
    f32* sp60;
    f32* sp5C;
    u32* sp58;
    gSPStarfield(gMasterDisp++);

    //gDPPipeSync(gMasterDisp++);
    gDPSetCycleType(gMasterDisp++, G_CYC_FILL);
    gDPSetCombineMode(gMasterDisp++, G_CC_SHADE, G_CC_SHADE);
    gDPSetRenderMode(gMasterDisp++, G_RM_OPA_SURF, G_RM_OPA_SURF2);

    if (gStarfieldX >= 1.5f * SCREEN_WIDTH) {
        gStarfieldX -= 1.5f * SCREEN_WIDTH;
    }
    if (gStarfieldY >= 1.5f * SCREEN_HEIGHT) {
        gStarfieldY -= 1.5f * SCREEN_HEIGHT;
    }
    if (gStarfieldX < 0.0f) {
        gStarfieldX += 1.5f * SCREEN_WIDTH;
    }
    if (gStarfieldY < 0.0f) {
        gStarfieldY += 1.5f * SCREEN_HEIGHT;
    }

    spf68 = gStarfieldX;
    spf64 = gStarfieldY;

    sp60 = &gStarOffsetsX[1];
    sp5C = &gStarOffsetsY[1];
    sp58 = &gStarFillColors[0];
    var_s2 = 500;

    cos = cosf(gStarfieldRoll);
    sin = sinf(gStarfieldRoll);

//    for (i = 0; i < var_s2; i++, sp5C++, sp60++, sp58++) {
    for (i = 0; i < var_s2; i+=2, sp5C+=2, sp60+=2, sp58+=2) {
        bx = *sp60 + spf68;
        by = *sp5C + spf64;
        if (bx >= 1.25f * SCREEN_WIDTH) {
             bx -= 1.5f * SCREEN_WIDTH;
        }
        bx -= SCREEN_WIDTH / 2.0f;

        if (by >= 1.25f * SCREEN_HEIGHT) {
            by -= 1.5f * SCREEN_HEIGHT;
        }
        by -= SCREEN_HEIGHT / 2.0f;

        vx = (cos * bx) + (sin * by) + SCREEN_WIDTH / 2.0f;
        vy = (-sin * bx) + (cos * by) + SCREEN_HEIGHT / 2.0f;
        if ((vx >= 0) && (vx < SCREEN_WIDTH-1) && (yMin < vy) && (vy < yMax-1)) {
            //gDPPipeSync(gMasterDisp++);
            gDPSetFillColor(gMasterDisp++, *sp58);
            gDPFillRectangle(gMasterDisp++, vx, vy, vx, vy);
        }

    }
    //gDPPipeSync(gMasterDisp++);
    gDPSetColorDither(gMasterDisp++, G_CD_MAGICSQ);
    gSPStarfield(gMasterDisp++);
}

void func_bg_8003E1E0(void) {
}


//#define F_PI        3.14159265f   /* pi             */
#define gSPFixDepthCut(pkt)                                       \
    {                                                                                   \
        Gfx* _g = (Gfx*) (pkt);                                                         \
                                                                                        \
        _g->words.w0 = 0x424C4E44; \
        _g->words.w1 = 0x46554369;                                           \
    }

#define gSPFixDepthCut2(pkt)                                       \
    {                                                                                   \
        Gfx* _g = (Gfx*) (pkt);                                                         \
                                                                                        \
        _g->words.w0 = 0x424C4E44; \
        _g->words.w1 = 0x46664369;                                           \
    }


// TODO: use SCREEN_WIDTH and _HEIGHT
void Background_DrawBackdrop(void) {
    f32 bgXpos2;
    f32 bgXpos;
    f32 bgYpos;
    f32 sp130;
    f32 sp12C;
    f32 bgScale;
    s32 i;
    u8 levelType;
    s32 levelId;
//return;
    if (gDrawBackdrop == 0) {
        return;
    }

    levelType = gLevelType;
    if ((gCurrentLevel == LEVEL_VERSUS) && (gVersusStage == VS_STAGE_SECTOR_Z)) {
        levelType = LEVELTYPE_PLANET;
    }
    if (gCurrentLevel == LEVEL_TRAINING) {
        levelType = LEVELTYPE_SPACE;
    }
    levelId = gCurrentLevel;

    Matrix_Push(&gGfxMatrix);

    if (gFovYMode == 2) {
        Matrix_Scale(gGfxMatrix, 1.2f, 1.2f, 1.0f, MTXF_APPLY);
    }
    switch (levelType) {
        case LEVELTYPE_PLANET:
            if (gCurrentLevel != LEVEL_VENOM_ANDROSS)
          gSPFixDepthCut(gMasterDisp++);

            RCP_SetupDL(&gMasterDisp, SETUPDL_17);
            switch (levelId) {
                case LEVEL_FORTUNA:
                case LEVEL_KATINA:
                case LEVEL_VENOM_2:
                case LEVEL_VERSUS:
                    bgYpos = (gPlayer[gPlayerNum].camPitch * -6000.0f) - (gPlayer[gPlayerNum].cam.eye.y * 0.4f);
                    bgXpos2 =
                        Math_ModF(Math_RadToDeg(gPlayer[gPlayerNum].camYaw) * (-7280.0f / 360.0f) * 5.0f, 7280.0f);
                    Matrix_RotateZ(gGfxMatrix, gPlayer[gPlayerNum].camRoll * M_DTOR, MTXF_APPLY);
                    Matrix_Translate(gGfxMatrix, bgXpos2, -2000.0f + bgYpos, -6000.0f, MTXF_APPLY);

                    if (gCurrentLevel == LEVEL_FORTUNA) {
                        Matrix_Translate(gGfxMatrix, 0.0f, -2000.0f, 0, MTXF_APPLY);
                    } else if (gCurrentLevel == LEVEL_KATINA) {
                        Matrix_Translate(gGfxMatrix, 0.0f, -2500.0f, 0, MTXF_APPLY);
                    }
                    Matrix_SetGfxMtx(&gMasterDisp);

                    switch (gCurrentLevel) {
                        case LEVEL_VERSUS:
                            if (gVersusStage == VS_STAGE_CORNERIA) {
                                gSPDisplayList(gMasterDisp++, aVsCorneriaBackdropDL);
                            } else if (gVersusStage == VS_STAGE_KATINA) {
                                gSPDisplayList(gMasterDisp++, aVsKatinaBackdropDL);
                            } else {
                                gSPDisplayList(gMasterDisp++, aVsSectorZBackdropDL);
                            }
                            break;
                        case LEVEL_FORTUNA:
                            gSPDisplayList(gMasterDisp++, aFoBackdropDL);
                            break;
                        case LEVEL_KATINA:
                            gSPDisplayList(gMasterDisp++, aKaBackdropDL);
                            break;
                        case LEVEL_VENOM_2:
                            gSPDisplayList(gMasterDisp++, aVe2BackdropDL);
                            break;
                    }
                    Matrix_Translate(gGfxMatrix, 7280.0f, 0.0f, 0.0f, MTXF_APPLY);
                    Matrix_SetGfxMtx(&gMasterDisp);
                    switch (gCurrentLevel) {
                        case LEVEL_VERSUS:
                            if (gVersusStage == VS_STAGE_CORNERIA) {
                                gSPDisplayList(gMasterDisp++, aVsCorneriaBackdropDL);
                            } else if (gVersusStage == VS_STAGE_KATINA) {
                                gSPDisplayList(gMasterDisp++, aVsKatinaBackdropDL);
                            } else {
                                gSPDisplayList(gMasterDisp++, aVsSectorZBackdropDL);
                            }
                            break;
                        case LEVEL_FORTUNA:
                            gSPDisplayList(gMasterDisp++, aFoBackdropDL);
                            break;
                        case LEVEL_KATINA:
                            gSPDisplayList(gMasterDisp++, aKaBackdropDL);
                            break;
                        case LEVEL_VENOM_2:
                            gSPDisplayList(gMasterDisp++, aVe2BackdropDL);
                            break;
                    }
                    break;

                case LEVEL_CORNERIA:
                case LEVEL_VENOM_1:
                    bgYpos = (gPlayer[gPlayerNum].camPitch * -6000.0f) - (gPlayer[gPlayerNum].cam.eye.y * 0.6f);
                    bgXpos2 =
                        Math_ModF(Math_RadToDeg(gPlayer[gPlayerNum].camYaw) * (-7280.0f / 360.0f) * 5.0f, 7280.0f);
                    Matrix_RotateZ(gGfxMatrix, gPlayer[gPlayerNum].camRoll * M_DTOR, MTXF_APPLY);
                    Matrix_Translate(gGfxMatrix, bgXpos2, -2000.0f + bgYpos, -6000.0f, MTXF_APPLY);
                    Matrix_SetGfxMtx(&gMasterDisp);

                    switch (gCurrentLevel) {
                        case LEVEL_CORNERIA:
                            gSPDisplayList(gMasterDisp++, aCoBackdropDL);
                            break;
                        case LEVEL_VENOM_1:
                            gSPDisplayList(gMasterDisp++, aVe1BackdropDL);
                            break;
                    }

                    Matrix_Translate(gGfxMatrix, 7280.0f, 0.0f, 0.0f, MTXF_APPLY);
                    Matrix_SetGfxMtx(&gMasterDisp);

                    switch (gCurrentLevel) {
                        case LEVEL_CORNERIA:
                            gSPDisplayList(gMasterDisp++, aCoBackdropDL);
                            break;
                        case LEVEL_VENOM_1:
                            gSPDisplayList(gMasterDisp++, aVe1BackdropDL);
                            break;
                    }
                    break;

                case LEVEL_VENOM_ANDROSS:
                    if (gDrawBackdrop != 6) {
                        if ((gDrawBackdrop == 2) || (gDrawBackdrop == 7)) {
                            Matrix_RotateZ(gGfxMatrix, gPlayer[gPlayerNum].camRoll * M_DTOR, MTXF_APPLY);
                            Matrix_Translate(gGfxMatrix, 0.0f, -4000.0f, -7000.0f, MTXF_APPLY);
                            Matrix_SetGfxMtx(&gMasterDisp);
                            gSPDisplayList(gMasterDisp++, aVe2BackdropDL);
                        } else if ((gDrawBackdrop == 3) || (gDrawBackdrop == 4)) {
                            RCP_SetupDL(&gMasterDisp, SETUPDL_62);

                            if (gDrawBackdrop == 4) {
                                if ((gGameFrameCount & 8) == 0) {
                                    Math_SmoothStepToF(&sAndrossUnkBrightness, 0.0f, 1.0f, 30.0f, 0);
                                } else {
                                    Math_SmoothStepToF(&sAndrossUnkBrightness, 120.0f, 1.0f, 30.0f, 0);
                                }
                            } else {
                                sAndrossUnkBrightness = 255.0f;
                            }
/* #if 1
        gDPSetEnvColor(gMasterDisp++, 0,0,0, 0xFF);
        gDPSetCombineLERP(gMasterDisp++, 1, ENVIRONMENT, TEXEL0, PRIMITIVE, PRIMITIVE, 0, TEXEL0, 0, 1, ENVIRONMENT,
                        TEXEL0, PRIMITIVE, PRIMITIVE, 0, TEXEL0, 0);
#endif */

                            gDPSetPrimColor(gMasterDisp++, 0x00, 0x00, 255, (s32) sAndrossUnkBrightness,
                                            (s32) sAndrossUnkBrightness, (s32) gAndrossUnkAlpha);

                            bgYpos = (gPlayer[gPlayerNum].camPitch * -6000.0f) - (gPlayer[gPlayerNum].cam.eye.y * 0.4f);
                            bgXpos2 = Math_ModF(Math_RadToDeg(gPlayer[gPlayerNum].camYaw) * (-7280.0f / 360.0f) * 5.0f,
                                                7280.0f);
                            Matrix_RotateZ(gGfxMatrix, gPlayer[gPlayerNum].camRoll * M_DTOR, MTXF_APPLY);
                            Matrix_Translate(gGfxMatrix, bgXpos2, -2000.0f + bgYpos, -6000.0f, MTXF_APPLY);
                            Matrix_Translate(gGfxMatrix, 0.0f, -2500.0f, 0.0f, MTXF_APPLY);
                            Matrix_SetGfxMtx(&gMasterDisp);
          gSPFixDepthCut(gMasterDisp++);
                            gSPDisplayList(gMasterDisp++, aVe2AndBrainBackdropDL);
                            Matrix_Translate(gGfxMatrix, 7280.0f, 0.0f, 0.0f, MTXF_APPLY);
                            Matrix_SetGfxMtx(&gMasterDisp);
                            gSPDisplayList(gMasterDisp++, aVe2AndBrainBackdropDL);
          gSPFixDepthCut(gMasterDisp++);
                        } else {
                            // this is why you cant see it
                                   //             RCP_SetupDL_36();

                       //     RCP_SetupDL(&gMasterDisp, SETUPDL_49);//62);
                            gDPSetEnvColor(gMasterDisp++, 255,0,127, 0xFF);
#if 1
                            gDPSetCombineLERP(gMasterDisp++, 1, ENVIRONMENT, TEXEL0, PRIMITIVE, PRIMITIVE, 0, TEXEL0, 0, 1, ENVIRONMENT,
                                            TEXEL0, PRIMITIVE, PRIMITIVE, 0, TEXEL0, 0);
    //                        gDPSetEnvColor(gMasterDisp++, 0,0,0, 0xFF);
#endif
                            if (gDrawBackdrop == 5) {
                                gDPSetPrimColor(gMasterDisp++, 0x00, 0x00, 255, 255, 255, 64);
                            } else {
                                gDPSetPrimColor(gMasterDisp++, 0x00, 0x00, 255,255,255,/*0 , 255, 128, */ (s32) gAndrossUnkAlpha);
                            }

                            Matrix_Translate(gGfxMatrix, 0.0f, 0.0f, -290.0f *41.0f , MTXF_APPLY);
                            Matrix_Push(&gGfxMatrix);
                            Matrix_Scale(gGfxMatrix, 11.0f *41.0f, 11.0f *41.0f , 1.0f, MTXF_APPLY);
                            Matrix_RotateZ(gGfxMatrix, (gPlayer[0].camRoll + (gGameFrameCount * 1.5f)) * M_DTOR,
                                           MTXF_APPLY);
                            Matrix_SetGfxMtx(&gMasterDisp);
                            gSPDisplayList(gMasterDisp++, aAndBackdropDL);
                            Matrix_Pop(&gGfxMatrix);

                            if (gDrawBackdrop != 5) {
                                Matrix_Translate(gGfxMatrix, 0.0f, 0.0f, 10.0f * 41.0f, MTXF_APPLY);
                                Matrix_Push(&gGfxMatrix);
//                                Matrix_Translate(gGfxMatrix, 0.0f, 0.0f, -280.0f * 35.0f, MTXF_APPLY);
//                                Matrix_Translate(gGfxMatrix, 0.0f, 0.0f, -280.0f*41.0f, MTXF_APPLY);
                                Matrix_Scale(gGfxMatrix, 10.0f *41.0f , 10.0f *41.0f , 1.0f, MTXF_APPLY);
                                Matrix_RotateZ(gGfxMatrix, (gPlayer[0].camRoll + (gGameFrameCount * -1.3f)) * M_DTOR,
                                               MTXF_APPLY);
                                Matrix_SetGfxMtx(&gMasterDisp);
                                gSPDisplayList(gMasterDisp++, aAndBackdropDL);
                                Matrix_Pop(&gGfxMatrix);
                            }
                            RCP_SetupDL(&gMasterDisp, SETUPDL_62);

                        }
                    }
                    break;

                case LEVEL_AQUAS:
                    if (gPlayer[0].state == PLAYERSTATE_LEVEL_INTRO) {
                        bgXpos2 = Math_RadToDeg(gPlayer[gPlayerNum].camYaw) - gPlayer[gPlayerNum].yRot_114;
                        bgYpos = (gPlayer[gPlayerNum].camPitch * -7000.0f) - (gPlayer[gPlayerNum].cam.eye.y * 0.6f);
                        bgXpos2 = Math_ModF(bgXpos2 * -40.44444f * 2.0f, 7280.0f); // close to 7280.0f / 180.0f
                        RCP_SetupDL_17();
                        Matrix_RotateZ(gGfxMatrix, gPlayer[gPlayerNum].camRoll * M_DTOR, MTXF_APPLY);
                        Matrix_Scale(gGfxMatrix, 1.5f, 1.0f, 1.0f, MTXF_APPLY);
                        Matrix_Push(&gGfxMatrix);
                        Matrix_Translate(gGfxMatrix, bgXpos2, bgYpos, -7000.0f, MTXF_APPLY);
                        Matrix_SetGfxMtx(&gMasterDisp);

                        if (gPlayer[0].state == PLAYERSTATE_LEVEL_INTRO) {
                            gSPDisplayList(gMasterDisp++, aAqBackdropIntroDL);
                        } else {
                            gSPDisplayList(gMasterDisp++, aAqBackdropDL);
                        }

                        if (bgXpos2 < 0) {
                            bgXpos2 = 1.0f;
                        } else {
                            bgXpos2 = -1.0f;
                        }
                        Matrix_Translate(gGfxMatrix, 7280.0f * bgXpos2, 0.0f, 0.0f, MTXF_APPLY);
                        Matrix_SetGfxMtx(&gMasterDisp);
                        if (gPlayer[0].state == PLAYERSTATE_LEVEL_INTRO) {
                            gSPDisplayList(gMasterDisp++, aAqBackdropIntroDL);

                        } else {
                            gSPDisplayList(gMasterDisp++, aAqBackdropDL);
                        }
                        Matrix_Pop(&gGfxMatrix);
                    }
                    break;

                case LEVEL_SOLAR:
                case LEVEL_ZONESS:
                case LEVEL_MACBETH:
                case LEVEL_TITANIA:
                    sp12C = Math_RadToDeg(gPlayer[gPlayerNum].camYaw) - gPlayer[gPlayerNum].yRot_114;
                    bgYpos = (gPlayer[gPlayerNum].camPitch * -7000.0f) - (gPlayer[gPlayerNum].cam.eye.y * 0.6f);
                    bgXpos2 = sp12C * -40.44444f * 2.0f; // close to 7280.0f / 180.0f

                    if ((gCurrentLevel == LEVEL_TITANIA) && (gPlayer[0].state == PLAYERSTATE_LEVEL_INTRO) &&
                        (gPlayer[0].csState < 3)) {
                        D_bg_8015F968 += sinf(gPlayer[0].camYaw) * 20.0f;
                        bgXpos2 += D_bg_8015F968;
                    }
                    if ((gCurrentLevel == LEVEL_SOLAR) && (gPlayer[0].state == PLAYERSTATE_LEVEL_INTRO) &&
                        (gPlayer[0].csState >= 2) && (gPlayer[0].cam.eye.z <= -1900.0f)) {
                        D_bg_8015F968 = sinf(gPlayer[0].camPitch) * 7000.0f;
                        bgYpos -= fabsf(D_bg_8015F968);
                    }

                    bgXpos2 = Math_ModF(bgXpos2, 7280.0f);

                    RCP_SetupDL_17();
                    Matrix_RotateZ(gGfxMatrix, gPlayer[gPlayerNum].camRoll * M_DTOR, MTXF_APPLY);
                    Matrix_Scale(gGfxMatrix, 1.5f, 1.0f, 1.0f, MTXF_APPLY);

                    if ((gCurrentLevel == LEVEL_TITANIA) || (gCurrentLevel == LEVEL_ZONESS)) {
                        Matrix_Translate(gGfxMatrix, bgXpos2, -3000.0f + bgYpos, -7000.0f, MTXF_APPLY);
                    } else if (gCurrentLevel == LEVEL_SOLAR) {
                        Matrix_Translate(gGfxMatrix, bgXpos2, -3500.0f + bgYpos, -7000.0f, MTXF_APPLY);
                    } else if (gCurrentLevel == LEVEL_MACBETH) {
                        Matrix_Translate(gGfxMatrix, bgXpos2, -4000.0f + bgYpos, -7000.0f, MTXF_APPLY);
                    }
                    Matrix_SetGfxMtx(&gMasterDisp);

                    if (gCurrentLevel == LEVEL_TITANIA) {
                        gSPDisplayList(gMasterDisp++, aTiBackdropDL);
                    } else if (gCurrentLevel == LEVEL_MACBETH) {
                        gSPDisplayList(gMasterDisp++, aMaBackdropDL);
                    } else if (gCurrentLevel == LEVEL_ZONESS) {
                        gSPDisplayList(gMasterDisp++, aZoBackdropDL);
                    } else if (gCurrentLevel == LEVEL_SOLAR) {
                        gSPDisplayList(gMasterDisp++, aSoBackdropDL);
                    }

                    if (bgXpos2 < 0) {
                        bgXpos2 = 1.0f;
                    } else {
                        bgXpos2 = -1.0f;
                    }

                    Matrix_Translate(gGfxMatrix, 7280.0f * bgXpos2, 0.0f, 0.0f, MTXF_APPLY);
                    Matrix_SetGfxMtx(&gMasterDisp);

                    if (gCurrentLevel == LEVEL_TITANIA) {
                        gSPDisplayList(gMasterDisp++, aTiBackdropDL);
                    } else if (gCurrentLevel == LEVEL_MACBETH) {
                        gSPDisplayList(gMasterDisp++, aMaBackdropDL);
                    } else if (gCurrentLevel == LEVEL_ZONESS) {
                        gSPDisplayList(gMasterDisp++, aZoBackdropDL);
                    } else if (gCurrentLevel == LEVEL_SOLAR) {
                        gSPDisplayList(gMasterDisp++, aSoBackdropDL);
                    }
                    break;
            }
            if (gCurrentLevel != LEVEL_VENOM_ANDROSS)
          gSPFixDepthCut(gMasterDisp++);

            break;

        case LEVELTYPE_SPACE:        

        if (gPlayer[0].state != PLAYERSTATE_ENTER_WARP_ZONE) {
                Matrix_Push(&gGfxMatrix);
                sp12C = Math_RadToDeg(gPlayer[0].camYaw);
                sp130 = Math_RadToDeg(gPlayer[0].camPitch);
                if (((sp12C < 45.0f) || (sp12C > 315.0f)) && ((sp130 < 40.0f) || (sp130 > 325.0f))) {
                    RCP_SetupDL_36();
                    bgXpos = gStarfieldX; /* Range: 0 - 320.0f */
                    bgYpos = gStarfieldY;

                    if (((gCurrentLevel == LEVEL_SECTOR_X) || (gCurrentLevel == LEVEL_METEO)) && (gLevelPhase == 1)) {
                        levelId = LEVEL_WARP_ZONE;
                    }

                    /* Modulus Xpos should be SCREEN_WIDTH + 80.0f * 2 (+ 80.0f for each side) */
                    /* Modulus Ypos should be SCREEN_HEIGHT + 60.0f * 2 (+ 60.0f for each side) */
                    if (levelId == LEVEL_SECTOR_X) {
                        bgXpos = Math_ModF(bgXpos + 60.0f, SCREEN_WIDTH + (80.0f * 2));
                        bgYpos = Math_ModF(bgYpos + 360.0f - 40.0f, 360.0f);
                    } else if (levelId == LEVEL_TRAINING) {
                        bgXpos = Math_ModF(bgXpos - 30.0f, SCREEN_WIDTH + (80.0f * 2));
                        bgYpos = Math_ModF(bgYpos + 360.0f - 40.0f, 360.0f);
                    } else if ((levelId == LEVEL_SECTOR_Y) && (gLevelMode == LEVELMODE_ON_RAILS)) {
                        bgXpos = Math_ModF(bgXpos + 480.0f - 60.0f, SCREEN_WIDTH + (80.0f * 2));
                        bgYpos = Math_ModF(bgYpos, 360.0f);
                    } else if (levelId == LEVEL_FORTUNA) {
                        bgXpos = Math_ModF(bgXpos - 34.5f, SCREEN_WIDTH + (80.0f * 2));
                        bgYpos = Math_ModF(bgYpos + 19.0f, 360.0f);
                    } else if (levelId == LEVEL_BOLSE) {
                        if ((gPlayer[0].state != PLAYERSTATE_LEVEL_COMPLETE) || (gPlayer[0].csState < 10)) {
                            bgYpos = Math_ModF(bgYpos + 360.0f - 100.0f, 360.0f);
                        }
                    } else {
                        bgXpos = Math_ModF(bgXpos, SCREEN_WIDTH + (80.0f * 2));
                        bgYpos = Math_ModF(bgYpos, SCREEN_HEIGHT + (60.0f * 2));
                    }

                    if ((sp12C < 180.0f) && (bgXpos > 380.0f)) {
                        bgXpos = -(480.0f - bgXpos);
                    }
                    if ((sp130 > 180.0f) && (bgYpos > 280.0f)) {
                        bgYpos = -(360.0f - bgYpos);
                    }

                    Matrix_RotateZ(gGfxMatrix, gStarfieldRoll, MTXF_APPLY);

                    switch (levelId) {
                        case LEVEL_WARP_ZONE:
                            if ((s32) gWarpZoneBgAlpha != 0) {
                                // RCP_SetupDL_62();
                                gDPSetEnvColor(gMasterDisp++, 0,0,0, 0xFF);
                                gDPSetCombineLERP(gMasterDisp++, 1, ENVIRONMENT, TEXEL0, PRIMITIVE, PRIMITIVE, 0, TEXEL0, 0, 1, ENVIRONMENT,
                                                TEXEL0, PRIMITIVE, PRIMITIVE, 0, TEXEL0, 0);
                                s32 wzAlpha = (s32) gWarpZoneBgAlpha ;//* 2;
                                if (wzAlpha > 240) wzAlpha = 240;
                                gDPSetPrimColor(gMasterDisp++, 0x00, 0x00, 255, 255, 255, wzAlpha);
                                Matrix_Translate(gGfxMatrix, (bgXpos - 120.0f)* 41.0f, -(bgYpos - 120.0f)* 41.0f, -290.0f* 41.0f, MTXF_APPLY);
                                Matrix_Scale(gGfxMatrix, 1.7f* 41.0f, 1.7f* 41.0f, 1.0f, MTXF_APPLY);
                                Matrix_Push(&gGfxMatrix);
                                Matrix_RotateZ(gGfxMatrix, -(f32) gGameFrameCount * 10.0f * M_DTOR, MTXF_APPLY);
                                Matrix_Scale(gGfxMatrix, 1.07f, 0.93f, 1.0f, MTXF_APPLY);
                                Matrix_RotateZ(gGfxMatrix, gGameFrameCount * 10.0f * M_DTOR, MTXF_APPLY);
                                Matrix_Scale(gGfxMatrix, 1.07f, 0.93f, 1.0f, MTXF_APPLY);
                                Matrix_SetGfxMtx(&gMasterDisp);
//                  gSPFixDepthCut2(gMasterDisp++);
                                gSPDisplayList(gMasterDisp++, aWzBackdropDL);
  //                gSPFixDepthCut2(gMasterDisp++);
                                Matrix_Pop(&gGfxMatrix);
                            }
                            break;

                        case LEVEL_METEO:
                            if ((gPlayer[0].state == PLAYERSTATE_LEVEL_COMPLETE) && (gCsFrameCount > 260)) {
                                Matrix_Translate(gGfxMatrix, (bgXpos - 120.0f)* 43.7f, (-(bgYpos - 120.0f) - 30.0f)* 43.7f, -290.0f* 43.7f,
                                                 MTXF_APPLY);
                                Matrix_Scale(gGfxMatrix, 0.5f* 43.7f, 0.5f* 43.7f, 1.0f, MTXF_APPLY);
                                Matrix_SetGfxMtx(&gMasterDisp);
    //              gSPFixDepthCut2(gMasterDisp++);
                                gSPDisplayList(gMasterDisp++, aMeBackdropDL);
      //            gSPFixDepthCut2(gMasterDisp++);
                            } else if (gPathProgress > 185668.0f) {
                                Matrix_Translate(gGfxMatrix, (bgXpos - 120.0f)* 43.7f,( -(bgYpos - 120.0f) - 130.0f)* 43.7f, -290.0f* 43.7f,
                                                 MTXF_APPLY);
                                Matrix_Scale(gGfxMatrix, 0.4f* 43.7f, 0.4f* 43.7f, 1.0f, MTXF_APPLY);
                                Matrix_SetGfxMtx(&gMasterDisp);
//                  gSPFixDepthCut2(gMasterDisp++);
                                gSPDisplayList(gMasterDisp++, aMeBackdropDL);
  //                gSPFixDepthCut2(gMasterDisp++);
                            }
                            break;

                        case LEVEL_SECTOR_X:
                            if (gSceneSetup == 0) {
                                Matrix_Translate(gGfxMatrix, (bgXpos - 120.0f)* 43.7f, -(bgYpos - 120.0f)* 43.7f, -290.0f* 43.7f, MTXF_APPLY);
                                Matrix_Scale(gGfxMatrix, 3.0f* 43.7f, 3.0f* 43.7f, 1.0f, MTXF_APPLY);
                                Matrix_SetGfxMtx(&gMasterDisp);
                                // RCP_SetupDL_62();
                                gDPSetEnvColor(gMasterDisp++, 0,0,0, 0xFF);
                                gDPSetCombineLERP(gMasterDisp++, 1, ENVIRONMENT, TEXEL0, PRIMITIVE, PRIMITIVE, 0, TEXEL0, 0, 1, ENVIRONMENT,
                                                TEXEL0, PRIMITIVE, PRIMITIVE, 0, TEXEL0, 0);
                                gDPSetPrimColor(gMasterDisp++, 0x00, 0x00, 255, 255, 255, 160);//192);
                                gSPDisplayList(gMasterDisp++, aSxBackdropDL);
                         //       Matrix_Translate(gGfxMatrix, 0, 0, 2.0f, MTXF_APPLY);
                           //     Matrix_SetGfxMtx(&gMasterDisp);
                             //   gSPDisplayList(gMasterDisp++, aSxBackdropDL);
                            }
                            break;

                        case LEVEL_TRAINING:
                            Matrix_Translate(gGfxMatrix, (bgXpos - 120.0f)* 43.7f, -(bgYpos - 120.0f)* 43.7f, -290.0f* 43.7f, MTXF_APPLY);
                            Matrix_Scale(gGfxMatrix, 0.2f* 43.7f, 0.2f* 43.7f, 1.0f, MTXF_APPLY);
                            Matrix_SetGfxMtx(&gMasterDisp);
                            // RCP_SetupDL_62();
                            gDPSetPrimColor(gMasterDisp++, 0x00, 0x00, 255, 255, 255, 255);
                            gSPDisplayList(gMasterDisp++, aTrBackdropDL);
                            break;

                        case LEVEL_AREA_6:
                        case LEVEL_UNK_4:
                            bgScale = (gPathProgress * 0.00004f) + 0.5f;
                            if (bgScale > 3.5f) {
                                bgScale = 3.5f;
                            }
                            if (gPlayer[0].state == PLAYERSTATE_LEVEL_COMPLETE) {
                                bgScale = gArea6BackdropScale;
                                if (bgScale > 3.5f) {
                                    bgScale = 3.5f;
                                }
                            }
                            Matrix_Translate(gGfxMatrix, (bgXpos - 120.0f)* 43.7f, (-(bgYpos - 120.0f))* 43.7f, -290.0f* 43.7f, MTXF_APPLY);
                            Matrix_Scale(gGfxMatrix, bgScale * 0.75* 43.7f, bgScale * 0.75f* 43.7f, 1.0f, MTXF_APPLY);
                            Matrix_SetGfxMtx(&gMasterDisp);
                            gSPDisplayList(gMasterDisp++, aA6BackdropDL);
                            break;

                        case LEVEL_FORTUNA:
                            bgScale = 1.5f;
                            if ((gCsFrameCount > 400) && (gMissionStatus == MISSION_COMPLETE)) {
                                bgScale = 0.75f;
                            }
                            Matrix_Translate(gGfxMatrix, (bgXpos - 120.0f)* 43.7f, -(bgYpos - 120.0f)* 43.7f, -290.0f* 43.7f, MTXF_APPLY);
                            Matrix_Scale(gGfxMatrix, bgScale* 43.7f, bgScale* 43.7f, bgScale, MTXF_APPLY);
                            Matrix_SetGfxMtx(&gMasterDisp);
                            gSPDisplayList(gMasterDisp++, aFoCorneriaDL);
                            break;

                        case LEVEL_BOLSE:
                            bgScale = 1.0f;
                            if ((gCsFrameCount > 500) && (gPlayer[0].state == PLAYERSTATE_LEVEL_COMPLETE)) {
                                bgScale = 1.3f;
                            }
                            Matrix_Translate(gGfxMatrix, (bgXpos - 120.0f)* 43.7f, -(bgYpos - 120.0f)* 43.7f, -290.0f* 43.7f, MTXF_APPLY);
                            Matrix_Scale(gGfxMatrix, bgScale* 43.7f, bgScale* 43.7f, bgScale, MTXF_APPLY);
                            Matrix_SetGfxMtx(&gMasterDisp);
                            gSPDisplayList(gMasterDisp++, aBoBackdropDL);
                            break;

                        case LEVEL_SECTOR_Z:
                            Matrix_Translate(gGfxMatrix, (bgXpos - 120.0f)* 43.7f, -(bgYpos - 120.0f)* 43.7f, -290.0f* 43.7f, MTXF_APPLY);
                            Matrix_Scale(gGfxMatrix, 0.5f* 43.7f, 0.5f* 43.7f, 0.5f, MTXF_APPLY);
                            Matrix_RotateX(gGfxMatrix, F_PI_2, MTXF_APPLY);
                            Matrix_SetGfxMtx(&gMasterDisp);
                  gSPFixDepthCut2(gMasterDisp++);
                            gSPDisplayList(gMasterDisp++, aSzBackgroundDL);
 //                               Matrix_Translate(gGfxMatrix, 0, 0, 2.0f, MTXF_APPLY);
   //                             Matrix_SetGfxMtx(&gMasterDisp);
     //                       gSPDisplayList(gMasterDisp++, aSzBackgroundDL);
                  gSPFixDepthCut2(gMasterDisp++);
                            break;

                        case LEVEL_SECTOR_Y:
                            Matrix_Translate(gGfxMatrix,( bgXpos - 120.0f)* 43.7f, -(bgYpos - 120.0f)* 43.7f, -290.0f * 43.7f, MTXF_APPLY);
                            Matrix_Scale(gGfxMatrix, 0.4f* 43.7f, 0.4f* 43.7f, 1.0f, MTXF_APPLY);
                            Matrix_SetGfxMtx(&gMasterDisp);
                            // RCP_SetupDL_62();
                            gDPSetEnvColor(gMasterDisp++, 0,0,0, 0xFF);
                            gDPSetCombineLERP(gMasterDisp++, 1, ENVIRONMENT, TEXEL0, PRIMITIVE, PRIMITIVE, 0, TEXEL0, 0, 1, ENVIRONMENT,
                                            TEXEL0, PRIMITIVE, PRIMITIVE, 0, TEXEL0, 0);
                            gDPSetPrimColor(gMasterDisp++, 0x00, 0x00, 255, 255, 255, 160);////192);
                            gSPDisplayList(gMasterDisp++, aSyBackdropDL);
                     //           Matrix_Translate(gGfxMatrix, 0, 0, 2.0f, MTXF_APPLY);
                       //         Matrix_SetGfxMtx(&gMasterDisp);
                         //   gSPDisplayList(gMasterDisp++, aSyBackdropDL);
                            break;
                    }

                }
                Matrix_Pop(&gGfxMatrix);
            }

            if (gStarWarpDistortion > 0.0f) {
                f32* xStar = gStarOffsetsX;
                f32* yStar = gStarOffsetsY;
                f32 zRot;

                RCP_SetupDL_14();
                gDPSetPrimColor(gMasterDisp++, 0x00, 0x00, 128, 128, 255, 255);

                zRot = 0.0f;
                for (i = 0; i < 300; i++, xStar++, yStar++) {
                    *xStar = RAND_FLOAT_SEEDED(480.0f) - 80.0f;
                    *yStar = RAND_FLOAT_SEEDED(360.0f) - 60.0f;
                    Matrix_Push(&gGfxMatrix);
                    Matrix_Translate(gGfxMatrix, (*xStar - 160.0f) * 10.0f, (*yStar - 120.0f) * 10.0f, -5000.0f,
                                     MTXF_APPLY);
                    Matrix_RotateZ(gGfxMatrix, zRot, MTXF_APPLY);
                    Matrix_Scale(gGfxMatrix, 10.0f, 1.0f, -gStarWarpDistortion, MTXF_APPLY);
                    Matrix_SetGfxMtx(&gMasterDisp);
                    gSPDisplayList(gMasterDisp++, gTexturedLineDL);
                    Matrix_Pop(&gGfxMatrix);
                    zRot += F_PI_4;
                }
            }

            break;
    }

    Matrix_Pop(&gGfxMatrix);
}

void Background_DrawSun(void) {
    f32 camYaw;
    f32 camPitch;
    Color_RGB8* sunColor;
    s32* sunAlpha;
    Gfx** sunDL;
    f32* sunScale;
    s32 i;
    s32 levelType = gLevelType;

    if ((gCurrentLevel == LEVEL_KATINA) || (gCurrentLevel == LEVEL_VENOM_2) || (gCurrentLevel == LEVEL_VENOM_ANDROSS) ||
        (gCurrentLevel == LEVEL_SOLAR) || (gCurrentLevel == LEVEL_TRAINING) || gVersusMode) {
        return;
    }

    gPlayerGlareAlphas[gPlayerNum] -= sSunGlareAlphaStep[levelType];
    if (gPlayerGlareAlphas[gPlayerNum] > 300) {
        gPlayerGlareAlphas[gPlayerNum] = 0;
    }

    if (((gCurrentLevel == LEVEL_AQUAS) && (gPlayer[0].state == PLAYERSTATE_LEVEL_INTRO)) ||
        (((gPlayer[gPlayerNum].state == PLAYERSTATE_U_TURN) || (gLevelMode == LEVELMODE_ALL_RANGE) ||
          (gPlayer[gPlayerNum].state == PLAYERSTATE_LEVEL_COMPLETE)) &&
         (gLevelType == LEVELTYPE_PLANET) && (gCurrentLevel != LEVEL_TITANIA) && (gCurrentLevel != LEVEL_AQUAS))) {
        gPlayerGlareReds[gPlayerNum] = 255;//128;
        gPlayerGlareGreens[gPlayerNum] = 255;//128;
        gPlayerGlareBlues[gPlayerNum] = 255;//128;

        camYaw = Math_RadToDeg(gPlayer[gPlayerNum].camYaw);
        camPitch = Math_RadToDeg(gPlayer[gPlayerNum].camPitch);
        if (camPitch > 180.0f) {
            camPitch -= 360.0f;
        }

        camYaw -= 135.0f;
        gSunViewX = -camYaw * 3.2f;
        gSunViewY = (-camPitch * 3.2f) + 130.0f - ((gPlayer[gPlayerNum].cam.eye.y - 350.0f) * 0.015f);

        if (gCurrentLevel == LEVEL_KATINA) {
            gSunViewY -= 80.0f;
        }
        if ((gCurrentLevel == LEVEL_ZONESS) && (gPlayer[0].csState >= 2) && !gMissedZoSearchlight) {
            gSunViewY -= 60.0f;
            gSunViewX -= 480.0f;
        }

        if ((gSunViewX < 120.0f) && (gSunViewX > -120.0f) && (gSunViewY < 120.0f)) {
            gPlayerGlareAlphas[gPlayerNum] += sSunGlareAlphaStep[levelType] * 2;
            if (sSunGlareMaxAlpha[levelType] < gPlayerGlareAlphas[gPlayerNum]) {
                gPlayerGlareAlphas[gPlayerNum] = sSunGlareMaxAlpha[levelType];
            }
        }
    }

    if (gPlayerGlareAlphas[gPlayerNum] != 0) {
        Matrix_Push(&gGfxMatrix);
        Matrix_RotateZ(gGfxMatrix, gPlayer[gPlayerNum].camRoll * M_DTOR, MTXF_APPLY);
        Matrix_Translate(gGfxMatrix, gSunViewX, gSunViewY, -200.0f, MTXF_APPLY);
        RCP_SetupDL_62();
        sunColor = sSunColors;
        sunAlpha = sSunAlphas;
        sunDL = sSunDLs;
        sunScale = sSunScales;

        if (gCurrentLevel == LEVEL_KATINA) {
            sunColor = sKaSunColors;
            sunAlpha = sKaSunAlphas;
            sunDL = sKaSunDLs;
            sunScale = sKaSunScales;
        }

        for (i = 0; i < 5; i++, sunColor++, sunAlpha++, sunDL++, sunScale++) {
            Matrix_Push(&gGfxMatrix);
            Matrix_Scale(gGfxMatrix, *sunScale, *sunScale, *sunScale, MTXF_APPLY);
            Matrix_SetGfxMtx(&gMasterDisp);
     gDPSetEnvColor(gMasterDisp++, 0,0,0, 0xFF);
    gDPSetCombineLERP(gMasterDisp++, 1, ENVIRONMENT, TEXEL0, PRIMITIVE, PRIMITIVE, 0, TEXEL0, 0, 1, ENVIRONMENT,
                      TEXEL0, PRIMITIVE, PRIMITIVE, 0, TEXEL0, 0);
            gDPSetPrimColor(gMasterDisp++, 0x00, 0x00, sunColor->r, sunColor->g, sunColor->b, *sunAlpha);
            gSPDisplayList(gMasterDisp++, *sunDL);
            Matrix_Pop(&gGfxMatrix);
        }
        Matrix_Pop(&gGfxMatrix);
    }
}

void Background_DrawLensFlare(void) {
    s32 i;
    Color_RGB8* lensFlareColor;
    s32* lensFlareAlpha;
    Gfx** lensFlareDL;
    f32* lensFlareScale;
    f32* lensFlareShift;
    f32 lensFlareOffsetX;
    f32 lensFlareOffsetY;
    f32 alphaMod;
    f32 alpha;

    if ((gCurrentLevel == LEVEL_VENOM_ANDROSS) || (gLevelType == LEVELTYPE_SPACE) ||
        (gPlayerGlareAlphas[gPlayerNum] == 0)) {
        return;
    }

    alphaMod = 1.0f;
    if (gPlayerGlareAlphas[gPlayerNum] < 80) {
        alphaMod = gPlayerGlareAlphas[gPlayerNum] / 80.0f;
    }
    alphaMod *= sLensFlareAlphaMod[gLevelType];

    Matrix_Push(&gGfxMatrix);
    Matrix_RotateZ(gGfxMatrix, gPlayer[gPlayerNum].camRoll * M_DTOR, MTXF_APPLY);
    Matrix_Translate(gGfxMatrix, gSunViewX, gSunViewY, -200.0f, MTXF_APPLY);
    RCP_SetupDL_62();

    lensFlareOffsetX = gSunViewX * -0.03f;
    lensFlareOffsetY = gSunViewY * 0.03f;
    lensFlareColor = &sSunColors[5];
    lensFlareAlpha = &sSunAlphas[5];
    lensFlareDL = &sSunDLs[5];
    lensFlareScale = &sSunScales[5];
    lensFlareShift = &sSunShifts[5];

    if (gCurrentLevel == LEVEL_KATINA) {
        lensFlareColor = &sKaSunColors[5];
        lensFlareAlpha = &sKaSunAlphas[5];
        lensFlareDL = &sKaSunDLs[5];
        lensFlareScale = &sKaSunScales[5];
        lensFlareShift = &sKaSunShifts[5];
    }

    for (i = 5; i < 13; i++, lensFlareColor++, lensFlareAlpha++, lensFlareDL++, lensFlareScale++, lensFlareShift++) {
        Matrix_Push(&gGfxMatrix);

        Matrix_Translate(gGfxMatrix, *lensFlareShift * lensFlareOffsetX, *lensFlareShift * -lensFlareOffsetY, 0.0f,
                         MTXF_APPLY);
        Matrix_Scale(gGfxMatrix, *lensFlareScale, *lensFlareScale, *lensFlareScale, MTXF_APPLY);

        if (((i == 5) || (i == 11)) && (gCurrentLevel != LEVEL_KATINA)) {
            Matrix_RotateX(gGfxMatrix, F_PI_2, MTXF_APPLY);
        }
        Matrix_SetGfxMtx(&gMasterDisp);

        alpha = *lensFlareAlpha;
        if (i >= 5) {
            alpha *= alphaMod;
        }

//        gDPSetPrimColor(gMasterDisp++, 0x00, 0x00, lensFlareColor->r, lensFlareColor->g, lensFlareColor->b,
  //                      (s32) alpha);

        gDPSetPrimColor(gMasterDisp++, 0x00, 0x00, lensFlareColor->r, lensFlareColor->g, lensFlareColor->b, (s32) alpha);
//        gDPSetEnvColor(gMasterDisp++, 0, 0, 0, 255);
        gDPSetRenderMode(gMasterDisp++, G_RM_AA_ZB_XLU_SURF, G_RM_AA_ZB_XLU_SURF2);
  //      gDPSetCombineLERP(gMasterDisp++, 1, ENVIRONMENT, TEXEL0, PRIMITIVE, PRIMITIVE, 0, TEXEL0, 0, 1, ENVIRONMENT,
    //                  TEXEL0, PRIMITIVE, PRIMITIVE, 0, TEXEL0, 0);

        gSPDisplayList(gMasterDisp++, *lensFlareDL);
        Matrix_Pop(&gGfxMatrix);
    }
    Matrix_Pop(&gGfxMatrix);
}
#include <stdio.h>
void Background_dummy_80040CDC(void) {
//    printf("%s\n",__func__);
}

extern void gfx_texture_cache_invalidate(void *addr);

int round_to_nearest_5(int n) {
    return ((n + 2) / 5) * 5;
}

#define gSPScrollingFloorFix(pkt)                                       \
    {                                                                                   \
        Gfx* _g = (Gfx*) (pkt);                                                         \
                                                                                        \
        _g->words.w0 = 0x424C4E44; \
        _g->words.w1 = 0x465543F0;                                           \
    }

Vtx ast_aquas_seg6_vtx_2AC602[] = {
    {{{  4000,      0,  -6000}, 0, { 20947+9207,  -19923+19923}, {  255, 0,   0, 255}}},
    {{{ -4000,      0,  -6000}, 0, {     0+9207,  -19923+19923 }, {  0, 255,   0, 255}}},
    {{{ -4000,      0,      0}, 0, {     0+9207,  -9449+19923 }, {  0, 0,   255, 255}}},
    {{{  4000,      0,      0}, 0, { 20947+9207,   -9449+19923 }, {  0, 0,   0, 255}}},
    {{{ -4000,      0,   6000}, 0, {     0+9207,   1023+19923}, {  255, 255,   255, 255}}},
    {{{  4000,      0,   6000}, 0, { 20947+9207,   1023+19923}, {  255, 0,   255, 255}}},
};

Gfx aAqWaterSurfaceDL2[] = {
    gsSPVertex(ast_aquas_seg6_vtx_2AC602, 6, 0),
    gsSP2Triangles(1, 2, 3, 0, 1, 3, 0, 0),
    gsSP2Triangles(3, 4, 5, 0, 2, 3, 4, 0),
    gsSPEndDisplayList(),
};


void Background_DrawGround(void) {
    f32 zPos;
    s32 i;
    u32 xScroll;
    u32 yScroll;
    u16* groundTex = NULL;
    Gfx* groundDL = NULL;
    if (((gCurrentLevel != LEVEL_VENOM_2) && (gPlayer[0].cam.eye.y > 4000.0f)) || !gDrawGround) {
        return;
    }
    if ((gCurrentLevel == LEVEL_BOLSE) && gBolseDynamicGround) {
        Bolse_DrawDynamicGround();
        return;
    }

    zPos = 0.0f;
    if ((gGroundType != GROUND_10) && (gGroundType != GROUND_11)) {
        zPos = -4000.0f;
    }
    if (gGroundType == GROUND_7) {
        zPos = 0.0f;
        gPlayer[gPlayerNum].xPath = 0.0f;
    }

    if (gLevelMode == LEVELMODE_ALL_RANGE) {
        Vec3f sp1B4;
        Vec3f sp1A8;
        f32 camX;
        f32 camZ;

        zPos = 0.0f;
        gPlayer[gPlayerNum].xPath = 0.0f;

        sp1B4.x = 0;
        sp1B4.y = 0;
        sp1B4.z = -5500.0f;

        Matrix_RotateY(gCalcMatrix, -gPlayer[gPlayerNum].camYaw, MTXF_NEW);
        Matrix_MultVec3fNoTranslate(gCalcMatrix, &sp1B4, &sp1A8);

        camX = gPlayer[gPlayerNum].cam.eye.x + sp1A8.x;
        camZ = gPlayer[gPlayerNum].cam.eye.z + sp1A8.z;

        if (camX > 6000.0f) {
            gPlayer[gPlayerNum].xPath = 12000.0f;
        }
        if (camX > 18000.0f) {
            gPlayer[gPlayerNum].xPath = 24000.0f;
        }
        if (camX < -6000.0f) {
            gPlayer[gPlayerNum].xPath = -12000.0f;
        }
        if (camX < -18000.0f) {
            gPlayer[gPlayerNum].xPath = -24000.0f;
        }

        if (camZ > 6000.0f) {
            zPos = 12000.0f;
        }
        if (camZ > 18000.0f) {
            zPos = 24000.0f;
        }
        if (camZ < -6000.0f) {
            zPos = -12000.0f;
        }
        if (camZ < -18000.0f) {
            zPos = -24000.0f;
        }
    }

    Matrix_Push(&gGfxMatrix);
    Matrix_Translate(gGfxMatrix, gPlayer[gPlayerNum].xPath, -3.0f + gCameraShakeY, zPos, MTXF_APPLY);

    if (gFovYMode == 2) {
        Matrix_Scale(gGfxMatrix, 1.2f, 1.2f, 1.0f, MTXF_APPLY);
    }

    Matrix_SetGfxMtx(&gMasterDisp);

    switch (gCurrentLevel) {
        case LEVEL_CORNERIA: {
            if (gGroundClipMode != 0) {
                RCP_SetupDL_29(gFogRed, gFogGreen, gFogBlue, gFogAlpha, gFogNear, gFogFar);
            } else {
                RCP_SetupDL_20(gFogRed, gFogGreen, gFogBlue, gFogAlpha, gFogNear, gFogFar);
            }

            if (gLevelMode == LEVELMODE_ON_RAILS) {
                gDPSetTextureImage(gMasterDisp++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 1,
                                   SEGMENTED_TO_VIRTUAL(aCoGroundGrassTex));

                yScroll = fabsf(gPathTexScroll * 0.4266667f);
                xScroll = (10000.0f - gPlayer[gPlayerNum].xPath) * 0.32f;

                gDPSetupTile(gMasterDisp++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 32, 32, xScroll, yScroll,
                             G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, 5, 5, G_TX_NOLOD, G_TX_NOLOD);
//                gSPScrollingFloorFix(gMasterDisp++);
                u32 uls, ult, lrs, lrt;
                uls = (((u32)(xScroll)) << 12) & 0x7FF000;
                lrs = (uls + (127 << 12)) & 0xFFF000;
                ult = ((u32)(yScroll)) & 0x0007FF;
                lrt = (ult + 127) & 0x000FFF;
                
                Gfx* gfx = (Gfx*)(gMasterDisp - 1);
                gfx->words.w0 = (G_SETTILESIZE << 24) | uls | ult;
                gfx->words.w1 = lrs | lrt;

                switch (gGroundSurface) {
                    case SURFACE_GRASS:
                        gDPLoadTileTexture(gMasterDisp++, aCoGroundGrassTex, G_IM_FMT_RGBA, G_IM_SIZ_16b, 32, 32);
                        gBgColor = 0x845; // 8, 8, 32
                        break;
                    case SURFACE_ROCK:
                        gDPLoadTileTexture(gMasterDisp++, aCoGroundRockTex, G_IM_FMT_RGBA, G_IM_SIZ_16b, 32, 32);
                        gBgColor = 0x845; // 8, 8, 32
                        break;
                    case SURFACE_WATER:
                        RCP_SetupDL_45(gFogRed, gFogGreen, gFogBlue, gFogAlpha, gFogNear, gFogFar);
                        gDPSetPrimColor(gMasterDisp++, 0x00, 0x00, 255, 255, 255, 128);
                        // jnmartin84
                        // anywhere you see this idiom, I am working around my shitty color combiner code
                        // in order to make a transparent colored surface
                        gDPSetEnvColor(gMasterDisp++, 0,0,0, 0xFF);
                        gDPSetCombineLERP(gMasterDisp++, 1, ENVIRONMENT, TEXEL0, PRIMITIVE, PRIMITIVE, 0, TEXEL0, 0, 1, ENVIRONMENT,
                                        TEXEL0, PRIMITIVE, PRIMITIVE, 0, TEXEL0, 0);
                        gDPLoadTileTexture(gMasterDisp++, D_CO_6028A60, G_IM_FMT_RGBA, G_IM_SIZ_16b, 32, 32);

                        gBgColor = 0x190F; // 24, 32, 56
                        break;
                }

                Matrix_Push(&gGfxMatrix);
                Matrix_Translate(gGfxMatrix, 0.0f, 0.0f, -3000.0f, MTXF_APPLY);
                Matrix_Scale(gGfxMatrix, 1.0f, 1.0f, 0.5f, MTXF_APPLY);
                Matrix_SetGfxMtx(&gMasterDisp);
                gSPDisplayList(gMasterDisp++, aCoGroundOnRailsDL);
                Matrix_Pop(&gGfxMatrix);
                Matrix_Translate(gGfxMatrix, 0.0f, 0.0f, 3000.0f, MTXF_APPLY);
                Matrix_Scale(gGfxMatrix, 1.0f, 1.0f, 0.5f, MTXF_APPLY);
                Matrix_SetGfxMtx(&gMasterDisp);
                gSPDisplayList(gMasterDisp++, aCoGroundOnRailsDL);
//                gSPScrollingFloorFix(gMasterDisp++);
            } else {
                gGroundSurface = SURFACE_GRASS;
                gBgColor = 0x845; // 8, 8, 32
                for (i = 0; i < ARRAY_COUNT(sGroundPositions360x); i++) {
                    Matrix_Push(&gGfxMatrix);
                    Matrix_Translate(gGfxMatrix, sGroundPositions360x[i], 0.0f, sGroundPositions360z[i], MTXF_APPLY);
                    Matrix_SetGfxMtx(&gMasterDisp);
                    gSPDisplayList(gMasterDisp++, aCoGroundAllRangeDL);
                    Matrix_Pop(&gGfxMatrix);
                }
            }
        }
            break;

        case LEVEL_VENOM_1:
        case LEVEL_MACBETH:
        {
            RCP_SetupDL_29(gFogRed, gFogGreen, gFogBlue, gFogAlpha, gFogNear, gFogFar);

            switch (gCurrentLevel) {
                case LEVEL_VENOM_1:
                    groundTex = aVe1GroundTex;
                    groundDL = aVe1GroundDL;
                    gDPLoadTextureBlock(gMasterDisp++, groundTex, G_IM_FMT_RGBA, G_IM_SIZ_16b, 32, 32, 0,
                                        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, 5, 5, G_TX_NOLOD,
                                        G_TX_NOLOD);
                    break;
                case LEVEL_MACBETH:
                    groundTex = aMaGroundTex;
                    groundDL = aMaGroundDL;
                    gDPLoadTextureBlock(gMasterDisp++, groundTex, G_IM_FMT_RGBA, G_IM_SIZ_16b, 32, 32, 0,
                                        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, 5, 5, G_TX_NOLOD,
                                        G_TX_NOLOD);
                    break;
            }

            gDPSetTextureImage(gMasterDisp++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, groundTex);

            yScroll = fabsf(gPathTexScroll * 0.4266667f);
            xScroll = (10000.0f - gPlayer[gPlayerNum].xPath) * 0.32f;
//                gSPScrollingFloorFix(gMasterDisp++);

            gDPSetupTile(gMasterDisp++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 32, 32, xScroll, yScroll,
                         G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, 5, 5, G_TX_NOLOD, G_TX_NOLOD);

            u16 uls, ult, lrs, lrt;

            uls = (u32)xScroll & 0x07FF;
            ult = (u32)yScroll & 0x07FF;

            lrs = (uls + 127) & 0x0FFF;
            lrt = (ult + 127) & 0x0FFF;

            Gfx* gfx = (Gfx*)(gMasterDisp - 1);
            gfx->words.w0 = (G_SETTILESIZE << 24) | (uls << 12) | ult;
            gfx->words.w1 = (lrs << 12) | lrt;
            
            Matrix_Push(&gGfxMatrix);
            Matrix_Translate(gGfxMatrix, 0.0f, 0.0f, -3000.0f, MTXF_APPLY);
            Matrix_Scale(gGfxMatrix, 1.0f, 1.0f, 0.5f, MTXF_APPLY);
            Matrix_SetGfxMtx(&gMasterDisp);
            gSPDisplayList(gMasterDisp++, groundDL);
            Matrix_Pop(&gGfxMatrix);
            Matrix_Push(&gGfxMatrix);
            Matrix_Translate(gGfxMatrix, 0.0f, 0.0f, 3000.0f, MTXF_APPLY);
            Matrix_Scale(gGfxMatrix, 1.0f, 1.0f, 0.5f, MTXF_APPLY);
            Matrix_SetGfxMtx(&gMasterDisp);
            gSPDisplayList(gMasterDisp++, groundDL);
            Matrix_Pop(&gGfxMatrix);
//                            gSPScrollingFloorFix(gMasterDisp++);

        }
            break;

        case LEVEL_TRAINING: {
            RCP_SetupDL_29(gFogRed, gFogGreen, gFogBlue, gFogAlpha, gFogNear, gFogFar);
            if (gLevelMode == LEVELMODE_ON_RAILS) {
                if (gPathTexScroll > 290.0f) {
                    gPathTexScroll -= 290.0f;
                }
                Matrix_Push(&gGfxMatrix);
                Matrix_Translate(gGfxMatrix, 0.0f, 0.0f, -3000.0f + gPathTexScroll, MTXF_APPLY);
                Matrix_Scale(gGfxMatrix, 1.0f, 1.0f, 0.5f, MTXF_APPLY);
                Matrix_SetGfxMtx(&gMasterDisp);
                gSPDisplayList(gMasterDisp++, aTrGroundDL);
                Matrix_Pop(&gGfxMatrix);
                if (1) {}
                Matrix_Push(&gGfxMatrix);
                Matrix_Translate(gGfxMatrix, 0.0f, 0.0f, 3000.0f + gPathTexScroll, MTXF_APPLY);
                Matrix_Scale(gGfxMatrix, 1.0f, 1.0f, 0.5f, MTXF_APPLY);
                Matrix_SetGfxMtx(&gMasterDisp);
                gSPDisplayList(gMasterDisp++, aTrGroundDL);
                Matrix_Pop(&gGfxMatrix);
            } else {
                for (i = 0; i < ARRAY_COUNT(sGroundPositions360x); i++) {
                    Matrix_Push(&gGfxMatrix);
                    Matrix_Translate(gGfxMatrix, sGroundPositions360x[i], 0.0f, sGroundPositions360z[i], MTXF_APPLY);
                    Matrix_Scale(gGfxMatrix, 1.5f, 1.0f, 1.0f, MTXF_APPLY);
                    Matrix_SetGfxMtx(&gMasterDisp);
                    gSPDisplayList(gMasterDisp++, aTrGroundDL);
                    Matrix_Pop(&gGfxMatrix);
                }
            }
        }
            break;

        case LEVEL_AQUAS:
        {
            RCP_SetupDL(&gMasterDisp, SETUPDL_20);
//            groundDL = aAqGroundDL;
            gSPFogPosition(gMasterDisp++, gFogNear, gFogFar);

                yScroll = fabsf(gPathTexScroll * 0.4266667f);
                xScroll = (10000.0f - gPlayer[gPlayerNum].xPath) * 0.32f;

            // Ground
            if (!gDrawAquasSurfaceWater && ((gAqDrawMode == 0) || (gAqDrawMode == 2))) {
                gDPLoadTileTexture(gMasterDisp++, SEGMENTED_TO_VIRTUAL(aAqGroundTex), G_IM_FMT_RGBA, G_IM_SIZ_16b, 32,
                                   32);
                gDPSetTextureImage(gMasterDisp++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, SEGMENTED_TO_VIRTUAL(aAqGroundTex));


                gDPSetupTile(gMasterDisp++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 32, 32, xScroll, yScroll, 
                             G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, 5, 5, G_TX_NOLOD, G_TX_NOLOD);
                u16 uls, ult, lrs, lrt;

                uls = xScroll & 0x07FF;
                ult = yScroll & 0x07FF;

                lrs = (uls + 127) & 0x0FFF;
                lrt = (ult + 127) & 0x0FFF;

                Gfx* gfx = (Gfx*)(gMasterDisp - 1);
                gfx->words.w0 = (G_SETTILESIZE << 24) | (uls << 12) | ult;
                gfx->words.w1 = (lrs << 12) | lrt;

                Matrix_Push(&gGfxMatrix);
                Matrix_Translate(gGfxMatrix, 0.0f, 0.0f, -3000.0f, MTXF_APPLY);
                Matrix_Scale(gGfxMatrix, 1.0f, 1.0f, 0.5f, MTXF_APPLY);
                Matrix_SetGfxMtx(&gMasterDisp);
                gSPDisplayList(gMasterDisp++, aAqGroundDL);
                Matrix_Pop(&gGfxMatrix);
                Matrix_Push(&gGfxMatrix);
                Matrix_Translate(gGfxMatrix, 0.0f, 0.0f, 3000.0f, MTXF_APPLY);
                Matrix_Scale(gGfxMatrix, 1.0f, 1.0f, 0.5f, MTXF_APPLY);
                Matrix_SetGfxMtx(&gMasterDisp);
                gSPDisplayList(gMasterDisp++, aAqGroundDL);
                Matrix_Pop(&gGfxMatrix);
            }

            // Surface water
            if (gDrawAquasSurfaceWater || (gAqDrawMode == 0)) {
                gDPLoadTileTexture(gMasterDisp++, SEGMENTED_TO_VIRTUAL(aAqWaterTex), G_IM_FMT_RGBA, G_IM_SIZ_16b, 32, 32);
                gDPSetTextureImage(gMasterDisp++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, SEGMENTED_TO_VIRTUAL(aAqWaterTex));

                gDPSetupTile(gMasterDisp++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 32, 32, xScroll, yScroll,
                            G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, 5, 5, G_TX_NOLOD, G_TX_NOLOD);

                u16 uls, ult, lrs, lrt;

                uls = xScroll & 0x07FF;
                ult = yScroll & 0x07FF;

                lrs = (uls + 127) & 0x0FFF;
                lrt = (ult + 127) & 0x0FFF;

                Gfx* gfx = (Gfx*)(gMasterDisp - 1);
                gfx->words.w0 = (G_SETTILESIZE << 24) | (uls << 12) | ult;
                gfx->words.w1 = (lrs << 12) | lrt;

                if (gAqDrawMode != 0) {
                    RCP_SetupDL(&gMasterDisp, SETUPDL_47);
                } else {
                    RCP_SetupDL(&gMasterDisp, SETUPDL_37);
                }

                if ((gPlayer[0].state == PLAYERSTATE_LEVEL_INTRO) && (gPlayer[0].csState < 2)) {
                    gDPSetPrimColor(gMasterDisp++, 0x00, 0x00, 255, 255, 255, 255);
                } else if (gPlayer[0].state == PLAYERSTATE_LEVEL_COMPLETE) {
                    gDPSetPrimColor(gMasterDisp++, 0x00, 0x00, gAquasSurfaceColorR, gAquasSurfaceColorG,
                                    gAquasSurfaceColorB, gAquasSurfaceAlpha2);
                } else {
                    gDPSetPrimColor(gMasterDisp++, 0x00, 0x00, 255, 255, 255, (s32) gAquasSurfaceAlpha);
                }

//                gDPSetCombineMode(gMasterDisp++,G_CC_BLENDRGBA, G_CC_BLENDRGBA);

                Matrix_Push(&gGfxMatrix);

                Matrix_Translate(gGfxMatrix, 0.0f, gSurfaceWaterYPos, -3000.0f, MTXF_APPLY);
                Matrix_Scale(gGfxMatrix, 2.0f, 1.0f, 0.5f, MTXF_APPLY);
                Matrix_SetGfxMtx(&gMasterDisp);
                gSPDisplayList(gMasterDisp++, aAqWaterSurfaceDL2);

                Matrix_Pop(&gGfxMatrix);

                Matrix_Translate(gGfxMatrix, 0.0f, gSurfaceWaterYPos, 3000.0f, MTXF_APPLY);
                Matrix_Scale(gGfxMatrix, 2.0f, 1.0f, 0.5f, MTXF_APPLY);
                Matrix_SetGfxMtx(&gMasterDisp);
                gSPDisplayList(gMasterDisp++, aAqWaterSurfaceDL2);
            }
        }
            break;

        case LEVEL_FORTUNA:
        case LEVEL_KATINA:
        case LEVEL_BOLSE:
        case LEVEL_VENOM_2:
            if ((gGroundClipMode != 0) || (gCurrentLevel == LEVEL_BOLSE)) {
                RCP_SetupDL_29(gFogRed, gFogGreen, gFogBlue, gFogAlpha, gFogNear, gFogFar);
            } else {
                RCP_SetupDL_20(gFogRed, gFogGreen, gFogBlue, gFogAlpha, gFogNear, gFogFar);
            }

            for (i = 0; i < ARRAY_COUNT(sGroundPositions360x); i++) {
                Matrix_Push(&gGfxMatrix);
                Matrix_Translate(gGfxMatrix, sGroundPositions360x[i], 0.0f, sGroundPositions360z[i], MTXF_APPLY);
                Matrix_SetGfxMtx(&gMasterDisp);
                if (gCurrentLevel == LEVEL_FORTUNA) {
                    gSPDisplayList(gMasterDisp++, aFoGroundDL);
                } else if (gCurrentLevel == LEVEL_KATINA) {
                    gSPDisplayList(gMasterDisp++, aKaGroundDL);
                } else if (gCurrentLevel == LEVEL_BOLSE) {
                    gSPDisplayList(gMasterDisp++, aBoGroundDL);
                } else if (gCurrentLevel == LEVEL_VENOM_2) {
                    gSPDisplayList(gMasterDisp++, aVe2GroundDL);
                }
                Matrix_Pop(&gGfxMatrix);
            }
            break;

        case LEVEL_VERSUS:
            if (gGroundClipMode != 0) {
                RCP_SetupDL_29(gFogRed, gFogGreen, gFogBlue, gFogAlpha, gFogNear, gFogFar);
            } else {
                RCP_SetupDL_20(gFogRed, gFogGreen, gFogBlue, gFogAlpha, gFogNear, gFogFar);
            }

            for (i = 0; i < ARRAY_COUNT(sGroundPositions360x); i++) {
                Matrix_Push(&gGfxMatrix);
                Matrix_Translate(gGfxMatrix, sGroundPositions360x[i], 0.0f, sGroundPositions360z[i], MTXF_APPLY);
                Matrix_SetGfxMtx(&gMasterDisp);
                if (gVersusStage == VS_STAGE_CORNERIA) {
                    gSPDisplayList(gMasterDisp++, aVsCorneriaGroundDL);
                } else {
                    gSPDisplayList(gMasterDisp++, aVsKatinaGroundDL);
                }
                Matrix_Pop(&gGfxMatrix);
            }
            break;

        case LEVEL_SOLAR:
            RCP_SetupDL_29(gFogRed, gFogGreen, gFogBlue, gFogAlpha, gFogNear, gFogFar);
            Matrix_Translate(gGfxMatrix, 0.0f, 0.0f, -2000.0f, MTXF_APPLY);
            Matrix_Scale(gGfxMatrix, 3.0f, 2.0f, 3.0f, MTXF_APPLY);
            Matrix_SetGfxMtx(&gMasterDisp);
            if ((gGameFrameCount % 2) != 0) {
                gSPDisplayList(gMasterDisp++, aSoLava1DL);
            } else {
                gSPDisplayList(gMasterDisp++, aSoLava2DL);
            }
            break;

        case LEVEL_ZONESS:
            RCP_SetupDL_29(gFogRed, gFogGreen, gFogBlue, gFogAlpha, gFogNear, gFogFar);
            Matrix_Translate(gGfxMatrix, 0.0f, 0.0f, -1500.0f, MTXF_APPLY);
            Matrix_Scale(gGfxMatrix, 3.0f, 2.0f, 3.0f, MTXF_APPLY);
            Matrix_SetGfxMtx(&gMasterDisp);
            if ((gGameFrameCount % 2) != 0) {
                gSPDisplayList(gMasterDisp++, aZoWater1DL);
            } else {
                gSPDisplayList(gMasterDisp++, aZoWater2DL);
            }
            break;
    }
    Matrix_Pop(&gGfxMatrix);
}

// Unused. Early water implementation in Aquas?
void func_bg_80042D38(void) {
    f32 xEye;
    f32 zEye;

    Matrix_Push(&gGfxMatrix);

    xEye = gPlayer[gPlayerNum].cam.eye.x;
    zEye = gPlayer[gPlayerNum].cam.eye.z;

    Matrix_Translate(gGfxMatrix, xEye, 2.0f + gCameraShakeY, zEye, MTXF_APPLY);
    Matrix_Scale(gGfxMatrix, 1.5f, 1.0f, 1.0f, MTXF_APPLY);

    RCP_SetupDL_37(gFogRed, gFogGreen, gFogBlue, gFogAlpha, gFogNear, gFogFar);
    gDPSetPrimColor(gMasterDisp++, 0x00, 0x00, 255, 255, 255, 125);

    Matrix_SetGfxMtx(&gMasterDisp);

    if ((gGameFrameCount % 2) != 0) {
        gSPDisplayList(gMasterDisp++, aAqUnusedWater1DL);
    } else {
        gSPDisplayList(gMasterDisp++, aAqUnusedWater2DL);
    }

    Matrix_Pop(&gGfxMatrix);
}
