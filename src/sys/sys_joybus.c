#include "n64sys.h"


#define N64_CONT_A 0x8000
#define N64_CONT_B 0x4000
#define N64_CONT_G 0x2000
#define N64_CONT_START 0x1000
#define N64_CONT_UP 0x0800
#define N64_CONT_DOWN 0x0400
#define N64_CONT_LEFT 0x0200
#define N64_CONT_RIGHT 0x0100
#define N64_CONT_L 0x0020
#define N64_CONT_R 0x0010
#define N64_CONT_E 0x0008
#define N64_CONT_D 0x0004
#define N64_CONT_C 0x0002
#define N64_CONT_F 0x0001

/* Nintendo's official button names */
#undef A_BUTTON 
#undef B_BUTTON
#undef L_TRIG
#undef R_TRIG
#undef Z_TRIG
#undef START_BUTTON
#undef U_JPAD
#undef L_JPAD
#undef R_JPAD
#undef D_JPAD
#undef U_CBUTTONS
#undef L_CBUTTONS
#undef R_CBUTTONS
#undef D_CBUTTONS

#define A_BUTTON N64_CONT_A
#define B_BUTTON N64_CONT_B
#define L_TRIG N64_CONT_L
#define R_TRIG N64_CONT_R
#define Z_TRIG N64_CONT_G
#define START_BUTTON N64_CONT_START
#define U_JPAD N64_CONT_UP
#define L_JPAD N64_CONT_LEFT
#define R_JPAD N64_CONT_RIGHT
#define D_JPAD N64_CONT_DOWN
#define U_CBUTTONS N64_CONT_E
#define L_CBUTTONS N64_CONT_C
#define R_CBUTTONS N64_CONT_F
#define D_CBUTTONS N64_CONT_D

#undef CONT_C
#undef CONT_B
#undef CONT_A
#undef CONT_START
#undef CONT_DPAD_UP
#undef CONT_DPAD_DOWN
#undef CONT_DPAD_LEFT
#undef CONT_DPAD_RIGHT
#undef CONT_Z
#undef CONT_Y
#undef CONT_X
#undef CONT_D
#undef CONT_DPAD2_UP
#undef CONT_DPAD2_DOWN
#undef CONT_DPAD2_LEFT
#undef CONT_DPAD2_RIGHT


#define CONT_C              (1<<0)      /**< \brief C button Mask. */
#define CONT_B              (1<<1)      /**< \brief B button Mask. */
#define CONT_A              (1<<2)      /**< \brief A button Mask. */
#define CONT_START          (1<<3)      /**< \brief Start button Mask. */
#define CONT_DPAD_UP        (1<<4)      /**< \brief Main Dpad Up button Mask. */
#define CONT_DPAD_DOWN      (1<<5)      /**< \brief Main Dpad Down button Mask. */
#define CONT_DPAD_LEFT      (1<<6)      /**< \brief Main Dpad Left button Mask. */
#define CONT_DPAD_RIGHT     (1<<7)      /**< \brief Main Dpad right button Mask. */
#define CONT_Z              (1<<8)      /**< \brief Z button Mask. */
#define CONT_Y              (1<<9)      /**< \brief Y button Mask. */
#define CONT_X              (1<<10)     /**< \brief X button Mask. */
#define CONT_D              (1<<11)     /**< \brief D button Mask. */
#define CONT_DPAD2_UP       (1<<12)     /**< \brief Secondary Dpad Up button Mask. */
#define CONT_DPAD2_DOWN     (1<<13)     /**< \brief Secondary Dpad Down button Mask. */
#define CONT_DPAD2_LEFT     (1<<14)     /**< \brief Secondary Dpad Left button Mask. */
#define CONT_DPAD2_RIGHT    (1<<15)     /**< \brief Secondary Dpad Right button Mask. */

OSContPad gControllerHold[4]={0};
OSContPad gControllerPress[4]={0};
u8 gControllerPlugged[4]={0};
u32 gControllerLock=0;
u8 gControllerRumbleEnabled[4]={0};
OSContPad sNextController[4]={0};
OSContPad sPrevController[4]={0};
OSContStatus sControllerStatus[4]={1};
OSPfs sControllerMotor[4]={0};

void I_RumbleThread(void *param);
#include <stdio.h>
#include <kos/thread.h>
#include <kos/worker_thread.h>


kthread_worker_t *rumble_worker_thread;
kthread_attr_t rumble_worker_attr;

void Controller_AddDeadZone(s32 contrNum) {
    s32 temp_v0 = gControllerHold[contrNum].stick_x;
    s32 temp_a2 = gControllerHold[contrNum].stick_y;
    s32 var_a0;
    s32 var_v0;

    if ((temp_v0 >= -16) && (temp_v0 <= 16)) {
        var_a0 = 0;
    } else if (temp_v0 > 16) {
        var_a0 = temp_v0 - 16;
    } else {
        var_a0 = temp_v0 + 16;
    }

    if ((temp_a2 >= -16) && (temp_a2 <= 16)) {
        var_v0 = 0;
    } else if (temp_a2 > 16) {
        var_v0 = temp_a2 - 16;
    } else {
        var_v0 = temp_a2 + 16;
    }

    if (var_a0 > 60) {
        var_a0 = 60;
    }
    if (var_a0 < -60) {
        var_a0 = -60;
    }
    if (var_v0 > 60) {
        var_v0 = 60;
    }
    if (var_v0 < -60) {
        var_v0 = -60;
    }
    gControllerPress[contrNum].stick_x = var_a0;
    gControllerPress[contrNum].stick_y = var_v0;
}
#include <kos.h>

