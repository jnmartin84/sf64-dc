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
//oneshot_timer_create(eeprom_flush, NULL, 2000);
void ostimer_fire(void* arg) {
    SFTimer *sftimer = (SFTimer *)arg;

 //   printf("timer fired :-D\n");

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

//u64 stupid_time_counter = 0;
//OSTime osGetTime(void) {
//    return stupid_time_counter++;
//}
#include <kos.h>

#define N64_NS_PER_TICK 21.91f

// Change me to "float" to trade accuracy for perf
typedef float N64Ticks;

volatile OSTime osGetTime(void) {
    uint64_t ns = timer_ns_gettime64();
    N64Ticks ticks = (N64Ticks)ns / (N64Ticks)N64_NS_PER_TICK;
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
static uint8_t icondata[512 * 2];

struct state_pak {
    OSPfsState state;
    file_t file;
    char filename[64];
};

struct state_pak openFile[16] = { 0 };

int fileIndex = 0;

static uint8_t eeprom_block[512] = {0};

#include <kos.h>

static file_t eeprom_file = -1;
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

    if (eeprom_file != -1) {
        fs_close(eeprom_file);
        eeprom_file = -1;
    }
}

s32 osEepromProbe(UNUSED OSMesgQueue* mq) {
#if 0
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
    vid_border_color(255, 0, 255);
    eeprom_file = fs_open(get_vmu_fn(vmudev, "sf64.rec"), O_RDONLY | O_META);
    if (-1 == eeprom_file) {
        eeprom_file = fs_open(get_vmu_fn(vmudev, "sf64.rec"), O_RDWR | O_CREAT | O_META);
        if (-1 == eeprom_file) {
            vid_border_color(0, 0, 0);
            return 1;
        }
        vid_border_color(255, 255, 0);
        vmu_pkg_t pkg;
        memset(eeprom_block, 0, 512);
        memset(&pkg, 0, sizeof(vmu_pkg_t));
        strcpy(pkg.desc_short, "Savegame");
        strcpy(pkg.desc_long, "Star Fox 64");
        strcpy(pkg.app_id, "Star Fox 64");
//        sprintf(texfn, "%s/kart.ico", fnpre);
        pkg.icon_cnt = 0;
        pkg.icon_data = NULL;//icondata;
        pkg.icon_anim_speed = 5;
        pkg.data_len = 512;
        pkg.data = &eeprom_block;
        //vmu_pkg_load_icon(&pkg, texfn);
        uint8_t* pkg_out;
        ssize_t pkg_size;
        vmu_pkg_build(&pkg, &pkg_out, &pkg_size);
        if (!pkg_out || pkg_size <= 0) {
            fs_close(eeprom_file);
            eeprom_file = -1;
            vid_border_color(0, 0, 0);
            return 0;
        }
        fs_write(eeprom_file, pkg_out, pkg_size);
        free(pkg_out);
        oneshot_timer_reset(timer);
    } else {
        fs_close(eeprom_file);
        eeprom_file = -1;
        eeprom_file = fs_open(get_vmu_fn(vmudev, "sf64.rec"), O_RDWR | O_META);
        if (-1 == eeprom_file) {
            vid_border_color(0, 0, 0);
            return EEPROM_TYPE_4K;
        }

        oneshot_timer_reset(timer);
    }

    vid_border_color(0, 0, 0);
    return EEPROM_TYPE_4K;
#endif
    return 0;
}
uint8_t* vmu_load_data(int channel, const char* name, uint8_t* outbuf, uint32_t* bytes_read);

static int reopen_vmu_eeprom(void) {
#if 0
    maple_device_t* vmudev = NULL;
    vmudev = maple_enum_type(0, MAPLE_FUNC_MEMCARD);
    if (!vmudev) {
        return 1;
    }

    eeprom_file = fs_open(get_vmu_fn(vmudev, "sf64.rec"), O_RDWR | O_META);
    return (eeprom_file == -1);
#endif
return 1;
}

s32 osEepromLongRead(UNUSED OSMesgQueue* mq, u8 address, u8* buffer,
                     int length) {
#if 0
    if (eeprom_file == -1) {
        if (reopen_vmu_eeprom()) {
            return 1;
        }
    }

    vid_border_color(0, 255, 0);

    if (-1 != eeprom_file) {
        mutex_lock_scoped(&eeprom_lock);
        ssize_t size = fs_total(eeprom_file);

        if (size != 1024) {
            fs_close(eeprom_file);
            vid_border_color(0, 0, 0);
            return 1;
        }

        // skip header
        fs_seek(eeprom_file, (512 * 1) + (address * 8), SEEK_SET);
        ssize_t rv = fs_read(eeprom_file, buffer, length);
        if (rv != length) {
            vid_border_color(0, 0, 0);
            return 1;
        }

        vid_border_color(0, 0, 0);
        oneshot_timer_reset(timer);
        return 0;
    } else {
        vid_border_color(0, 0, 0);
        return 1;
    }
    #endif
    return 1;
}

s32 osEepromRead(OSMesgQueue* mq, u8 address, u8* buffer) {
    return osEepromLongRead(mq, address, buffer, 8);
}

s32 osEepromLongWrite(UNUSED OSMesgQueue* mq, u8 address, u8* buffer,
                      int length) {
#if 0
    if (eeprom_file == -1) {
        if (reopen_vmu_eeprom()) {
            return 1;
        }
    }

    vid_border_color(0, 0, 255);

    if (-1 != eeprom_file) {
        mutex_lock_scoped(&eeprom_lock);
        ssize_t size = fs_total(eeprom_file);
        if (size != 1024) {
            vid_border_color(0, 0, 0);
            fs_close(eeprom_file);
            return 1;
        }
        // skip header
        fs_seek(eeprom_file, (512 * 1) + (address * 8), SEEK_SET);
        ssize_t rv = fs_write(eeprom_file, buffer, length);
        if (rv != length) {
            vid_border_color(0, 0, 0);
            return 1;
        }

        vid_border_color(0, 0, 0);
        oneshot_timer_reset(timer);
        return 0;
    } else {
        vid_border_color(0, 0, 0);
        return 1;
    }
#endif
    return 1;
}

s32 osEepromWrite(OSMesgQueue* mq, unsigned char address, unsigned char* buffer) {
    return osEepromLongWrite(mq, address, buffer, 8);
}

s32 osPfsDeleteFile(OSPfs* pfs, UNUSED u16 company_code, UNUSED u32 game_code,
                    UNUSED u8* game_name, UNUSED u8* ext_name) {
#if 0
    maple_device_t* vmudev = NULL;

    vmudev = maple_enum_type(pfs->channel, MAPLE_FUNC_MEMCARD);
    if (!vmudev)
        return PFS_ERR_NOPACK;
    vid_border_color(255, 0, 0);

    int rv = fs_unlink(get_vmu_fn(vmudev, "mk64.gho"));

    vid_border_color(0, 0, 0);

    if (rv)
        return PFS_ERR_ID_FATAL;

    return PFS_NO_ERROR;
#endif
    return -1;
}

// I would stack allocate this but I really would like to guarantee that
// the file/header creation can't fail from a lack of available memory
static uint8_t __attribute__((aligned(32))) tempblock[32768];

s32 osPfsReadWriteFile(OSPfs* pfs, s32 file_no, u8 flag, int offset, int size_in_bytes,
                       u8* data_buffer) {
#if 0
                        if (openFile[file_no].file == -1) {
        return PFS_ERR_INVALID;
    }

    if (flag == PFS_READ) {
        vid_border_color(0, 255, 0);
        fs_seek(openFile[file_no].file, (512 * 3) + offset, SEEK_SET);
        ssize_t res = fs_read(openFile[file_no].file, data_buffer, size_in_bytes);

        if (res != size_in_bytes) {
            vid_border_color(0, 0, 0);
            fs_close(openFile[file_no].file);
            openFile[file_no].file = -1;
            return PFS_ERR_BAD_DATA;
        }
        // dont need to close/reopen, didnt write
    } else {
        vid_border_color(0, 0, 255);
        fs_seek(openFile[file_no].file, (512 * 3) + offset, SEEK_SET);
        ssize_t rv = fs_write(openFile[file_no].file, data_buffer, size_in_bytes);
        if (rv != size_in_bytes) {
            vid_border_color(0, 0, 0);
            fs_close(openFile[file_no].file);
            openFile[file_no].file = -1;
            return PFS_CORRUPTED;
        }

        fs_close(openFile[file_no].file);
        openFile[file_no].file = fs_open(openFile[file_no].filename, O_RDWR | O_META);
        if (-1 == openFile[file_no].file) {
            vid_border_color(0, 0, 0);
            return PFS_CORRUPTED;
        }
    }
    vid_border_color(0, 0, 0);
    return PFS_NO_ERROR;
#endif
                        return -1;
}

s32 osPfsAllocateFile(OSPfs* pfs, u16 company_code, u32 game_code, u8* game_name, u8* ext_name,
                      UNUSED int file_size_in_bytes, s32* file_no) {
#if 0
                        maple_device_t* vmudev = NULL;

    vmudev = maple_enum_type(pfs->channel, MAPLE_FUNC_MEMCARD);
    if (!vmudev) {
        return PFS_NO_PAK_INSERTED;
    }
    char* filename = get_vmu_fn(vmudev, "mk64.gho");

    *file_no = fileIndex++;
    openFile[*file_no].file = fs_open(filename, O_RDWR | O_CREAT | O_META);
    if (-1 == openFile[*file_no].file) {
        return PFS_INVALID_DATA;
    }

    strcpy(openFile[*file_no].filename, filename);
    vid_border_color(255, 255, 0);
    vmu_pkg_t newpkg;
    memset(&newpkg, 0, sizeof(vmu_pkg_t));
    memset(tempblock, 0, 32768);
    strcpy(newpkg.desc_short, "Ghost Data");
    strcpy(newpkg.desc_long, "Mario Kart 64");
    strcpy(newpkg.app_id, "Mario Kart 64");
    newpkg.data = tempblock;
    newpkg.data_len = 32768;
    newpkg.icon_data = icondata;
    newpkg.icon_cnt = 2;
    newpkg.icon_anim_speed = 5;
    sprintf(texfn, "%s/ghost.ico", fnpre);
    vmu_pkg_load_icon(&newpkg, texfn);
    ssize_t pkg_size;
    u8* pkg_out;
    vmu_pkg_build(&newpkg, &pkg_out, &pkg_size);
    ssize_t rv = fs_write(openFile[*file_no].file, pkg_out, pkg_size);
    if (rv != pkg_size) {
        vid_border_color(0, 0, 0);
        fs_close(openFile[*file_no].file);
        free(pkg_out);
        return PFS_CORRUPTED;
    }
    fs_close(openFile[*file_no].file);
    free(pkg_out);
    openFile[*file_no].file = fs_open(openFile[*file_no].filename, O_RDWR | O_META);
    if (-1 == openFile[*file_no].file) {
        vid_border_color(0, 0, 0);
        return PFS_CORRUPTED;
    }
    openFile[*file_no].state.company_code = company_code;
    openFile[*file_no].state.game_code = game_code;
    strcpy(openFile[*file_no].state.game_name, (char*) game_name);
    strcpy(openFile[*file_no].state.ext_name, (char*) ext_name);
    vid_border_color(0, 0, 0);
    return PFS_NO_ERROR;
#endif
return -1;
}

extern int vmu_status(int channel);

s32 osPfsIsPlug(UNUSED OSMesgQueue* queue, u8* pattern) {
    *pattern = 0;
//    if (!vmu_status(0)) {
  //      *pattern = 1;
   // }
    return 1;
}

s32 osPfsInit(UNUSED OSMesgQueue* queue, OSPfs* pfs, int channel) {
#if 0
    int rv = vmu_status(channel);
    if (rv) {
        return PFS_NO_PAK_INSERTED;
    }
    pfs->queue = queue;
    if (channel != 0) {
        return PFS_NO_PAK_INSERTED;
    }
    pfs->channel = channel;
    pfs->status = 0;
    pfs->status |= PFS_INITIALIZED;
    return PFS_NO_ERROR;
#endif
return -1;
}

s32 osPfsNumFiles(UNUSED OSPfs* pfs, s32* max_files, s32* files_used) {
#if 0
    *max_files = 16;
    *files_used = fileIndex;
    return 0;
#endif
return -1;
    }

s32 osPfsFileState(UNUSED OSPfs* pfs, UNUSED s32 file_no, UNUSED OSPfsState* state) {
    return -1;//PFS_NO_ERROR;
}

extern int32_t Pak_Memory;

s32 osPfsFreeBlocks(OSPfs* pfs, s32* bytes_not_used) {
    //vmu_status(pfs->channel);

    //*bytes_not_used = Pak_Memory * 512;
    return -1;//PFS_NO_ERROR;
}

void __osPfsCloseAllFiles(void) {
//    for (size_t i = 0; i < 16; i++) {
  //      if (openFile[i].file != -1) {
    //        fs_close(openFile[i].file);
      //  }
    //}
}

s32 osPfsFindFile(OSPfs* pfs, u16 company_code, u32 game_code, u8* game_name, u8* ext_name,
                  s32* file_no) {
 #if 0
                    maple_device_t* vmudev = NULL;

    vmudev = maple_enum_type(pfs->channel, MAPLE_FUNC_MEMCARD);
    if (!vmudev) {
        return PFS_NO_PAK_INSERTED;
    }

    char* filename = get_vmu_fn(vmudev, "mk64.gho");

    vid_border_color(255, 0, 255);

    for (size_t i = 0; i < 16; i++) {
        if (openFile[i].state.game_code == game_code && 
            openFile[i].state.company_code == company_code &&
            strcmp(openFile[i].state.game_name, (char*) game_name) == 0 &&
            strcmp(openFile[i].state.ext_name, (char*) ext_name) == 0) {
            *file_no = i;
            vid_border_color(0, 0, 0);
            return PFS_NO_ERROR;
        }
    }

    *file_no = fileIndex++;

    openFile[*file_no].file = fs_open(filename, O_RDONLY | O_META);
    strcpy(openFile[*file_no].filename, filename);
    if (-1 == openFile[*file_no].file) {
        vid_border_color(0, 0, 0);
        return PFS_ERR_INVALID;
    } else {
        fs_close(openFile[*file_no].file);
        openFile[*file_no].file = -1;
        openFile[*file_no].file = fs_open(filename, O_RDWR | O_META);
        if (-1 == openFile[*file_no].file) {
            vid_border_color(0, 0, 0);
            return PFS_CORRUPTED;
        }
    }

    openFile[*file_no].state.company_code = company_code;
    openFile[*file_no].state.game_code = game_code;
    strcpy(openFile[*file_no].state.game_name, (char*) game_name);
    strcpy(openFile[*file_no].state.ext_name, (char*) ext_name);
    vid_border_color(0, 0, 0);
    return PFS_NO_ERROR;
#endif
return -1;
}