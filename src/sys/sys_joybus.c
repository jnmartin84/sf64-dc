#include "n64sys.h"
#include "context.h"

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

#include <stdio.h>
#include <stdlib.h>
#include <kos/thread.h>
#include <dc/maple.h>
#include <dc/maple/controller.h>
#include <dc/maple/keyboard.h>
#include <dc/maple/purupuru.h>

static maple_device_t *ControllerDevice[4] = {0};

void Controller_Scan(void) {
    /* Clear existing controller status */
    for (int i = 0; i < 4; i++) {
        gControllerPlugged[i] = 0;
        sControllerStatus[i].status = 0;
        ControllerDevice[i] = NULL;
    }

    /* Loop through all available controllers
       and assign them to the proper ports */
    int i = 0;
    int port = 0;
    maple_device_t *cont;
    while((cont = maple_enum_type(i, MAPLE_FUNC_CONTROLLER))) {
        port = cont->port;

        gControllerPlugged[port] = 1;
        sControllerStatus[port].status = 1;
        ControllerDevice[port] = cont;

        i++;
    }
}

static maple_device_t *RumbleDevice[4] = {0};

void Rumble_Scan(void) {
    /* Clear existing rumble info */
    for (int i = 0; i < 4; i++) {
        RumbleDevice[i] = NULL;
        gControllerRumbleEnabled[i] = 0;
        gControllerRumbleFlags[i] = 0;
    }

    /* Loop through all available purupuru packs
       and assign them to the proper ports  */
    int i = 0;
    int port = 0;
    maple_device_t *purupuru;
    while((purupuru = maple_enum_type(i, MAPLE_FUNC_PURUPURU))) {
        port = purupuru->port;

        gControllerRumbleEnabled[port] = 1;
        RumbleDevice[port] = purupuru;

        i++;
    }
}

void Controller_Init(void) {
    printf("%s()\n", __func__);

    Controller_Scan();
    Rumble_Scan();

    /* Re-scan controllers and rumble packs
       if there's relevant maple activity */
    maple_attach_callback(MAPLE_FUNC_CONTROLLER, Controller_Scan);
    maple_detach_callback(MAPLE_FUNC_CONTROLLER, Controller_Scan);

    maple_attach_callback(MAPLE_FUNC_PURUPURU, Rumble_Scan);
    maple_detach_callback(MAPLE_FUNC_PURUPURU, Rumble_Scan);

#if 1
    rumble_worker_attr.create_detached = 1;
	rumble_worker_attr.stack_size = 4096;
	rumble_worker_attr.stack_ptr = NULL;
	rumble_worker_attr.prio = PRIO_DEFAULT;
	rumble_worker_attr.label = "I_RumbleThread";
	rumble_worker_thread = thd_worker_create_ex(&rumble_worker_attr, I_RumbleThread, NULL);
#endif
}

u16 ucheld;
extern char *fnpre;
int shader_debug_toggle = 0;
int cc_debug_toggle = 0;
#include <string.h>
//#include "../dcprofiler.h"
void Map_Main(void);

void Controller_UpdateInput(void) {
    for (int i = 0; i < 4; i++) {
        cont_state_t* state;
        ucheld = 0;

        if(!ControllerDevice[i]) {
            gControllerPlugged[i] = 0;
            continue;
        }

        if(!(state = maple_dev_status(ControllerDevice[i])))
            continue;

        gControllerPlugged[i] = 1;
        sControllerStatus[i].status = 1;
        sNextController[i].errno = 0;

        if (state->ltrig && state->rtrig) {
            if (state->buttons & CONT_START) {
                if (strcmp("/pc", fnpre) == 0) {
                    // profiler_stop();
                    // profiler_clean_up();
                    //  give vmu a chance to write and close
                    thd_sleep(3000);
                    exit(0);
                }
            }
        }

        const char stickH = state->joyx;
        const char stickV = 0xff - ((uint8_t) (state->joyy));

        if (state->buttons & CONT_A)
            ucheld |= N64_CONT_A; // A_BUTTON;
        if (state->buttons & CONT_X)
            ucheld |= N64_CONT_B; // B_BUTTON;
        if (state->ltrig)
            ucheld |= N64_CONT_G; // Z_TRIG;
        if (state->rtrig)
            ucheld |= N64_CONT_R; // R_TRIG;
        if (state->buttons & CONT_START)
            ucheld |= N64_CONT_START; // START_BUTTON;

        if (state->buttons & CONT_Y)
            ucheld |= L_CBUTTONS; // C_UP
        if (state->buttons & CONT_B)
            ucheld |= D_CBUTTONS; // C_RIGHT
        if (state->buttons & CONT_DPAD_UP)
            ucheld |= U_CBUTTONS; // U_CBUTTONS;
        if (state->buttons & CONT_DPAD_RIGHT)
            ucheld |= R_CBUTTONS; // R_CBUTTONS;

        sNextController[i].stick_x = ((float) stickH / 127) * 80;
        sNextController[i].stick_y = ((float) stickV / 127) * 80;

        if (sNextController[i].stick_x < -50)
            ucheld |= N64_CONT_LEFT;
        if (sNextController[i].stick_x > 50)
            ucheld |= N64_CONT_RIGHT;
        if (sNextController[i].stick_y < -50)
            ucheld |= N64_CONT_DOWN;
        if (sNextController[i].stick_y > 50)
            ucheld |= N64_CONT_UP;

        sNextController[i].button = ucheld;

        Controller_AddDeadZone(i);
        sPrevController[i] = gControllerHold[i];
        gControllerHold[i] = sNextController[i];
        gControllerPress[i].button =
            (gControllerHold[i].button ^ sPrevController[i].button) & gControllerHold[i].button;
        gControllerHold[i].errno = 0;
    }

#if 0
    maple_device_t *controller;
	// now move on to the keyboard and mouse additions
	controller = maple_enum_type(0, MAPLE_FUNC_KEYBOARD);
    if (controller) {
        kbd_state_t* kbd = kbd_get_state(controller);
        if(kbd->key_states[KBD_KEY_ESCAPE].value == KEY_STATE_CHANGED_DOWN) {
            shader_debug_toggle = !shader_debug_toggle;
        }
    }
#endif
}

