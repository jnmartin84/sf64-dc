#include "n64sys.h"
#include "sf64audio_external.h"
//#include "mods.h"
#include <stdio.h>
#include <kos/thread.h>
#include <kos/genwait.h>
#include <dc/vblank.h>
#include "gfx/gfx_pc.h"
#include "gfx/gfx_opengl.h"
#include "gfx/gfx_dc.h"
#include <stdlib.h>

//#define SAMPLES_HIGH 533
#define SAMPLES_HIGH 560
#define SAMPLES_LOW 528

uintptr_t arch_stack_32m = 0x8d000000;

extern struct GfxWindowManagerAPI gfx_glx;
extern struct GfxRenderingAPI gfx_opengl_api;
static struct GfxWindowManagerAPI *wm_api = &gfx_dc;
static struct GfxRenderingAPI *rendering_api = &gfx_opengl_api;

extern void gfx_run(Gfx *commands);
char *fnpre;
#include "src/dcaudio/audio_api.h"
#include "src/dcaudio/audio_dc.h"
volatile int inited = 0;
extern struct AudioAPI audio_dc;
s16 audio_buffer[SAMPLES_HIGH * 2 * 2 * 3] __attribute__((aligned(64)));
static struct AudioAPI *audio_api = NULL;
void AudioThread_CreateNextAudioBuffer(s16* samples, u32 num_samples);

void *SPINNING_THREAD(UNUSED void *arg);

static volatile uint64_t vblticker=0;

void vblfunc(uint32_t c, void *d) {
	(void)c;
	(void)d;
    vblticker++;
    genwait_wake_one((void *)&vblticker);
}

void _AudioInit(void) {
    if (audio_api == NULL) {
        audio_api = &audio_dc;
        audio_api->init();
    }
}


s32 sGammaMode = 1;

SPTask* gCurrentTask;
SPTask* sAudioTasks[1];
SPTask* sGfxTasks[2];
#ifdef AVOID_UB
SPTask* sNewAudioTasks[2];
#else
SPTask* sNewAudioTasks[1];
#endif
SPTask* sNewGfxTasks[2];

// make sure that a segment base address is always valid before the game starts
u32 gSegments[16] = {
0x8c010000,
0x8c010000,
0x8c010000,
0x8c010000,
0x8c010000,
0x8c010000,
0x8c010000,
0x8c010000,
0x8c010000,
0x8c010000,
0x8c010000,
0x8c010000,
0x8c010000,
0x8c010000,
0x8c010000,
0x8c010000,
};
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
u16* gTextureRender;

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
OSThread gAudioThread;           // 8013B1F0
u8 gAudioThreadStack[0x1000];    // 800DDAA0
OSThread gGraphicsThread;        // 800DEAA0
u8 gGraphicsThreadStack[0x1000]; // 800DEC50
OSThread gTimerThread;           // 800DFC50
u8 gTimerThreadStack[0x1000];    // 800DFE00
OSThread gSerialThread;          // 800E0E00
u8 gSerialThreadStack[0x1000];   // 800E0FB0

void Main_Initialize(void) {
    u8 i;
//printf("%s()\n", __func__);

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
}

void *Audio_ThreadEntry(void* arg0) {
    SPTask* task;
//printf("%s()\n", __func__);
inited = 1;
while(1) {
thd_sleep(60000);
//    thd_pass();
}
    #if 0
    task = AudioThread_CreateTask();
    if (task != NULL) {
        task->mesgQueue = &gAudioTaskMesgQueue;
        task->msg = (OSMesg) TASK_MESG_1;
        osWritebackDCacheAll();
        osSendMesg(&gTaskMesgQueue, task, OS_MESG_NOBLOCK);
    }

    while (true) {
        task = AudioThread_CreateTask();
        if (task != NULL) {
            task->mesgQueue = &gAudioTaskMesgQueue;
            task->msg = (OSMesg) TASK_MESG_1;
            osWritebackDCacheAll();
        }
        MQ_GET_MESG(&gAudioTaskMesgQueue, NULL);

        if (task != NULL) {
            osSendMesg(&gTaskMesgQueue, task, OS_MESG_NOBLOCK);
        }
        MQ_WAIT_FOR_MESG(&gAudioVImesgQueue, NULL);
    }
#endif
    return NULL;
}