void Controller_Init(void) {
    s32 i;
    printf("%s()\n", __func__);

    for (i = 0; i < 4; i++) {
        maple_device_t *cont;
        cont_state_t *state;
        cont = maple_enum_type(i, MAPLE_FUNC_CONTROLLER);

        gControllerPlugged[i] = !!cont;//(sp1F >> i) & 1;
        if (cont)
            sControllerStatus[i].status = 1;
        cont = maple_enum_type(i, MAPLE_FUNC_PURUPURU);

        gControllerRumbleEnabled[i] = !!cont;//(sp1F >> i) & 1;
    }

rumble_worker_attr.create_detached = 1;
	rumble_worker_attr.stack_size = 4096;
	rumble_worker_attr.stack_ptr = NULL;
	rumble_worker_attr.prio = PRIO_DEFAULT;
	rumble_worker_attr.label = "I_RumbleThread";
	rumble_worker_thread = thd_worker_create_ex(&rumble_worker_attr, I_RumbleThread, NULL);

//    for (i = 0; i < 4; i++) {
//        printf("controller enable? %d rumble enable? %d\n", gControllerPlugged[i], gControllerRumbleEnabled[i]);
//    }
}

#include <stdlib.h>
u16 ucheld;
extern char *fnpre;
int shader_debug_toggle = 0;
#include <dc/maple/keyboard.h>

void Map_Main(void);
extern file_t streamout;

void Controller_UpdateInput(void) {
#if 1
    s32 i;
    for (i = 0; i < 4; i++) {

    maple_device_t *cont;
    cont_state_t *state;
    ucheld = 0;
    cont = maple_enum_type(i, MAPLE_FUNC_CONTROLLER);
    gControllerPlugged[i] = !!cont;//(sp1F >> i) & 1;
    if (!cont)
        continue;
    if (cont) {
        sControllerStatus[i].status = 1;
        sNextController[i].errno = 0;
        maple_device_t *puru = maple_enum_type(i, MAPLE_FUNC_PURUPURU);
        gControllerRumbleEnabled[i] = !!puru;//(sp1F >> i) & 1;
    }

    state = maple_dev_status(cont);

    if (strcmp("/pc", fnpre) == 0) {
        if (/* (state->buttons & CONT_START) &&  */state->ltrig && state->rtrig) {
            if (state->buttons & CONT_START) {
            //profiler_stop();
            //profiler_clean_up();
            // give vmu a chance to write and close
            fs_close(streamout);
            exit(0);
            } //else {
//                if (state->buttons && CONT_A)
  //                  Map_Main();
   //         }
        }
    }
    const char stickH =state->joyx;
    const char stickV = 0xff-((uint8_t)(state->joyy));

    if (state->buttons & CONT_A)
        ucheld |= N64_CONT_A;//A_BUTTON;
    if (state->buttons & CONT_X)
        ucheld |= N64_CONT_B;//B_BUTTON;
    if (state->ltrig)
        ucheld |= N64_CONT_G;//Z_TRIG;
    if (state->rtrig)
        ucheld |= N64_CONT_R;//R_TRIG;
    if (state->buttons & CONT_START)
       ucheld |= N64_CONT_START;//START_BUTTON;

    if (state->buttons & CONT_Y)
        ucheld |= L_CBUTTONS;//C_UP
    if (state->buttons & CONT_B)
        ucheld |= D_CBUTTONS;//C_RIGHT
    if (state->buttons & CONT_DPAD_UP)
        ucheld |= U_CBUTTONS;//U_CBUTTONS;
    if (state->buttons & CONT_DPAD_RIGHT)
        ucheld |= R_CBUTTONS;//R_CBUTTONS;

    sNextController[i].stick_x = ((float)stickH/127)*80;
        sNextController[i].stick_y = ((float)stickV/127)*80;


    if (sNextController[i].stick_x < -50)
        ucheld |= N64_CONT_LEFT;
    if (sNextController[i].stick_x > 50)
        ucheld |= N64_CONT_RIGHT;
    if (sNextController[i].stick_y < -50)
        ucheld |= N64_CONT_DOWN;
    if (sNextController[i].stick_y > 50)
        ucheld |= N64_CONT_UP;


//printf("ucheld %04x\n",ucheld);
sNextController[i].button = ucheld;

        Controller_AddDeadZone(i);
            sPrevController[i] = gControllerHold[i];
            gControllerHold[i] = sNextController[i];
            gControllerPress[i].button =
                (gControllerHold[i].button ^ sPrevController[i].button) & gControllerHold[i].button;
gControllerHold[i].errno = 0;
            }    


#if 0
    for (i = 1; i < 4; i++) {
        if ((gControllerPlugged[i] == 1) && (sNextController[i].errno == 0)) {
            sPrevController[i] = gControllerHold[i];
            gControllerHold[i] = sNextController[i];
            gControllerPress[i].button =
                (gControllerHold[i].button ^ sPrevController[i].button) & gControllerHold[i].button;
            Controller_AddDeadZone(i);
        } else {
            gControllerHold[i].button = gControllerHold[i].stick_x = gControllerHold[i].stick_y =
                gControllerHold[i].errno = gControllerPress[i].button = gControllerPress[i].stick_x =
                    gControllerPress[i].stick_y = gControllerPress[i].errno = 0;
        }
    }
#endif
#endif

    maple_device_t *controller;
	// now move on to the keyboard and mouse additions
	controller = maple_enum_type(0, MAPLE_FUNC_KEYBOARD);
    if (controller) {
		kbd_state_t *kbd = maple_dev_status(controller);
for (int i = 0; i < KBD_MAX_PRESSED_KEYS; i++) {
			if (!kbd->cond.keys[i] || kbd->cond.keys[i] == KBD_KEY_ERROR)
				break;

			switch (kbd->cond.keys[i]) {
                case KBD_KEY_ESCAPE:
                if (shader_debug_toggle == 0) {
                    shader_debug_toggle = 1;
                }
                else if (shader_debug_toggle == 1) {
                    shader_debug_toggle = 2;
                }
                else if (shader_debug_toggle == 2) {
                    shader_debug_toggle = 0;
                }
printf("shader_debug_toggle %d\n",shader_debug_toggle);
                break;
                default:
                break;
            }

    }
}
}

