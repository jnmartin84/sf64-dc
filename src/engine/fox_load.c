#include <kos.h>

#include "global.h"
#include "sf64dma.h"

u8 sFillTimer = 3;
extern char *fnpre;
#include "fox_load_inits.c"

NewScene sCurrentScene = {
    sERROR,
    -1,
    {
        "","","","",
        "","","","",
        "","","","",
        "","","","",
    }
};


#if 0
Scene sCurrentScene = {
    NO_OVERLAY,
    { /* 0x1 */ NO_SEGMENT,
      /* 0x2 */ NO_SEGMENT,
      /* 0x3 */ NO_SEGMENT,
      /* 0x4 */ NO_SEGMENT,
      /* 0x5 */ NO_SEGMENT,
      /* 0x6 */ NO_SEGMENT,
      /* 0x7 */ NO_SEGMENT,
      /* 0x8 */ NO_SEGMENT,
      /* 0x9 */ NO_SEGMENT,
      /* 0xA */ NO_SEGMENT,
      /* 0xB */ NO_SEGMENT,
      /* 0xC */ NO_SEGMENT,
      /* 0xD */ NO_SEGMENT,
      /* 0xE */ NO_SEGMENT,
      /* 0xF */ NO_SEGMENT },
};
#endif

// 5,950,080 bytes
#if 0
void Load_RomFile(void* vRomAddress, void* dest, ptrdiff_t size) {
    s32 i;

    for (i = 0; gDmaTable[i].pRom.end != NULL; i++) {
        if (gDmaTable[i].vRomAddress == vRomAddress) {
            if (gDmaTable[i].compFlag == 0) {
                Lib_DmaRead(gDmaTable[i].pRom.start, dest, size);
            } else {
                Lib_FillScreen(1);
                sFillTimer = 3;
                gGameStandby = 1;
                Lib_DmaRead(gDmaTable[i].pRom.start, gFrameBuffers, SEGMENT_SIZE(gDmaTable[i].pRom));
                Mio0_Decompress(gFrameBuffers, dest);
            }
            break;
        }
    }
}
#endif
extern u8 *SEG_BUF[15];
#include <stdio.h>
char fullfn[256];
void Load_RomFile(char *fname, int snum) {
    if (strcmp("",fname)) { 
    //printf("LOAD_ROMFILE %s %d\n", fname, snum);
//Lib_DmaRead(gDmaTable[i].pRom.start, dest, size);
    // dest is always SEG_BUF[snum - 1]
    sprintf(fullfn, "%s/sf_data/%s.bin", fnpre, fname);
    file_t rom_file = fs_open(fullfn, O_RDONLY);
    size_t rom_file_size = fs_seek(rom_file, 0, SEEK_END);
    fs_seek(rom_file, 0, SEEK_SET);
    //printf("\treading %d bytes from %s (%d)\n", rom_file_size, fullfn, rom_file);
    fs_read(rom_file, SEG_BUF[snum - 1], rom_file_size);
    fs_close(rom_file);
    gSegments[snum] = SEG_BUF[snum - 1];
//            gSPSegment(gUnkDisp1++, snum, SEG_BUF[snum - 1]);
    }
}


