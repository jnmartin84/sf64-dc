#include "n64sys.h"
#include "sf64audio_external.h"
// #include "mods.h"
#include <stdio.h>
#include <kos/thread.h>
#include <kos/genwait.h>
#include <dc/vblank.h>
#include "gfx/gfx_pc.h"
#include "gfx/gfx_opengl.h"
#include "gfx/gfx_dc.h"
#include <stdlib.h>

#if defined(IDE_SUPPORT) || defined(SDCARD_SUPPORT)
#include <dc/g1ata.h>
#include <dc/sd.h>
#include <fat/fs_fat.h>

static kos_blockdev_t dev;
#endif

#if USE_32KHZ
#define SAMPLES_HIGH 560
#define SAMPLES_LOW 528
#else
#if USE_16KHZ
#define SAMPLES_HIGH 280
#define SAMPLES_LOW 264
#else
#define SAMPLES_HIGH 464
#define SAMPLES_LOW 432
//#define SAMPLES_HIGH 470
//#define SAMPLES_LOW 442
#endif
#endif

extern struct GfxWindowManagerAPI gfx_glx;
extern struct GfxRenderingAPI gfx_opengl_api;
static struct GfxWindowManagerAPI* wm_api = &gfx_dc;
static struct GfxRenderingAPI* rendering_api = &gfx_opengl_api;

extern void gfx_run(Gfx* commands);
char* fnpre;
#include "src/dcaudio/audio_api.h"
#include "src/dcaudio/audio_dc.h"
volatile int inited = 0;
extern struct AudioAPI audio_dc;
s16 audio_buffer[2][SAMPLES_HIGH * 2 * 2 * 3] __attribute__((aligned(64)));
static struct AudioAPI* audio_api = NULL;
void AudioThread_CreateNextAudioBuffer(s16* samplesL, s16* samplesR, u32 num_samples);

void* AudioThread(UNUSED void* arg);

static volatile uint64_t vblticker = 0;

void vblfunc(uint32_t c, void* d) {
    (void) c;
    (void) d;
    vblticker++;
    osSendMesg(&gGfxVImesgQueue, (OSMesg) NULL, OS_MESG_NOBLOCK);
    genwait_wake_one((void*) &vblticker);
}

void _AudioInit(void) {
    if (audio_api == NULL) {
        audio_api = &audio_dc;
        audio_api->init();
    }
}

SPTask* gCurrentTask;
SPTask* sAudioTasks[1];
SPTask* sGfxTasks[2];
#ifdef AVOID_UB
SPTask* sNewAudioTasks[2];
#else
SPTask* sNewAudioTasks[1];
#endif
SPTask* sNewGfxTasks[2];

#include <kos.h>

extern u8* SEG_BUF[15];
extern u32 SEG_PAGECOUNT[15];

/* Macro for converting P1 address to physical memory address */
#define P1_TO_PHYSICAL(addr) ((uintptr_t)(addr) & MEM_AREA_CACHE_MASK)

void segment_init(void) {
    printf("Using SH4 TLB mappings for segmented address translation.\n");

    mmu_init_basic();

    mmu_page_map_static(0, 0x0C000000, PAGE_SIZE_1M, MMU_ALL_RDWR, true);

    for (uint32_t s = 1; s < 16; s++) {
        for (uint32_t p = 0; p < SEG_PAGECOUNT[s - 1]; p++) {
            mmu_page_map_static(
                ((uintptr_t) (s << 24) + (65536 * p)),
                P1_TO_PHYSICAL(SEG_BUF[(s - 1)]) + (65536 * p),
                PAGE_SIZE_64K, MMU_ALL_RDWR, true);
        }
    }
}

OSMesgQueue gPiMgrCmdQueue;
OSMesg sPiMgrCmdBuff[50];

