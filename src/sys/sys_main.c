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

#if USE_32KHZ
#define SAMPLES_HIGH 560
#define SAMPLES_LOW 528
#else
#define SAMPLES_HIGH 464
#define SAMPLES_LOW 432
#endif

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
s16 audio_buffer[2][SAMPLES_HIGH * 2 * 2 * 3] __attribute__((aligned(64)));
static struct AudioAPI *audio_api = NULL;
void AudioThread_CreateNextAudioBuffer(s16* samplesL, s16* samplesR, u32 num_samples);

void *AudioThread(UNUSED void *arg);

static volatile uint64_t vblticker=0;

void vblfunc(uint32_t c, void *d) {
	(void)c;
	(void)d;
    vblticker++;
    osSendMesg(&gGfxVImesgQueue, (OSMesg)NULL, OS_MESG_NOBLOCK);
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

// borrowed from skmp / DCA3
// thanks
extern const char etext[];
__attribute__((noinline)) void stacktrace() {
	uint32 sp=0, pr=0;
	__asm__ __volatile__(
		"mov	r15,%0\n"
		"sts	pr,%1\n"
		: "+r" (sp), "+r" (pr)
		:
		: );
	printf("[ %08X ", (uintptr_t)pr);
	int found = 0;
	if(!(sp & 3) && sp > 0x8c000000 && sp < 0x8d000000) {
		char** sp_ptr = (char**)sp;
		for (int so = 0; so < 16384; so++) {
			if ((uintptr_t)(&sp_ptr[so]) >= 0x8d000000) {
				//printf("(@@%08X) ", (uintptr_t)&sp_ptr[so]);
				break;
			}
			if (sp_ptr[so] > (char*)0x8c000000 && sp_ptr[so] < etext) {
				uintptr_t addr = (uintptr_t)(sp_ptr[so]);
				// candidate return pointer
				if (addr & 1) {
					// dbglog(DBG_CRITICAL, "Stack trace: %p (@%p): misaligned\n", (void*)sp_ptr[so], &sp_ptr[so]);
					continue;
				}

				uint16_t* instrp = (uint16_t*)addr;

				uint16_t instr = instrp[-2];
				// BSR or BSRF or JSR @Rn ?
				if (((instr & 0xf000) == 0xB000) || ((instr & 0xf0ff) == 0x0003) || ((instr & 0xf0ff) == 0x400B)) {
					printf("%08X ", (uintptr_t)instrp);
					if (found++ > 24) {
						//printf("(@%08X) ", (uintptr_t)&sp_ptr[so]);
						break;
					}
				} else {
					// dbglog(DBG_CRITICAL, "%p:%04X ", instrp, instr);
				}
			} else {
				// dbglog(DBG_CRITICAL, "Stack trace: %p (@%p): out of range\n", (void*)sp_ptr[so], &sp_ptr[so]);
			}
		}
		printf("]\n");
	} else {
		printf("%08x ]\n", (uintptr_t)sp);
	}
}

extern u8 seg_used[15];
void *virtual_to_segmented(const void *addr) {
    unsigned int uip_addr = (unsigned int) addr;

    uint32_t lowest_dist = 0xffffffff;
    uint32_t lowest_index = 0xffffffff;

    for (int i=15;i>0;i--) {
        if (seg_used[i] == 0)
            continue;
        if ((uip_addr >= gSegments[i]) && (uip_addr <= (gSegments[i] + 0xffffff))) {
            if ((uip_addr - gSegments[i]) < lowest_dist) {
                lowest_dist = (uip_addr - gSegments[i]);
                lowest_index = i;
            }
        }
    }

    if (lowest_index != 0xffffffff) {
        return (void*)((uip_addr - gSegments[lowest_index]) | ((lowest_index+1) << 24));
    }

    return uip_addr;
}


void* segmented_to_virtual(const void* addr) {
    unsigned int uip_addr = (unsigned int) addr;

    if ((uip_addr >= 0x8c010000) && (uip_addr <= 0x8cffffff)) {
        return uip_addr;
    }

    unsigned int segment = (unsigned int) (uip_addr >> 24);// & 0x0f;

    unsigned int offset = (unsigned int) uip_addr & 0x00FFFFFF;
    u32 translated_addr = gSegments[segment] + offset;
    return (void*)translated_addr;
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
    //osSetEventMesg(OS_EVENT_SI, &gSerialEventQueue, NULL);
    osCreateMesgQueue(&gMainThreadMesgQueue, sMainThreadMsgBuff, ARRAY_COUNT(sMainThreadMsgBuff));
    //osViSetEvent(&gMainThreadMesgQueue, (OSMesg) EVENT_MESG_VI, 1);
    //osSetEventMesg(OS_EVENT_SP, &gMainThreadMesgQueue, (OSMesg) EVENT_MESG_SP);
    //osSetEventMesg(OS_EVENT_DP, &gMainThreadMesgQueue, (OSMesg) EVENT_MESG_DP);
    //osSetEventMesg(OS_EVENT_PRENMI, &gMainThreadMesgQueue, (OSMesg) EVENT_MESG_PRENMI);
    osCreateMesgQueue(&gTimerTaskMesgQueue, sTimerTaskMsgBuff, ARRAY_COUNT(sTimerTaskMsgBuff));
    osCreateMesgQueue(&gTimerWaitMesgQueue, sTimerWaitMsgBuff, ARRAY_COUNT(sTimerWaitMsgBuff));
    osCreateMesgQueue(&gSerialThreadMesgQueue, sSerialThreadMsgBuff, ARRAY_COUNT(sSerialThreadMsgBuff));
    osCreateMesgQueue(&gControllerMesgQueue, sControllerMsgBuff, ARRAY_COUNT(sControllerMsgBuff));
    osCreateMesgQueue(&gSaveMesgQueue, sSaveMsgBuff, ARRAY_COUNT(sSaveMsgBuff));
}

int ever_init_wav = 0;

#include <dc/sound/sound.h>
#include <dc/sound/stream.h>
#include "sndwav.h"
#include <stdio.h>
#include <stdlib.h>
#include <kos/thread.h>
#include <kos.h>
#include "james2.xbm"

vmufb_t vmubuf;

void Main_ThreadEntry(void* arg0) {
    OSMesg osMesg;
    u8 mesg;
    void* sp24;
    u8 i;
    u8 visPerFrame;
    u8 validVIsPerFrame;

    maple_device_t* dev = NULL;
    if ((dev = maple_enum_type(0, MAPLE_FUNC_LCD))) {
        vmufb_paint_xbm(&vmubuf, 0, 0, 48, 32, james_bits);
        // only draw to first vmu
        vmufb_present(&vmubuf, dev);
    }

    printf("loading audio files\n");
    AudioLoad_LoadFiles();
    printf("\tdone.\n");

    if (ever_init_wav == 0) {
        ever_init_wav = 1;
        wav_init();
    }

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

#if defined(MEMTEST)
    for (int mi = 0; mi < 8 * 1048576; mi += 65536) {
        void* test_m = malloc(mi);
        if (test_m != NULL) {
            free(test_m);
            test_m = NULL;
            continue;
        } else {
            int bi = mi - 65536;
            for (; bi < 8 * 1048576; bi++) {
                test_m = malloc(bi);
                if (test_m != NULL) {
                    free(test_m);
                    test_m = NULL;
                    continue;
                } else {
                    printf("free ram for malloc: %d\n", bi);
                    goto run_game_loop;
                }
            }
        }
    }
run_game_loop:
#endif

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

int main(int argc, char **argv) {
    FILE* fntest = fopen("/pc/sf_data/logo.bin", "rb");
    if (NULL == fntest) {
        fntest = fopen("/cd/sf_data/logo.bin", "rb");
        if (NULL == fntest) {
            printf("Cant load from /pc or /cd");
            printf("\n");
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

    Main_ThreadEntry(NULL);

    return 0;
}

#if MODS_ISVIEWER == 1
#include "../mods/isviewer.c"
#endif

void* AudioThread(UNUSED void* arg) {
    uint64_t last_vbltick = vblticker;

    while (1) {
#if USE_32KHZ
        while (vblticker <= last_vbltick + 1)
            genwait_wait((void*) &vblticker, NULL, 15, NULL);
#else
        while (vblticker <= last_vbltick)
            genwait_wait((void*)&vblticker, NULL, 5, NULL);
#endif

        last_vbltick = vblticker;

#if USE_32KHZ
        int samplecount = gSysFrameCount & 1 ? SAMPLES_HIGH : SAMPLES_LOW;
        AudioThread_CreateNextAudioBuffer(audio_buffer[0], audio_buffer[1], samplecount);
        AudioThread_CreateNextAudioBuffer(audio_buffer[0] + (samplecount), audio_buffer[1] + (samplecount),
                                          samplecount);

        audio_api->play((u8*) audio_buffer[0], (u8*) audio_buffer[1], samplecount * 8);
#else
        AudioThread_CreateNextAudioBuffer(audio_buffer[0], audio_buffer[1], 448);
        audio_api->play((u8 *)audio_buffer[0], (u8*)audio_buffer[1], 1792);
#endif
    }

    return NULL;
}