extern void nuke_everything();
u8 Load_SceneFiles(NewScene* scene, int snum) {
//    u8* ramPtr = SEGMENT_VRAM_START(ovl_i1);
    u8 segment;
    u8 changeScene = 0;

    //printf("%s(%d,%d)\n",__func__, scene->id, snum);

    if (scene->id == sCurrentScene.id && snum == sCurrentScene.snum) {
        // do nothing
    } else {
        changeScene = 1;
        nuke_everything();
        // still do nothing
    }

    sCurrentScene.id = scene->id;
    sCurrentScene.snum = snum;

#if 0
    if (scene->ovl.rom.start == (0, sCurrentScene.ovl.rom.start)) { // fake because D_800CBDD4 is probably 2D array
        ramPtr = ramPtr + SEGMENT_SIZE(scene->ovl.rom);
        ramPtr = ramPtr + SEGMENT_SIZE(scene->ovl.bss);
    } else {
        sCurrentScene.ovl.rom.start = scene->ovl.rom.start;
        sCurrentScene.ovl.rom.end = ramPtr;
        if (scene->ovl.rom.start != 0) {
            changeScene = 1;
            Load_RomFile(scene->ovl.rom.start, ramPtr, SEGMENT_SIZE(scene->ovl.rom));
            ramPtr = ramPtr + SEGMENT_SIZE(scene->ovl.rom);
            bzero(scene->ovl.bss.start, SEGMENT_SIZE(scene->ovl.bss));
            ramPtr = ramPtr + SEGMENT_SIZE(scene->ovl.bss);
        }
    }
    segment = 0;
    while ((segment < 15) && 
strcmp("",scene->segs[segment].fname) && 
    //(scene->assets[segment].start == sCurrentScene.assets[segment].start) &&
           (changeScene == 0)) {
        if (scene->assets[segment].start != 0) {
            gSegments[segment + 1] = K0_TO_PHYS(ramPtr);
            gSPSegment(gUnkDisp1++, segment + 1, K0_TO_PHYS(ramPtr));
            ramPtr = ramPtr + SEGMENT_SIZE(scene->assets[segment]);
        }
        segment += 1; // can't be ++
    }

    for (segment; segment < 15; segment += 1) {
        sCurrentScene.assets[segment].start = scene->assets[segment].start;
        sCurrentScene.assets[segment].end = ramPtr;
        if (scene->assets[segment].start != 0) {
            gSegments[segment + 1] = K0_TO_PHYS(ramPtr);
            gSPSegment(gUnkDisp1++, segment + 1, K0_TO_PHYS(ramPtr));
            Load_RomFile(scene->assets[segment].start, ramPtr, SEGMENT_SIZE(scene->assets[segment]));
            ramPtr = ramPtr + SEGMENT_SIZE(scene->assets[segment]);
        }
    }
#endif
if(changeScene) {
    for (segment=1; segment < 16; segment += 1) {
//        memcpy(&sCurrentScene.segs[segment].fname, 
//            &scene->segs[segment].fname, 32);
        
        Load_RomFile(scene->segs[segment], segment);
    }
}
    if (sFillTimer != 0) {
        sFillTimer--;
    } else if (gStartNMI == 0) {
       //Lib_FillScreen(0);
    }
    return changeScene;
}

extern NewScene ns_title[];
extern NewScene ns_option[];
extern NewScene ns_map[];
extern NewScene ns_gameover[];
extern NewScene ns_corneria[];
extern NewScene ns_meteo[];
extern NewScene ns_titania[];
extern NewScene ns_sectorx[];
extern NewScene ns_sectorz[];
extern NewScene ns_aquas[];
extern NewScene ns_area6[];
extern NewScene ns_fortuna[];
extern NewScene ns_unk4[];
extern NewScene ns_sectory[];
extern NewScene ns_solar[];
extern NewScene ns_zoness[];
extern NewScene ns_andross[];
extern NewScene ns_training[];
extern NewScene ns_venom1[];
extern NewScene ns_venom2[];
extern NewScene ns_setup20[];
extern NewScene ns_bolse[];
extern NewScene ns_katina[];
extern NewScene ns_macbeth[];
extern NewScene ns_versus[];
extern NewScene ns_logo[];
extern NewScene ns_ending[];