OSMesgQueue gDmaMesgQueue;
void* sDmaMsgBuff[1];
OSIoMesg gDmaIOMsg;
OSMesgQueue gSerialEventQueue;
void* sSerialEventBuff[1];
OSMesgQueue gMainThreadMesgQueue;
void* sMainThreadMsgBuff[32];
OSMesgQueue gTaskMesgQueue;
void* sTaskMsgBuff[16];
OSMesgQueue gAudioVImesgQueue;
void* sAudioVImsgBuff[1];
OSMesgQueue gAudioTaskMesgQueue;
void* sAudioTaskMsgBuff[1];
OSMesgQueue gGfxVImesgQueue;
void* sGfxVImsgBuff[4];
OSMesgQueue gGfxTaskMesgQueue;
void* sGfxTaskMsgBuff[2];
OSMesgQueue gSerialThreadMesgQueue;
void* sSerialThreadMsgBuff[8];
OSMesgQueue gControllerMesgQueue;
void* sControllerMsgBuff[1];
OSMesgQueue gSaveMesgQueue;
void* sSaveMsgBuff[1];
OSMesgQueue gTimerTaskMesgQueue;
void* sTimerTaskMsgBuff[16];
OSMesgQueue gTimerWaitMesgQueue;
void* sTimerWaitMsgBuff[1];

GfxPool gGfxPools[2];

GfxPool* gGfxPool;
SPTask* gGfxTask;
Vp* gViewport;
Mtx* gGfxMtx;
Gfx* gUnkDisp1;
Gfx* gMasterDisp;
Gfx* gUnkDisp2;
Lightsn* gLight;
FrameBuffer* gFrameBuffer;

u8 gVIsPerFrame;
u32 gSysFrameCount;
u8 gStartNMI;
u8 gStopTasks;
u8 gControllerRumbleFlags[4];
u16 gFillScreenColor;
u16 gFillScreen;

u8 gUnusedStack[0x1000];
OSThread sIdleThread;            // 80138E90
u8 sIdleThreadStack[0x1000];     // 801390A0
OSThread gMainThread;            // 8013A040
u8 sMainThreadStack[0x1000];     // 8013A1F0
OSThread gGraphicsThread;        // 800DEAA0
u8 gGraphicsThreadStack[0x1000]; // 800DEC50
OSThread gTimerThread;           // 800DFC50
u8 gTimerThreadStack[0x1000];    // 800DFE00
OSThread gSerialThread;          // 800E0E00
u8 gSerialThreadStack[0x1000];   // 800E0FB0

volatile int called = 0;

extern void AudioLoad_LoadFiles(void);

void Graphics_SetTask(void) {
    gGfxTask->mesgQueue = &gGfxTaskMesgQueue;
    gGfxTask->msg = (OSMesg) TASK_MESG_2;
    gGfxTask->task.t.type = M_GFXTASK;
    gGfxTask->task.t.flags = 0;
    gGfxTask->task.t.ucode_boot = NULL;
    gGfxTask->task.t.ucode_boot_size = 0;
    gGfxTask->task.t.ucode = NULL;
    gGfxTask->task.t.ucode_size = SP_UCODE_SIZE;
    gGfxTask->task.t.ucode_data = NULL;
    gGfxTask->task.t.ucode_data_size = SP_UCODE_DATA_SIZE;
    gGfxTask->task.t.dram_stack = gDramStack;
    gGfxTask->task.t.dram_stack_size = SP_DRAM_STACK_SIZE8;
    gGfxTask->task.t.output_buff = (u64*) gTaskOutputBuffer;
    gGfxTask->task.t.output_buff_size = (u64*) gAudioHeap;
    gGfxTask->task.t.data_ptr = (u64*) gGfxPool->masterDL;
    gGfxTask->task.t.data_size = (gMasterDisp - gGfxPool->masterDL) * sizeof(Gfx);
    gGfxTask->task.t.yield_data_ptr = (u64*) &gOSYieldData;
    gGfxTask->task.t.yield_data_size = OS_YIELD_DATA_SIZE;
    called++;
    if (called > 5) {
        gfx_run(gGfxPool->masterDL);
    }
}