void Controller_ReadData(void) {
#if 1
    s32 i;

    if (gControllerLock != 0) {
        gControllerLock--;
        for (i = 0; i < 4; i++) {
            sNextController[i].button = sNextController[i].stick_x = sNextController[i].stick_y =
                sNextController[i].errno = 0;
        }
    } else {
    //    osContStartReadData(&gSerialEventQueue);
  //      MQ_WAIT_FOR_MESG(&gSerialEventQueue, NULL);
//        osContGetReadData(sNextController);
        for (i = 0; i < 4; i++) {
            sNextController[i].button = sNextController[i].stick_x = sNextController[i].stick_y =
                sNextController[i].errno = 0;
        }

}
#endif
    osSendMesg(&gControllerMesgQueue, (OSMesg) SI_CONT_READ_DONE, OS_MESG_NOBLOCK);
}

void Save_ReadData(void) {
#if 0    
    if ((gStartNMI == 0) && (Save_ReadEeprom(&gSaveIOBuffer) == 0)) {
        osSendMesg(&gSaveMesgQueue, (OSMesg) SI_SAVE_SUCCESS, OS_MESG_NOBLOCK);
        return;
    }
#endif
    osSendMesg(&gSaveMesgQueue, (OSMesg) SI_SAVE_FAILED, OS_MESG_NOBLOCK);
}

void Save_WriteData(void) {
#if 0
    if ((gStartNMI == 0) && (Save_WriteEeprom(&gSaveIOBuffer) == 0)) {
        osSendMesg(&gSaveMesgQueue, (OSMesg) SI_SAVE_SUCCESS, OS_MESG_NOBLOCK);
        return;
    }
#endif
    osSendMesg(&gSaveMesgQueue, (OSMesg) SI_SAVE_FAILED, OS_MESG_NOBLOCK);
}
void I_RumbleThread(void *param)
{
	(void)param;
	kthread_job_t *next_job = thd_worker_dequeue_job(rumble_worker_thread);

	if (next_job) {
        uint32_t packet = (uint32_t)next_job->data;
        free(next_job);
		maple_device_t *purudev = NULL;
		purudev = maple_enum_type(0, MAPLE_FUNC_PURUPURU);
		if (purudev)
				purupuru_rumble_raw(purudev, packet);
	}
}

void Controller_Rumble(void) {
#if 1
    s32 i;

    for (i = 0; i < 4; i++) {
        if ((gControllerPlugged[i] != 0) && (sControllerStatus[i].errno == 0)) {
            if (sControllerStatus[i].status & 1) {
                if (gControllerRumbleEnabled[i] == 1) {
                    if (gControllerRumbleFlags[i] != 0) {
                        kthread_job_t *next_job = (kthread_job_t *)malloc(sizeof(kthread_job_t));
                        next_job->data = (void *)0x021A7009;
                        thd_worker_add_job(rumble_worker_thread, next_job);
                        thd_worker_wakeup(rumble_worker_thread);
                    } else {
                        kthread_job_t *next_job = (kthread_job_t *)malloc(sizeof(kthread_job_t));
                        next_job->data = (void *)0;
                        thd_worker_add_job(rumble_worker_thread, next_job);
                        thd_worker_wakeup(rumble_worker_thread);
                    }
                }
            } else {
                gControllerRumbleEnabled[i] = 0;
            }
        }
    }
    for (i = 0; i < 4; i++) {
        gControllerRumbleFlags[i] = 0;
    }
#endif
    }