volatile int called = 0;

void Graphics_SetTask(void) {
    //printf("%s()\n", __func__);
called++;
    gGfxTask->mesgQueue = &gGfxTaskMesgQueue;
    gGfxTask->msg = (OSMesg) TASK_MESG_2;
    gGfxTask->task.t.type = M_GFXTASK;
    gGfxTask->task.t.flags = 0;
    gGfxTask->task.t.ucode_boot = NULL; //rspbootTextStart;
    gGfxTask->task.t.ucode_boot_size = 0;//(uintptr_t) rspbootTextEnd - (uintptr_t) rspbootTextStart;
    gGfxTask->task.t.ucode = NULL;//gspF3DEX_fifoTextStart;
    gGfxTask->task.t.ucode_size = SP_UCODE_SIZE;
    gGfxTask->task.t.ucode_data = NULL;//(u64*) &gspF3DEX_fifoDataStart;
    gGfxTask->task.t.ucode_data_size = SP_UCODE_DATA_SIZE;
    gGfxTask->task.t.dram_stack = gDramStack;
    gGfxTask->task.t.dram_stack_size = SP_DRAM_STACK_SIZE8;
    gGfxTask->task.t.output_buff = (u64*) gTaskOutputBuffer;
    gGfxTask->task.t.output_buff_size = (u64*) gAudioHeap;
    gGfxTask->task.t.data_ptr = (u64*) gGfxPool->masterDL;
    gGfxTask->task.t.data_size = (gMasterDisp - gGfxPool->masterDL) * sizeof(Gfx);
    gGfxTask->task.t.yield_data_ptr = (u64*) &gOSYieldData;
    gGfxTask->task.t.yield_data_size = OS_YIELD_DATA_SIZE;
//    osWritebackDCacheAll();
//    osSendMesg(&gTaskMesgQueue, gGfxTask, OS_MESG_NOBLOCK);
if (called > 5) {
        gfx_run(gGfxPool->masterDL);

}
}

void Graphics_InitializeTask(u32 frameCount) {
    //printf("%s()\n", __func__);

    gGfxPool = &gGfxPools[frameCount % 2];

    gGfxTask = &gGfxPool->task;
    gViewport = gGfxPool->viewports;
    gGfxMtx = gGfxPool->mtx;
    gUnkDisp1 = gGfxPool->unkDL1;
    gMasterDisp = gGfxPool->masterDL;
    gUnkDisp2 = gGfxPool->unkDL2;
    gLight = gGfxPool->lights;

    gFrameBuffer = &gFrameBuffers[frameCount % 3];
    gTextureRender = &gTextureRenderBuffer[0];

    gGfxMatrix = &sGfxMatrixStack[0];
    gCalcMatrix = &sCalcMatrixStack[0];

    D_80178710 = &D_80178580[0];
}

void Main_SetVIMode(void) {
    if ((gControllerHold[0].button & D_JPAD) && (gControllerHold[1].button & D_JPAD) &&
        (gControllerHold[2].button & D_JPAD) && (gControllerHold[3].button & L_TRIG) &&
        (gControllerHold[3].button & R_TRIG) && (gControllerHold[3].button & Z_TRIG)) {
        sGammaMode = 1 - sGammaMode;
    }
#if 0
    switch (osTvType) {
        case OS_TV_PAL:
            osViSetMode(&osViModePalLan1);
            break;
        case OS_TV_MPAL:
            osViSetMode(&osViModeMpalLan1);
            break;
        default:
        case OS_TV_NTSC:
            osViSetMode(&osViModeNtscLan1);
            break;
    }

    if (sGammaMode != 0) {
        osViSetSpecialFeatures(OS_VI_DITHER_FILTER_ON | OS_VI_DIVOT_OFF | OS_VI_GAMMA_ON | OS_VI_GAMMA_DITHER_ON);
    } else {
        osViSetSpecialFeatures(OS_VI_DITHER_FILTER_ON | OS_VI_DIVOT_OFF | OS_VI_GAMMA_OFF | OS_VI_GAMMA_DITHER_OFF);
    }
#endif        
}

