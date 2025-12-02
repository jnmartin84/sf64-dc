#ifdef __GNUC__
#define UNUSED __attribute__((unused))
#else
#define UNUSED
#endif

#include "global.h"

#include <kos.h>



u64 osClockRate = 62500000;

OSTimer timerList = {0};

int ever_set = 0;

#include <kos/oneshot_timer.h>

typedef struct SFTimer_s {
    int active;

    OSTime			interval;	/* duration set by user */
	OSTime			value;		/* time remaining before */
						/* timer fires           */

    oneshot_timer_t *timer;

	OSMesgQueue		*mq;		/* Message Queue */
	OSMesg			msg;		/* Message to send */

} SFTimer;

OSTimer *mappool[64] = {0};
SFTimer timerpool[64] = {0};
size_t timerpool_pos = 0;
#include <stdio.h>

void ostimer_fire(void* arg) {
    SFTimer *sftimer = (SFTimer *)arg;

    if (sftimer->mq) {
        osSendMesg(sftimer->mq, sftimer->msg, OS_MESG_NOBLOCK);
    }

    if (sftimer->interval) {
        // reload this
        sftimer->timer = oneshot_timer_create(ostimer_fire, sftimer, sftimer->interval);
    } else {
        sftimer->active = 0;
    }
}

int osSetTimer(OSTimer *timer, OSTime countdown, OSTime interval, 
                        OSMesgQueue *mq, OSMesg msg) {

    SFTimer *nextTimer = &timerpool[(timerpool_pos++)&63];

    while (nextTimer->active) {
        // potential for infinite loop
        nextTimer = &timerpool[(timerpool_pos++)&63];
    }
    mappool[(timerpool_pos-1) & 63] = timer;
    nextTimer->active = 1;
    nextTimer->msg = msg;
    nextTimer->mq = mq;
    nextTimer->value = countdown;
    nextTimer->interval = interval;
//printf("made a timer\n");   
    nextTimer->timer = oneshot_timer_create(ostimer_fire, (void*)nextTimer, countdown);
oneshot_timer_reset(nextTimer->timer);
    return 0;
}

int osStopTimer(OSTimer *timer) {
    for (size_t i=0;i<64;i++) {
        if ((mappool[i] != NULL) && (mappool[i] == timer)) {
            timerpool[i].active = 0;
            // does it need to be cancelable?
            mappool[i] = NULL;
        }
    }
    return 0;
}

void osCreateMesgQueue(OSMesgQueue* mq, OSMesg* msgBuf, s32 count) {
    mq->validCount = 0;
    mq->first = 0;
    mq->msgCount = count;
    mq->msg = msgBuf;
    return;
}


s32 osSendMesg(OSMesgQueue* mq, OSMesg msg, UNUSED s32 flag) {
    s32 index;

    if (mq->validCount >= mq->msgCount) {
        return -1;
    }

    index = (mq->first + mq->validCount) % mq->msgCount;

    mq->msg[index] = msg;
    mq->validCount++;

    return 0;
}

s32 osRecvMesg(OSMesgQueue* mq, OSMesg* msg, UNUSED s32 flag) {
    if (mq->validCount == 0) {
        return -1;
    }

    if (msg != NULL) {
        *msg = *(mq->first + mq->msg);
    }

    mq->first = (mq->first + 1) % mq->msgCount;
    mq->validCount--;

    return 0;
}

void osSetTime(UNUSED OSTime time) {
    ;
}

#include <kos.h>

#define N64_NS_PER_TICK 21.91f
#define RECIP_N64_NS_PER_TICK 0.04564126f
// Change me to "float" to trade accuracy for perf
typedef float N64Ticks;

volatile OSTime osGetTime(void) {
    uint64_t ns = timer_ns_gettime64();
    N64Ticks ticks = (N64Ticks)ns * (N64Ticks)RECIP_N64_NS_PER_TICK;
    return (OSTime)ticks;
}

void osWritebackDCacheAll(void) {
    ;
}

