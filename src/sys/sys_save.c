#include "n64sys.h"

void n64_memcpy(void* dst, const void* src, size_t size);


SaveFile gSaveIOBuffer;
SaveFile sPrevSaveData;

s32 osFullEepromWrite(OSMesgQueue* mq, unsigned char* buffer);

s32 Save_WriteEeprom(SaveFile* arg0) {
    if (osEepromProbe(&gSerialEventQueue) != 1) {
        PRINTF("ＥＥＰＲＯＭ が ありません\n");
        return -1;
    }

    if (osFullEepromWrite(&gSerialEventQueue, arg0)) {
        return -1;
    }

    return 0;
}

s32 osFullEepromRead(OSMesgQueue* mq, u8* buffer);

s32 Save_ReadEeprom(SaveFile* arg0) {
    if (osEepromProbe(&gSerialEventQueue) != 1) {
        PRINTF("ＥＥＰＲＯＭ が ありません\n");
        return -1;
    }

    if (osFullEepromRead(&gSerialEventQueue, arg0)) {
        return -1;
    }

    n64_memcpy(&sPrevSaveData, arg0, sizeof(SaveFile));

    return 0;
}