void *SerialInterface_ThreadEntry(void* arg0) {
    OSMesg sp34;
//printf("%s()\n", __func__);

    Controller_Init();
    while (true) {
        #if 0
//        printf("z");
        //MQ_WAIT_FOR_MESG(&gSerialThreadMesgQueue, &sp34);
if(MQ_GET_MESG(&gSerialThreadMesgQueue, &sp34)) {
            switch ((s32) sp34) {
            case SI_READ_CONTROLLER:
                Controller_ReadData();
                break;
            case SI_READ_SAVE:
                Save_ReadData();
                break;
            case SI_WRITE_SAVE:
                Save_WriteData();
                break;
            case SI_RUMBLE:
                Controller_Rumble();
                break;
        }
    }
    #endif
thd_sleep(60000);
}
    return NULL;
}

void *Timer_ThreadEntry(void* arg0) {
    void* sp24;
//printf("%s()\n", __func__);

    while (true) {
//                        printf("y");

//        MQ_WAIT_FOR_MESG(&gTimerTaskMesgQueue, &sp24);
if(MQ_GET_MESG(&gTimerTaskMesgQueue, &sp24)) {

Timer_CompleteTask(sp24);
}
thd_pass();
}

    return NULL;
}

void Main_InitMesgQueues(void) {
    //printf("%s()\n",__func__);
    osCreateMesgQueue(&gDmaMesgQueue, sDmaMsgBuff, ARRAY_COUNT(sDmaMsgBuff));
    osCreateMesgQueue(&gTaskMesgQueue, sTaskMsgBuff, ARRAY_COUNT(sTaskMsgBuff));
    osCreateMesgQueue(&gAudioVImesgQueue, sAudioVImsgBuff, ARRAY_COUNT(sAudioVImsgBuff));
    osCreateMesgQueue(&gAudioTaskMesgQueue, sAudioTaskMsgBuff, ARRAY_COUNT(sAudioTaskMsgBuff));
    osCreateMesgQueue(&gGfxVImesgQueue, sGfxVImsgBuff, ARRAY_COUNT(sGfxVImsgBuff));
    osCreateMesgQueue(&gGfxTaskMesgQueue, sGfxTaskMsgBuff, ARRAY_COUNT(sGfxTaskMsgBuff));
    osCreateMesgQueue(&gSerialEventQueue, sSerialEventBuff, ARRAY_COUNT(sSerialEventBuff));
//    osSetEventMesg(OS_EVENT_SI, &gSerialEventQueue, NULL);
    osCreateMesgQueue(&gMainThreadMesgQueue, sMainThreadMsgBuff, ARRAY_COUNT(sMainThreadMsgBuff));
 //   osViSetEvent(&gMainThreadMesgQueue, (OSMesg) EVENT_MESG_VI, 1);
   // osSetEventMesg(OS_EVENT_SP, &gMainThreadMesgQueue, (OSMesg) EVENT_MESG_SP);
   // osSetEventMesg(OS_EVENT_DP, &gMainThreadMesgQueue, (OSMesg) EVENT_MESG_DP);
   // osSetEventMesg(OS_EVENT_PRENMI, &gMainThreadMesgQueue, (OSMesg) EVENT_MESG_PRENMI);
    osCreateMesgQueue(&gTimerTaskMesgQueue, sTimerTaskMsgBuff, ARRAY_COUNT(sTimerTaskMsgBuff));
    osCreateMesgQueue(&gTimerWaitMesgQueue, sTimerWaitMsgBuff, ARRAY_COUNT(sTimerWaitMsgBuff));
    osCreateMesgQueue(&gSerialThreadMesgQueue, sSerialThreadMsgBuff, ARRAY_COUNT(sSerialThreadMsgBuff));
    osCreateMesgQueue(&gControllerMesgQueue, sControllerMsgBuff, ARRAY_COUNT(sControllerMsgBuff));
    osCreateMesgQueue(&gSaveMesgQueue, sSaveMsgBuff, ARRAY_COUNT(sSaveMsgBuff));
}