void osWritebackDCache(UNUSED void* a, UNUSED s32 b) {
    ;
}

void osInvalDCache(UNUSED void* a, UNUSED s32 b) {
    ;
}

void osInvalICache(UNUSED void* a, UNUSED s32 b) {
    ;
}

// 
static u32 counter = 0;
static u32 ticked = 0;
u32 osGetCount(void) {
    ticked++;
    counter += 757576;
    counter += ticked & 1;
    return counter;
}

s32 osAiSetFrequency(u32 freq) {
    u32 a1;
    s32 a2;
    u32 D_8033491C;

    D_8033491C = 0x02E6D354;
    a1 = D_8033491C / (float) freq + .5f;

    if (a1 < 0x84) {
        return -1;
    }

    a2 = (a1 / 66) & 0xff;
    if (a2 > 16) {
        a2 = 16;
    }

    return D_8033491C / (s32) a1;
}

extern char* fnpre;
static char texfn[256];
static uint8_t icondata[512];

static uint8_t eeprom_block[512] = {0};

#include <kos.h>

static file_t eeprom_file = FILEHND_INVALID;
static mutex_t eeprom_lock;
static int eeprom_init = 0;
#include "sf64save.h"


// thanks @zcrc
#include <kos/oneshot_timer.h>
/* 2s timer, to delay closing the VMU file.
 * This is because the emulator might open/modify/close often, and we want the
 * VMU VFS driver to only write to the VMU once we're done modifying the file. */
static oneshot_timer_t* timer;

static char full_fn[20];

char *get_vmu_fn(maple_device_t *vmudev, char *fn) {
	if (fn)
		sprintf(full_fn, "/vmu/%c%d/%s", 'a'+vmudev->port, vmudev->unit, fn);
	else
		sprintf(full_fn, "/vmu/%c%d", 'a'+vmudev->port, vmudev->unit);

	return full_fn;
}

void eeprom_flush(UNUSED void* arg) {
    mutex_lock_scoped(&eeprom_lock);
    
    if (eeprom_file != FILEHND_INVALID) {
        fs_close(eeprom_file);
        eeprom_file = FILEHND_INVALID;
    }
    
}

s32 osEepromProbe(UNUSED OSMesgQueue* mq) {
    maple_device_t* vmudev = NULL;
    
    if (!eeprom_init) {
        mutex_init(&eeprom_lock, MUTEX_TYPE_NORMAL);
        eeprom_init = 1;
        timer = oneshot_timer_create(eeprom_flush, NULL, 2000);
    }

    vmudev = maple_enum_type(0, MAPLE_FUNC_MEMCARD);
    if (!vmudev) {
        
        return 0;
    }
    //vid_border_color(255, 0, 255);
    eeprom_file = fs_open(get_vmu_fn(vmudev, "sf64.rec"), O_RDONLY | O_META);
    if (FILEHND_INVALID == eeprom_file) {
        eeprom_file = fs_open(get_vmu_fn(vmudev, "sf64.rec"), O_RDWR | O_CREAT | O_META);

        if (FILEHND_INVALID == eeprom_file) {
            //vid_border_color(0, 0, 0);
            
            return 1;
        }

        //vid_border_color(255, 255, 0);
        vmu_pkg_t pkg;
        memset(eeprom_block, 0, 512);
        memset(&pkg, 0, sizeof(vmu_pkg_t));
        strcpy(pkg.desc_short, "Savegame");
        strcpy(pkg.desc_long, "Star Fox 64");
        strcpy(pkg.app_id, "Star Fox 64");
        sprintf(texfn, "%s/sf64.ico", fnpre);
        pkg.icon_cnt = 1;
        pkg.icon_data = icondata;
        pkg.icon_anim_speed = 5;
        pkg.data_len = 512;
        pkg.data = &eeprom_block;
        vmu_pkg_load_icon(&pkg, texfn);
        uint8_t* pkg_out;
        ssize_t pkg_size;
        vmu_pkg_build(&pkg, &pkg_out, &pkg_size);

        if (!pkg_out || pkg_size <= 0) {
            fs_close(eeprom_file);
            eeprom_file = FILEHND_INVALID;
            //vid_border_color(0, 0, 0);
            
            return 0;
        }

        fs_write(eeprom_file, pkg_out, pkg_size);
        free(pkg_out);
        oneshot_timer_reset(timer);
    } else {
        fs_close(eeprom_file);
        eeprom_file = FILEHND_INVALID;
        eeprom_file = fs_open(get_vmu_fn(vmudev, "sf64.rec"), O_RDWR | O_META);

        if (FILEHND_INVALID == eeprom_file) {
            //vid_border_color(0, 0, 0);
            
            return EEPROM_TYPE_4K;
        }

        oneshot_timer_reset(timer);
    }

    //vid_border_color(0, 0, 0);
    
    return EEPROM_TYPE_4K;
}
uint8_t* vmu_load_data(int channel, const char* name, uint8_t* outbuf, uint32_t* bytes_read);

