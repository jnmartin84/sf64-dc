#include "n64sys.h"

#include <stdint.h>
#include <math.h>
#include <string.h>
#include <stddef.h>
#include <stdarg.h>
s32 sMemoryBuffer[0x2200] = {0};
s32* sMemoryPtr;

void Memory_FreeAll(void) {
    sMemoryPtr = sMemoryBuffer;
}

void* Memory_Allocate(s32 size) {
    void* addr = sMemoryPtr;

    sMemoryPtr = (void*) (((size + 0xF) & ~0xF) + (uintptr_t) sMemoryPtr);
    return addr;
}