void Main_HandleRDP(void) {
#if 0
    SPTask** task = sGfxTasks;
    u8 i;

    if ((*task)->mesgQueue != NULL) {
        osSendMesg((*task)->mesgQueue, (*task)->msg, OS_MESG_NOBLOCK);
    }
    (*task)->state = SPTASK_STATE_FINISHED_DP;

    for (i = 0; i < ARRAY_COUNT(sGfxTasks) - 1; i += 1, task++) {
        *task = *(task + 1);
    }
    *task = NULL;
#endif
    }

void Main_HandleRSP(void) {
    #if 0
    SPTask* task = gCurrentTask;

    gCurrentTask = NULL;
    if (task->state == SPTASK_STATE_INTERRUPTED) {
        //if (osSpTaskYielded(&task->task) == 0) {
          //  task->state = SPTASK_STATE_FINISHED;
        //}
    } else {
        task->state = SPTASK_STATE_FINISHED;
        if (task->task.t.type == M_AUDTASK) {
            if (task->mesgQueue != NULL) {
                osSendMesg(task->mesgQueue, task->msg, OS_MESG_NOBLOCK);
            }
            sAudioTasks[0] = NULL;
        }
    }
        #endif
}

void Main_GetNewTasks(void) {
    u8 i;
    SPTask** audioTask;
    SPTask** gfxTask;
    SPTask** newAudioTask;
    SPTask** newGfxTask;
    OSMesg spTaskMsg;
    SPTask* newTask;

    newAudioTask = sNewAudioTasks;
    newGfxTask = sNewGfxTasks;
    for (i = 0; i < ARRAY_COUNT(sNewAudioTasks); i += 1) {
        *(newAudioTask++) = NULL;
    }
    for (i = 0; i < ARRAY_COUNT(sNewGfxTasks); i += 1) {
        *(newGfxTask++) = NULL;
    }

    newAudioTask = sNewAudioTasks;
    newGfxTask = sNewGfxTasks;
    while (MQ_GET_MESG(&gTaskMesgQueue, &spTaskMsg)) {
        newTask = (SPTask*) spTaskMsg;
        newTask->state = SPTASK_STATE_NOT_STARTED;

        switch (newTask->task.t.type) {
            case M_AUDTASK:
                *(newAudioTask++) = newTask;
                break;
            case M_GFXTASK:
                *(newGfxTask++) = newTask;
                break;
        }
    }
    newAudioTask = sNewAudioTasks;
    newGfxTask = sNewGfxTasks;
    audioTask = sAudioTasks;
    gfxTask = sGfxTasks;

    for (i = 0; i < ARRAY_COUNT(sAudioTasks); i += 1, audioTask++) {
        if (*audioTask == NULL) {
            break;
        }
    }
    for (i; i < ARRAY_COUNT(sAudioTasks); i += 1) {
        *(audioTask++) = *(newAudioTask++);
    }

    for (i = 0; i < ARRAY_COUNT(sGfxTasks); i += 1, gfxTask++) {
        if (*gfxTask == NULL) {
            break;
        }
    }
    for (i; i < ARRAY_COUNT(sGfxTasks); i += 1) {
        *(gfxTask++) = *(newGfxTask++);
    }
}