static int reopen_vmu_eeprom(void) {
    maple_device_t* vmudev = NULL;
    
    vmudev = maple_enum_type(0, MAPLE_FUNC_MEMCARD);
    if (!vmudev) {
        return 1;
    }

    eeprom_file = fs_open(get_vmu_fn(vmudev, "sf64.rec"), O_RDWR | O_META);
    
    return (eeprom_file == FILEHND_INVALID);
}

s32 osEepromLongRead(UNUSED OSMesgQueue* mq, u8 address, u8* buffer,
                     int length) {
    if (eeprom_file == FILEHND_INVALID) {
        if (reopen_vmu_eeprom()) {
            return 1;
        }
    }

    //vid_border_color(0, 255, 0);

    if (FILEHND_INVALID != eeprom_file) {
        mutex_lock_scoped(&eeprom_lock);
        ssize_t size = fs_total(eeprom_file);

        if (size != (512 * 3)) {
            fs_close(eeprom_file);
            eeprom_file = FILEHND_INVALID;
            //vid_border_color(0, 0, 0);
            
            return 1;
        }

        // skip header
        fs_seek(eeprom_file, (512 * 2) + (address * 8), SEEK_SET);
        ssize_t rv = fs_read(eeprom_file, buffer, length);
        if (rv != length) {
            //vid_border_color(0, 0, 0);
            
            return 1;
        }

        //vid_border_color(0, 0, 0);
        oneshot_timer_reset(timer);
        
        return 0;
    } else {
        //vid_border_color(0, 0, 0);
        
        return 1;
    }
}

s32 osEepromRead(OSMesgQueue* mq, u8 address, u8* buffer) {
    return osEepromLongRead(mq, address, buffer, 8);
}

s32 osEepromLongWrite(UNUSED OSMesgQueue* mq, u8 address, u8* buffer,
                      int length) {
    if (eeprom_file == FILEHND_INVALID) {
        if (reopen_vmu_eeprom()) {
            return 1;
        }
    }
    
    //vid_border_color(0, 0, 255);

    if (FILEHND_INVALID != eeprom_file) {
        mutex_lock_scoped(&eeprom_lock);
        ssize_t size = fs_total(eeprom_file);
        if (size != (512 * 3)) {
            //vid_border_color(0, 0, 0);
            fs_close(eeprom_file);
            eeprom_file = FILEHND_INVALID;
            
            return 1;
        }
        // skip header
        fs_seek(eeprom_file, (512 * 2) + (address * 8), SEEK_SET);
        ssize_t rv = fs_write(eeprom_file, buffer, length);
        if (rv != length) {
            //vid_border_color(0, 0, 0);
            
            return 1;
        }

        //vid_border_color(0, 0, 0);
        oneshot_timer_reset(timer);
        
        return 0;
    } else {
        //vid_border_color(0, 0, 0);
        
        return 1;
    }
}

s32 osEepromWrite(OSMesgQueue* mq, unsigned char address, unsigned char* buffer) {
    return osEepromLongWrite(mq, address, buffer, 8);
}
