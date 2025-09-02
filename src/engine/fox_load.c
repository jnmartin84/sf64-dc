#include "global.h"
#include "sf64dma.h"

#include <kos.h>
#include <stdio.h>
#include <stdlib.h>


u8 sFillTimer = 3;
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


extern char *fnpre;
extern u8 *SEG_BUF[15];


void Load_RomFile(char *fname, int snum) {
    if (strcmp("",fname)) { 
        char fullfn[256];
        // dest is always SEG_BUF[snum - 1]
        sprintf(fullfn, "%s/sf_data/%s.bin", fnpre, fname);
        file_t rom_file = fs_open(fullfn, O_RDONLY);
        if ((file_t)-1 == rom_file) {
            printf("Could not open %s for reading.\n", fullfn);
            exit(-1);
        }
        size_t rom_file_size = fs_seek(rom_file, 0, SEEK_END);
        fs_seek(rom_file, 0, SEEK_SET);
        fs_read(rom_file, SEG_BUF[snum - 1], rom_file_size);
        fs_close(rom_file);
        gSegments[snum] = SEG_BUF[snum - 1];
        printf("loaded %s (%d bytes) to %08x\n", fullfn, rom_file_size, SEG_BUF[snum - 1]);
    }
}


extern void nuke_everything();
u8 Load_SceneFiles(NewScene* scene, int snum) {
    u8 segment;
    u8 changeScene = 0;

    if (scene[snum].id == sCurrentScene.id && snum == sCurrentScene.snum) {
        // do nothing
    } else {
        changeScene = 1;
        nuke_everything();
        // still do nothing
    }

    sCurrentScene.id = scene[snum].id;
    sCurrentScene.snum = snum;

    if(changeScene) {
        printf("changing scene to snum %d\n", snum);
        for (segment=1; segment < 16; segment += 1) {
            Load_RomFile(scene[snum].segs[segment], segment);
        }
    }

    if (sFillTimer != 0) {
        sFillTimer--;
    } else if (gStartNMI == 0) {
       Lib_FillScreen(0);
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

    switch (sceneId) {
        case SCENE_TITLE:
            changeScene = Load_SceneFiles(ns_title, sceneSetup);
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