void Main_StartNextTask(void) {
    #if 0
    if (sAudioTasks[0] != NULL) {
        if (gCurrentTask != NULL) {
            if (gCurrentTask->task.t.type == M_GFXTASK) {
                gCurrentTask->state = SPTASK_STATE_INTERRUPTED;
                //osSpTaskYield();
            }
        } else {
            gCurrentTask = sAudioTasks[0];
         //   osSpTaskLoad(&gCurrentTask->task);
           // osSpTaskStartGo(&gCurrentTask->task);
            gCurrentTask->state = SPTASK_STATE_RUNNING;
        }
    } else if ((gCurrentTask == NULL) && (sGfxTasks[0] != NULL) && (sGfxTasks[0]->state != SPTASK_STATE_FINISHED)) {
        gCurrentTask = sGfxTasks[0];
//        osDpSetStatus(DPC_CLR_TMEM_CTR | DPC_CLR_PIPE_CTR | DPC_CLR_CMD_CTR | DPC_CLR_CLOCK_CTR);
  //      osSpTaskLoad(&gCurrentTask->task);
    //    osSpTaskStartGo(&gCurrentTask->task);
        gCurrentTask->state = SPTASK_STATE_RUNNING;
    }
        
        #endif

    }


void Main_ThreadEntry(void* arg0) {
    OSMesg osMesg;
    u8 mesg;

    kthread_attr_t main_attr3;
    main_attr3.create_detached = 1;
	main_attr3.stack_size = 32768;
	main_attr3.stack_ptr = NULL;
	main_attr3.prio = 11;
	main_attr3.label = "SerialInterface";
    thd_create_ex(&main_attr3, &SerialInterface_ThreadEntry, arg0);

    kthread_attr_t main_attr2;
    main_attr2.create_detached = 1;
	main_attr2.stack_size = 32768;
	main_attr2.stack_ptr = NULL;
	main_attr2.prio = 11;
	main_attr2.label = "Timer";
    thd_create_ex(&main_attr2, &Timer_ThreadEntry, arg0);

    kthread_attr_t main_attr5;
    main_attr5.create_detached = 1;
	main_attr5.stack_size = 32768;
	main_attr5.stack_ptr = NULL;
	main_attr5.prio = 11;
	main_attr5.label = "SPINNING";
    thd_create_ex(&main_attr5, &SPINNING_THREAD, arg0);

    Main_InitMesgQueues();


{
    u8 i;
    u8 visPerFrame;
    u8 validVIsPerFrame;
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
gfx_start_frame();
        gSysFrameCount++;
        Graphics_InitializeTask(gSysFrameCount);
        Controller_UpdateInput();
        Controller_Rumble();
        {
            gSPSegment(gUnkDisp1++, 0, 0);
            gSPDisplayList(gMasterDisp++, gGfxPool->unkDL1);
            Game_Update();
//            printf("did yo uget out\n");
//            if (gStartNMI == 1) {
//                Graphics_NMIWipe();
  //          }
            gSPEndDisplayList(gUnkDisp1++);
            gSPEndDisplayList(gUnkDisp2++);
            gSPDisplayList(gMasterDisp++, gGfxPool->unkDL2);
            gDPFullSync(gMasterDisp++);
            gSPEndDisplayList(gMasterDisp++);
        }
//        MQ_WAIT_FOR_MESG(&gGfxTaskMesgQueue, NULL);

        Graphics_SetTask();

        if (!gFillScreen) {
////            osViSwapBuffer(&gFrameBuffers[(gSysFrameCount - 1) % 3]);
        }

    //    Fault_SetFrameBuffer(&gFrameBuffers[(gSysFrameCount - 1) % 3], SCREEN_WIDTH, 16);

        visPerFrame = MIN(gVIsPerFrame, 4);
        validVIsPerFrame = MAX(visPerFrame, gGfxVImesgQueue.validCount + 1);
        for (i = 0; i < validVIsPerFrame; i += 1) { // Can't be ++
      //      MQ_WAIT_FOR_MESG(&gGfxVImesgQueue, NULL);
        }

        Audio_Update();
gfx_end_frame();     
thd_pass();
    }    
}
}