void Graphics_InitializeTask(u32 frameCount) {
    gGfxPool = &gGfxPools[frameCount % 2];

    gGfxTask = &gGfxPool->task;
    gViewport = gGfxPool->viewports;
    gGfxMtx = gGfxPool->mtx;
    gUnkDisp1 = gGfxPool->unkDL1;
    gMasterDisp = gGfxPool->masterDL;
    gUnkDisp2 = gGfxPool->unkDL2;
    gLight = gGfxPool->lights;

    gFrameBuffer = &gFrameBuffers[frameCount % 3];

    gGfxMatrix = &sGfxMatrixStack[0];
    gCalcMatrix = &sCalcMatrixStack[0];

    D_80178710 = &D_80178580[0];
}

void Main_InitMesgQueues(void) {
    osCreateMesgQueue(&gDmaMesgQueue, sDmaMsgBuff, ARRAY_COUNT(sDmaMsgBuff));
    osCreateMesgQueue(&gTaskMesgQueue, sTaskMsgBuff, ARRAY_COUNT(sTaskMsgBuff));
    osCreateMesgQueue(&gAudioVImesgQueue, sAudioVImsgBuff, ARRAY_COUNT(sAudioVImsgBuff));
    osCreateMesgQueue(&gAudioTaskMesgQueue, sAudioTaskMsgBuff, ARRAY_COUNT(sAudioTaskMsgBuff));
    osCreateMesgQueue(&gGfxVImesgQueue, sGfxVImsgBuff, ARRAY_COUNT(sGfxVImsgBuff));
    osCreateMesgQueue(&gGfxTaskMesgQueue, sGfxTaskMsgBuff, ARRAY_COUNT(sGfxTaskMsgBuff));
    osCreateMesgQueue(&gSerialEventQueue, sSerialEventBuff, ARRAY_COUNT(sSerialEventBuff));
    // osSetEventMesg(OS_EVENT_SI, &gSerialEventQueue, NULL);
    osCreateMesgQueue(&gMainThreadMesgQueue, sMainThreadMsgBuff, ARRAY_COUNT(sMainThreadMsgBuff));
    // osViSetEvent(&gMainThreadMesgQueue, (OSMesg) EVENT_MESG_VI, 1);
    // osSetEventMesg(OS_EVENT_SP, &gMainThreadMesgQueue, (OSMesg) EVENT_MESG_SP);
    // osSetEventMesg(OS_EVENT_DP, &gMainThreadMesgQueue, (OSMesg) EVENT_MESG_DP);
    // osSetEventMesg(OS_EVENT_PRENMI, &gMainThreadMesgQueue, (OSMesg) EVENT_MESG_PRENMI);
    osCreateMesgQueue(&gTimerTaskMesgQueue, sTimerTaskMsgBuff, ARRAY_COUNT(sTimerTaskMsgBuff));
    osCreateMesgQueue(&gTimerWaitMesgQueue, sTimerWaitMsgBuff, ARRAY_COUNT(sTimerWaitMsgBuff));
    osCreateMesgQueue(&gSerialThreadMesgQueue, sSerialThreadMsgBuff, ARRAY_COUNT(sSerialThreadMsgBuff));
    osCreateMesgQueue(&gControllerMesgQueue, sControllerMsgBuff, ARRAY_COUNT(sControllerMsgBuff));
    osCreateMesgQueue(&gSaveMesgQueue, sSaveMsgBuff, ARRAY_COUNT(sSaveMsgBuff));
}

#include <dc/sound/sound.h>
#include <dc/sound/stream.h>
#include "sndwav.h"
#include <stdio.h>
#include <stdlib.h>
#include <kos/thread.h>
#include <kos.h>
#include "james2.xbm"

vmufb_t vmubuf;

extern mutex_t io_lock;