void Controller_ReadData(void) {
    s32 i;

    if (gControllerLock != 0) {
        gControllerLock--;
        for (i = 0; i < 4; i++) {
            sNextController[i].button = sNextController[i].stick_x = sNextController[i].stick_y =
                sNextController[i].errno = 0;
        }
    } else {
        for (i = 0; i < 4; i++) {
            sNextController[i].button = sNextController[i].stick_x = sNextController[i].stick_y =
                sNextController[i].errno = 0;
        }
    }

    osSendMesg(&gControllerMesgQueue, (OSMesg) SI_CONT_READ_DONE, OS_MESG_NOBLOCK);
}

int last_read = SI_SAVE_SUCCESS;
int last_write = SI_SAVE_SUCCESS;

void Save_ReadData(void) {
#if 1
    if ((gStartNMI == 0) && (Save_ReadEeprom(&gSaveIOBuffer) == 0)) {
        //osSendMesg(&gSaveMesgQueue, (OSMesg) SI_SAVE_SUCCESS, OS_MESG_NOBLOCK);
        last_read = SI_SAVE_SUCCESS;
        return;
    }
#endif
    last_read = SI_SAVE_FAILED;
    //osSendMesg(&gSaveMesgQueue, (OSMesg) SI_SAVE_FAILED, OS_MESG_NOBLOCK);
}

void Save_WriteData(void) {
#if 1
    if ((gStartNMI == 0) && (Save_WriteEeprom(&gSaveIOBuffer) == 0)) {
        //osSendMesg(&gSaveMesgQueue, (OSMesg) SI_SAVE_SUCCESS, OS_MESG_NOBLOCK);
        last_write = SI_SAVE_SUCCESS;
        return;
    }
#endif
    last_write = SI_SAVE_FAILED;
    //osSendMesg(&gSaveMesgQueue, (OSMesg) SI_SAVE_FAILED, OS_MESG_NOBLOCK);
}

struct rumble_data_s {
    maple_device_t *dev;
    uint32_t packet;
};
struct rumble_data_s datas[4];


void I_RumbleThread(void *param)
{
	(void)param;
	kthread_job_t *next_job = thd_worker_dequeue_job(rumble_worker_thread);

	if (next_job) {
        uint32_t i = (uint32_t)next_job->data;
		purupuru_rumble_raw(datas[i].dev, datas[i].packet);
	}
}

kthread_job_t jobs[4];

void Controller_Rumble(void) {
    for (int i = 0; i < 4; i++) {
        if (!gControllerPlugged[i])
            continue;

        if (sControllerStatus[i].errno)
            continue;

        if (!(sControllerStatus[i].status & 1)) {
            gControllerRumbleEnabled[i] = 0;
            continue;
        }

        if(!RumbleDevice[i]) {
            gControllerRumbleEnabled[i] = 0;
            continue;
        }

        gControllerRumbleEnabled[i] = 1;

        if (!(gControllerRumbleFlags[i]))
            continue;

        purupuru_effect_t rumble = {
            .cont  = false,                        /* Do not run continuously */
            .motor = 1,                            /* Standard motor */
            .fpow  = 6,                            /* Forward motion, intensity 6/7 */
            .conv  = 1,                            /* Convergent curve to vibration */
            .freq  = 26,                           /* Frequency 26/59 */
            .inc   = gControllerRumbleTimers[i] ?  /* Duration */
                     gControllerRumbleTimers[i] : 1,
        };

        datas[i].dev = RumbleDevice[i];
        datas[i].packet = rumble.raw;
        jobs[i].data = i;

        thd_worker_add_job(rumble_worker_thread, &jobs[i]);
        thd_worker_wakeup(rumble_worker_thread);

//        purupuru_rumble_raw(RumbleDevice[i], rumble.raw);

        gControllerRumbleFlags[i] = 0;
    }
}