void Idle_ThreadEntry(void* arg0) {
    Main_SetVIMode();
    Lib_FillScreen(1);
    Main_ThreadEntry(NULL);
    Fault_Init();
    osSetThreadPri(NULL, OS_PRIORITY_IDLE);
    for (;;) {}
}

extern void AudioLoad_LoadFiles(void);

typedef struct {                                  // Little Endian
    /* 0x00 */ uint8_t strongLeft : 1;            // 0000 0001
    /* 0x00 */ uint8_t strongRight : 1;           // 0000 0010
    /* 0x00 */ uint8_t bit2 : 2;                  // 0000 0100
    /* 0x00 */ uint8_t unused : 2;                // 0001 0000
    /* 0x00 */ uint8_t usesHeadsetPanEffects : 1; // 0100 0000
    /* 0x00 */ uint8_t stereoHeadsetEffects : 1;  // 1000 0000
} StereoDataTest;

typedef union {
    StereoDataTest data;
    uint8_t raw;
} StereoUnionTest;

void testBits(void) {
    StereoUnionTest test;

    test.raw = 0; // Clear all bits
    test.data.stereoHeadsetEffects = 1;

    printf("Raw byte value: 0x%08X\n", test.raw);
}

//void bootproc(void) {
int main(int argc, char **argv) {
    //RdRam_CheckIPL3();
//    osInitialize();
//#if MODS_ISVIEWER == 1
//    ISViewer_Init();
//#endif
//printf("%s(%d, %08x)\n", __func__, argc, argv);
//testBits();
//exit(0);
    FILE* fntest = fopen("/pc/sf_data/ast_logo.bin", "rb");
    if (NULL == fntest) {
        fntest = fopen("/cd/sf_data/ast_logo.bin", "rb");
        if (NULL == fntest) {
            printf("Cant load from /pc or /cd");
            printf("\n");
//            while(1){}
           exit(-1);
        } else {
            printf("             using /cd for assets\n");
            fnpre = "/cd";
        }
    } else {
        printf("             using /pc for assets\n");
        fnpre = "/pc";
    }

    fclose(fntest);
    printf("loading audio files\n");
AudioLoad_LoadFiles();
printf("\tdone.\n");
Main_Initialize();


//    osCreateThread(&sIdleThread, THREAD_ID_IDLE, &Idle_ThreadEntry, NULL, sIdleThreadStack + sizeof(sIdleThreadStack),
  //                 255);
   // osStartThread(&sIdleThread);
   Idle_ThreadEntry(NULL);
    while (1) {
        thd_pass();
    }
    return 0;
}

#if MODS_ISVIEWER == 1
#include "../mods/isviewer.c"
#endif
//533
void *SPINNING_THREAD(UNUSED void *arg) {
    uint64_t last_vbltick = vblticker;

    while (1) {
//        {
  //          irq_disable_scoped();
            while (vblticker <= last_vbltick)
                genwait_wait((void*)&vblticker, NULL, 15, NULL);
    //    }

        last_vbltick = vblticker;

        int num_samples = 448;//540;//called & 1 ? SAMPLES_HIGH : SAMPLES_LOW;

        irq_disable();
        AudioThread_CreateNextAudioBuffer(audio_buffer, num_samples);
//        AudioThread_CreateNextAudioBuffer(audio_buffer + (num_samples * 2), num_samples);
        irq_enable();
        audio_api->play((u8 *)audio_buffer, (num_samples * 2 * 2 /* * 2 */));
    }

    return NULL;
}