void Main_ThreadEntry(void* arg0) {
    OSMesg osMesg;
    u8 mesg;
    void* sp24;
    u8 i;
    u8 visPerFrame;
    u8 validVIsPerFrame;

    segment_init();

    mutex_init(&io_lock, MUTEX_TYPE_NORMAL);

    maple_device_t* dev = NULL;
    if ((dev = maple_enum_type(0, MAPLE_FUNC_LCD))) {
        vmufb_paint_xbm(&vmubuf, 0, 0, 48, 32, james_bits);
        // only draw to first vmu
        vmufb_present(&vmubuf, dev);
    }

    printf("loading audio files\n");
    AudioLoad_LoadFiles();
    printf("\tdone.\n");

    wav_init();

    gVIsPerFrame = 0;
    gSysFrameCount = 0;
    gStartNMI = false;
    gStopTasks = false;
    gFillScreenColor = 0;
    gFillScreen = false;
    gCurrentTask = NULL;

    for (i = 0; i < ARRAY_COUNT(sAudioTasks); i += 1) {
        sAudioTasks[i] = NULL;
    }
    for (i = 0; i < ARRAY_COUNT(sGfxTasks); i += 1) {
        sGfxTasks[i] = NULL;
    }
    for (i = 0; i < ARRAY_COUNT(sNewAudioTasks); i += 1) {
        sNewAudioTasks[i] = NULL;
    }
    for (i = 0; i < ARRAY_COUNT(sNewGfxTasks); i += 1) {
        sNewGfxTasks[i] = NULL;
    }

    Lib_FillScreen(1);

    kthread_attr_t main_attr5;
    main_attr5.create_detached = 1;
    main_attr5.stack_size = 32768;
    main_attr5.stack_ptr = NULL;
    main_attr5.prio = 11;
    main_attr5.label = "SPINNING";
    thd_create_ex(&main_attr5, &AudioThread, arg0);

    Controller_Init();
    Main_InitMesgQueues();

    gfx_init(wm_api, rendering_api, "Star Fox 64", false);

    _AudioInit();
    AudioLoad_Init();
    Audio_InitSounds();
    vblank_handler_add(&vblfunc, NULL);
    Game_Initialize();

    osSendMesg(&gSerialThreadMesgQueue, (OSMesg) SI_READ_CONTROLLER, OS_MESG_NOBLOCK);
    Graphics_InitializeTask(gSysFrameCount);
    {
        gSPDisplayList(gMasterDisp++, gGfxPool->unkDL1);
        Game_Update();
        gSPEndDisplayList(gUnkDisp1++);
        gSPEndDisplayList(gUnkDisp2++);
        gSPDisplayList(gMasterDisp++, gGfxPool->unkDL2);
        gSPEndDisplayList(gMasterDisp++);
    }
    Graphics_SetTask();

    while (true) {
        if (MQ_GET_MESG(&gTimerTaskMesgQueue, &sp24)) {
            Timer_CompleteTask(sp24);
        }

        gfx_start_frame();
        gSysFrameCount++;
        Graphics_InitializeTask(gSysFrameCount);
        Controller_UpdateInput();
        gSPSegment(gUnkDisp1++, 0, 0);
        gSPDisplayList(gMasterDisp++, gGfxPool->unkDL1);
        Game_Update();

        gSPEndDisplayList(gUnkDisp1++);
        gSPEndDisplayList(gUnkDisp2++);
        gSPDisplayList(gMasterDisp++, gGfxPool->unkDL2);
        gDPFullSync(gMasterDisp++);
        gSPEndDisplayList(gMasterDisp++);
        Graphics_SetTask();
        gfx_end_frame();
        Audio_Update();
        Controller_Rumble();
        thd_pass();
    }
}