u8 Load_SceneSetup(u8 sceneId, u8 sceneSetup) {
    u8 changeScene = 0;
    //printf("%s\n", __func__);

    switch (sceneId) {
        case SCENE_TITLE:
            changeScene = Load_SceneFiles(ns_title, sceneSetup);
                //&sOvlmenu_Title[sceneSetup]);
            if (changeScene == 1) {
                AUDIO_SET_SPEC(SFX_LAYOUT_DEFAULT, AUDIOSPEC_OPENING);
            }
            break;
        case SCENE_MENU:
            changeScene = Load_SceneFiles(ns_option, sceneSetup);
            break;
        case SCENE_MAP:
            changeScene = Load_SceneFiles(ns_map, sceneSetup);
            break;
        case SCENE_GAME_OVER:
            changeScene = Load_SceneFiles(ns_gameover, sceneSetup);
            break;
        case SCENE_CORNERIA:
            changeScene = Load_SceneFiles(ns_corneria, sceneSetup);
            break;
        case SCENE_METEO:
            changeScene = Load_SceneFiles(ns_meteo, sceneSetup);
            break;
        case SCENE_TITANIA:
            changeScene = Load_SceneFiles(ns_titania, sceneSetup);
            break;
        case SCENE_SECTOR_X:
            changeScene = Load_SceneFiles(ns_sectorx, sceneSetup);
            break;
        case SCENE_SECTOR_Z:
            changeScene = Load_SceneFiles(ns_sectorz, sceneSetup);
            break;
        case SCENE_AQUAS:
            changeScene = Load_SceneFiles(ns_aquas, sceneSetup);
            break;
        case SCENE_AREA_6:
            changeScene = Load_SceneFiles(ns_area6, sceneSetup);
            break;
        case SCENE_FORTUNA:
            changeScene = Load_SceneFiles(ns_fortuna, sceneSetup);
            break;
        case SCENE_UNK_4:
            changeScene = Load_SceneFiles(ns_unk4, sceneSetup);
            break;
        case SCENE_SECTOR_Y:
            changeScene = Load_SceneFiles(ns_sectory, sceneSetup);
            break;
        case SCENE_SOLAR:
            changeScene = Load_SceneFiles(ns_solar, sceneSetup);
            break;
        case SCENE_ZONESS:
            changeScene = Load_SceneFiles(ns_zoness, sceneSetup);
            break;
        case SCENE_VENOM_ANDROSS:
            changeScene = Load_SceneFiles(ns_andross, sceneSetup);
            break;
        case SCENE_TRAINING:
            changeScene = Load_SceneFiles(ns_training, sceneSetup);
            break;
        case SCENE_VENOM_1:
            changeScene = Load_SceneFiles(ns_venom1, sceneSetup);
            break;
        case SCENE_VENOM_2:
            changeScene = Load_SceneFiles(ns_venom2, sceneSetup);
            break;
        case SCENE_20:
            changeScene = Load_SceneFiles(ns_setup20, sceneSetup);
            break;
        case SCENE_BOLSE:
            changeScene = Load_SceneFiles(ns_bolse, sceneSetup);
            break;
        case SCENE_KATINA:
            changeScene = Load_SceneFiles(ns_katina, sceneSetup);
            break;
        case SCENE_MACBETH:
            changeScene = Load_SceneFiles(ns_macbeth, sceneSetup);
            break;
        case SCENE_VERSUS:
            changeScene = Load_SceneFiles(ns_versus, sceneSetup);
            if (changeScene == 1) {
                AUDIO_SET_SPEC_ALT(SFX_LAYOUT_VS, AUDIOSPEC_VS);
            }
            break;
        case SCENE_LOGO:
            changeScene = Load_SceneFiles(ns_logo, sceneSetup); // Logo does not load an overlay file
            if (changeScene == 1) {
                AUDIO_SET_SPEC(SFX_LAYOUT_DEFAULT, AUDIOSPEC_MA);
            }
            break;
        case SCENE_CREDITS:
            changeScene = Load_SceneFiles(ns_ending, sceneSetup);
            break;
        default:
            PRINTF("DMA MODE ERROR %d\n");
            changeScene = 0;
            break;
    }
    return changeScene;
}

void Load_InitDmaAndMsg(void) {
//    Lib_DmaRead(SEGMENT_ROM_START(dma_table), SEGMENT_VRAM_START(dma_table), SEGMENT_ROM_SIZE(dma_table));
//    Load_RomFile(SEGMENT_ROM_START(ast_radio), SEGMENT_VRAM_START(ast_radio), SEGMENT_ROM_SIZE(ast_radio));
}
