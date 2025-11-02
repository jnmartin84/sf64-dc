#ifndef GLOBAL_H
#define GLOBAL_H

#include "n64sys.h"

typedef enum OverlayCalls {
    /*  90 */ OVLCALL_FO_CS_COMPLETE = 90,
    /*  91 */ OVLCALL_BO_BASE_UPDATE,
    /*  92 */ OVLCALL_BO_BASE_DRAW,
    /*  93 */ OVLCALL_BO_BASE_SHIELD_UPDATE,
    /*  94 */ OVLCALL_BO_BASE_SHIELD_DRAW,
    /*  95 */ OVLCALL_BO_SHIELD_REACTOR_UPDATE,
    /*  96 */ OVLCALL_BO_SHIELD_REACTOR_DRAW,
    /*  97 */ OVLCALL_BO_LASER_CANNON_UPDATE,
    /*  98 */ OVLCALL_BO_LASER_CANNON_DRAW,
    /* 103 */ OVLCALL_TITLE_UPDATE = 103,
    /* 104 */ OVLCALL_TITLE_DRAW,
    /* 105 */ OVLCALL_MAP_UPDATE,
    /* 106 */ OVLCALL_MAP_DRAW,
    /* 107 */ OVLCALL_OPTION_UPDATE,
    /* 108 */ OVLCALL_OPTION_DRAW,
    /* 109 */ OVLCALL_GAME_OVER_UPDATE,
    /* 110 */ OVLCALL_UNKMAP_DRAW,
} OverlayCalls;


#include "sf64audio_external.h"
#include "functions.h"
#include "variables.h"
#include "context.h"
#include "sf64mesg.h"
#include "assets/ast_radio.h"
#include "sf64object.h"
#include "sf64level.h"
#include "sf64event.h"
#include "sf64player.h"
#include "i1.h"
#include "i2.h"
#include "i3.h"
#include "i4.h"
#include "i5.h"
#include "i6.h"
#include "assets/ast_common.h"

#include <stdint.h>
#include <math.h>
#include <string.h>
#include <stddef.h>
#include <stdarg.h>

#ifndef F_PI
#define F_PI        3.1415926f   /* pi             */
#endif

#define M_DTOR (F_PI / 180.0f)
//	0.01745329f
//(F_PI / 180.0f)
#define M_RTOD	(180.0f / F_PI)
//57.29577951f
//(180.0f / F_PI)

#endif // GLOBAL_H