#if defined(SDCARD_SUPPORT)
void sdcard_init(void) {
    uint8_t partition_type;

    if (sd_init())
        return;

    if (sd_blockdev_for_partition(0, &dev, &partition_type))
        return;

    if (fs_fat_mount("/sd", &dev, FS_FAT_MOUNT_READWRITE))
        return;

    printf("mounted sd card at /sd\n");
}
#endif

#if defined(IDE_SUPPORT)
void ide_init(void) {
    uint8_t partition_type;

    if (g1_ata_init())
        return;

    if (g1_ata_blockdev_for_partition(0, 1, &dev, &partition_type))
        return;

    if (fs_fat_mount("/ide", &dev, FS_FAT_MOUNT_READWRITE))
        return;

    printf("mounted ide partition at /ide\n");
}
#endif

int main(int argc, char **argv) {
    printf("Searching for assets...\n");

    FILE* fntest;

    printf("/pc: ");
    fntest = fopen("/pc/sf_data/logo.bin", "rb");
    if (fntest) {
        printf("found. Using /pc for assets.\n");
        fnpre = "/pc";
        goto assetsfound;
    }
    printf("not found.\n");

    printf("/cd: ");
    fntest = fopen("/cd/sf_data/logo.bin", "rb");
    if (fntest) {
        printf("found. Using /cd for assets.\n");
        fnpre = "/cd";
        goto assetsfound;
    }
    printf("not found.\n");

#if defined(IDE_SUPPORT) || defined(SDCARD_SUPPORT)
    fs_fat_init();
#endif

#if defined(SDCARD_SUPPORT)
    printf("/sd/sf64-dc: ");
    sdcard_init();

    fntest = fopen("/sd/sf64-dc/sf_data/logo.bin", "rb");
    if (fntest) {
        printf("found. Using /sd/sf64-dc for assets.\n");
        fnpre = "/sd/sf64-dc";
        goto assetsfound;
    }
    printf("not found.\n");
#endif

#if defined(IDE_SUPPORT)
    printf("/ide/sf64-dc: ");
    ide_init();

    fntest = fopen("/ide/sf64-dc/sf_data/logo.bin", "rb");
    if (fntest) {
        printf("found. Using /ide/sf64-dc for assets.\n");
        fnpre = "/ide/sf64-dc";
        goto assetsfound;
    }
    printf("not found.\n");
#endif

    printf("Couldn't find assets, quitting...\n");
    exit(-1);

assetsfound:
    fclose(fntest);

    Main_ThreadEntry(NULL);

#if defined(IDE_SUPPORT)
    fs_fat_unmount("/ide");
    g1_ata_shutdown();
#endif

#if defined(SDCARD_SUPPORT)
    fs_fat_unmount("/sd");
    sd_shutdown();
#endif

#if defined(IDE_SUPPORT) || defined(SDCARD_SUPPORT)
    fs_fat_shutdown();
#endif

    return 0;
}

#if MODS_ISVIEWER == 1
#include "../mods/isviewer.c"
#endif

extern int USE_MIXER_MUSIC;

void* AudioThread(UNUSED void* arg) {
    uint64_t last_vbltick = vblticker;

    while (1) {
        while (vblticker <= last_vbltick)
            genwait_wait((void*) &vblticker, NULL, 5, NULL);

        last_vbltick = vblticker;

#if !USE_16KHZ && !USE_32KHZ
        int samplecount = ((gSysFrameCount & 3) < 2) ? SAMPLES_HIGH : SAMPLES_LOW;
#else
        int samplecount = SAMPLES_LOW;
        if ((gSysFrameCount & 3) == 0)
            samplecount = SAMPLES_HIGH;
#endif
        if (USE_MIXER_MUSIC)
            irq_disable();
        AudioThread_CreateNextAudioBuffer(audio_buffer[0], audio_buffer[1], samplecount);
        audio_api->play((u8*) audio_buffer[0], (u8*) audio_buffer[1], samplecount * 4);
        if (USE_MIXER_MUSIC)
            irq_enable();
    }

    return NULL;
}
