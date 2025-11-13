#include "gfx_window_manager_api.h"
#include "gfx_screen_config.h"
#include "macros.h"
#include <kos.h>
#include <stdio.h>
#include <string.h>
#include <dc/video.h>
#include <assert.h>

#define GFX_API_NAME "Dreamcast GLdc"
#define LOWRES 0
#if LOWRES
#define SCR_WIDTH (320)
// for fsaa
// (640*2)
#define SCR_HEIGHT (240)
#else
#define SCR_WIDTH (640)
// for fsaa
// (640*2)
#define SCR_HEIGHT (480)
#endif
static int force_vis = 1;
static unsigned int last_time = 0;

extern void glKosSwapBuffers(void);
extern uint64_t timer_ms_gettime64(void);

unsigned int GetSystemTimeLow(void) {
    uint64_t msec = timer_ms_gettime64();
    return (unsigned int) msec;
}

void DelayThread(unsigned int ms) {
    thd_sleep(ms);
}

static void gfx_dc_init(UNUSED const char *game_name, UNUSED uint8_t start_in_fullscreen) {
    last_time = GetSystemTimeLow();
}

static void gfx_dc_set_fullscreen_changed_callback(UNUSED void (*on_fullscreen_changed)(uint8_t is_now_fullscreen)) {
}

static void gfx_dc_set_fullscreen(UNUSED uint8_t enable) {
}

static void gfx_dc_set_keyboard_callbacks(UNUSED uint8_t (*on_key_down)(int scancode),
                                          UNUSED uint8_t (*on_key_up)(int scancode),
                                          UNUSED void (*on_all_keys_up)(void)) {
}

static void gfx_dc_main_loop(void (*run_one_game_iter)(void)) {
    while (1) {
        run_one_game_iter();
    }
}

static void gfx_dc_get_dimensions(uint32_t *width, uint32_t *height) {
    *width = SCR_WIDTH;
    *height = SCR_HEIGHT;
}

/* What events should we be handling? */
static void gfx_dc_handle_events(void) {
    /* Lets us yield to other threads*/
    //DelayThread(100);
}

uint8_t skip_debounce = 0;

extern uint8_t gVIsPerFrame;
typedef enum LevelId {
    /* -1 */ LEVEL_UNK_M1 = -1,
    /*  0 */ LEVEL_CORNERIA,
    /*  1 */ LEVEL_METEO,
    /*  2 */ LEVEL_SECTOR_X,
    /*  3 */ LEVEL_AREA_6,
    /*  4 */ LEVEL_UNK_4,
    /*  5 */ LEVEL_SECTOR_Y,
    /*  6 */ LEVEL_VENOM_1,
    /*  7 */ LEVEL_SOLAR,
    /*  8 */ LEVEL_ZONESS,
    /*  9 */ LEVEL_VENOM_ANDROSS,
    /* 10 */ LEVEL_TRAINING,
    /* 11 */ LEVEL_MACBETH,
    /* 12 */ LEVEL_TITANIA,
    /* 13 */ LEVEL_AQUAS,
    /* 14 */ LEVEL_FORTUNA,
    /* 15 */ LEVEL_UNK_15,
    /* 16 */ LEVEL_KATINA,
    /* 17 */ LEVEL_BOLSE,
    /* 18 */ LEVEL_SECTOR_Z,
    /* 19 */ LEVEL_VENOM_2,
    /* 20 */ LEVEL_VERSUS,
    /* 77 */ LEVEL_WARP_ZONE = 77,
} LevelId;
extern LevelId gCurrentLevel;

static uint8_t gfx_dc_start_frame(void) {
    const unsigned int cur_time = GetSystemTimeLow();
    const unsigned int elapsed = cur_time - last_time;
#if 1
    if (skip_debounce) {
        skip_debounce--;
        return 1;
    }
    const float OneFrameTime = 16.666667f;
    uint32_t ActualFrameTime = (uint32_t)(gVIsPerFrame * OneFrameTime);

    // skip if frame took longer than (gVIsPerFrame * 16.666667) ms
    if (elapsed > ActualFrameTime) {
        skip_debounce = 3; // skip a max of once every 4 (1+3) frames
        last_time = cur_time;
        return 0;
    }
#endif
    return 1;
}

static void gfx_dc_swap_buffers_begin(void) {
}

static void gfx_dc_swap_buffers_end(void) {
    /* Lets us yield to other threads*/
    glKosSwapBuffers();

    // Number of microseconds a frame should take (anywhere between 2 and 5 VIs per frame)
    const unsigned int cur_time = GetSystemTimeLow();
    const unsigned int elapsed = cur_time - last_time;
    last_time = cur_time;
    const float OneFrameTime = 16.666667f;
    uint32_t ActualFrameTime = (uint32_t)(gVIsPerFrame * OneFrameTime);


    if (force_vis && elapsed < ActualFrameTime) {
#ifdef DEBUG
        printf("elapsed %d ms fps %f delay %d \n", elapsed, 1000.0f / elapsed, ActualFrameTime - elapsed);
#endif
        DelayThread(ActualFrameTime - elapsed);
        last_time += (ActualFrameTime - elapsed);
    }
}

/* Idk what this is for? */
static double gfx_dc_get_time(void) {
    return 0.0;
}

struct GfxWindowManagerAPI gfx_dc = { gfx_dc_init,
                                      gfx_dc_set_keyboard_callbacks,
                                      gfx_dc_set_fullscreen_changed_callback,
                                      gfx_dc_set_fullscreen,
                                      gfx_dc_main_loop,
                                      gfx_dc_get_dimensions,
                                      gfx_dc_handle_events,
                                      gfx_dc_start_frame,
                                      gfx_dc_swap_buffers_begin,
                                      gfx_dc_swap_buffers_end,
                                      gfx_dc_get_time };

