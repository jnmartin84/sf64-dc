#include "sf64dma.h"

#define SEGBUF_ALIGNMENT 65536

// NO SEGMENT 0

// do not get rid of those `+2048`, you've been warned

// to use TLB static mappings, requires 38 64kb pages
// which is fewer than the 64 static mappings available
// this should be faster than regular page mapping

// 4 64k pages
// common
#define SEG1_BUF_SIZE 207520+2048
u8 __attribute__((aligned(SEGBUF_ALIGNMENT))) SEG1_BUF[SEG1_BUF_SIZE];

// 2 64kb pages
// max(bg_space,bg_planet)
// max(30576,72384)
#define SEG2_BUF_SIZE 72384+2048
u8 __attribute__((aligned(SEGBUF_ALIGNMENT))) SEG2_BUF[SEG2_BUF_SIZE];

// 3 64kb pages
// max(arwing,blue_marine,landmaster,versus)
// max(105836,29496,34144,192844)
#define SEG3_BUF_SIZE 192844+2048
u8 __attribute__((aligned(SEGBUF_ALIGNMENT))) SEG3_BUF[SEG3_BUF_SIZE];

// 1 64kb page
// max(ast_enmy_space,ast_enmy_planet)
// max(50362,41622)
#define SEG4_BUF_SIZE 50362+2048
u8 __attribute__((aligned(SEGBUF_ALIGNMENT))) SEG4_BUF[SEG4_BUF_SIZE];

// 1 64kb page
// text
#define SEG5_BUF_SIZE 47216+2048
u8 __attribute__((aligned(SEGBUF_ALIGNMENT))) SEG5_BUF[SEG5_BUF_SIZE];

// 7 64kb pages
// max(title,map,corneria,meteo,titania,sector_x,aquas,area_6,fortuna,
// sector_y,solar,zoness,venom_1,venom_2,ve1_boss,bolse,katina,macbeth,
// training)
#define SEG6_BUF_SIZE 396660+2048
u8 __attribute__((aligned(SEGBUF_ALIGNMENT))) SEG6_BUF[SEG6_BUF_SIZE];

// 2 64kb pages
// max(ending,vs_menu,warp_zone,7_ti_1,7_ti_2)
// 114092,74984,7632,60628,57876
#define SEG7_BUF_SIZE 114092+2048
u8 __attribute__((aligned(SEGBUF_ALIGNMENT))) SEG7_BUF[SEG7_BUF_SIZE];

// 5 64kb pages
// max(ending_award_front,ending_award_back,ending_expert,option,8_ti
#define SEG8_BUF_SIZE 303360+2048
u8 __attribute__((aligned(SEGBUF_ALIGNMENT))) SEG8_BUF[SEG8_BUF_SIZE];

// 3 64kb pages
// max(font_3d,9_ti,ve1_boss)
//49416 65764 149596
#define SEG9_BUF_SIZE 149596+2048
u8 __attribute__((aligned(SEGBUF_ALIGNMENT))) SEG9_BUF[SEG9_BUF_SIZE];

// 1 64kb page
// A_ti
#define SEGA_BUF_SIZE 39272+2048
u8 __attribute__((aligned(SEGBUF_ALIGNMENT))) SEGA_BUF[SEGA_BUF_SIZE];

// NO SEGMENT B

// 4 64kb pages
// andross
#define SEGC_BUF_SIZE 241992+2048
u8 __attribute__((aligned(SEGBUF_ALIGNMENT))) SEGC_BUF[SEGC_BUF_SIZE];

// 1 64kb page
// allies
#define SEGD_BUF_SIZE 51800+2048
u8 __attribute__((aligned(SEGBUF_ALIGNMENT))) SEGD_BUF[SEGD_BUF_SIZE];

// 2 64kb pages
// great_fox
#define SEGE_BUF_SIZE 73336+2048
u8 __attribute__((aligned(SEGBUF_ALIGNMENT))) SEGE_BUF[SEGE_BUF_SIZE];

// 4 + 2 + 3 + 1 + 1 + 7 + 2 + 5 + 3 + 1 + 4 + 1 + 2 + 2
// 2 64kb pages
// max(logo,star_wolf)
// max(9472,83856)
#define SEGF_BUF_SIZE 83856+2048
u8 __attribute__((aligned(SEGBUF_ALIGNMENT))) SEGF_BUF[SEGF_BUF_SIZE];

u8 *SEG_BUF[15] = {
    SEG1_BUF, SEG2_BUF, SEG3_BUF, SEG4_BUF,
    SEG5_BUF, SEG6_BUF, SEG7_BUF, SEG8_BUF,
    SEG9_BUF, SEGA_BUF, SEG6_BUF, SEGC_BUF,
    SEGD_BUF, SEGE_BUF, SEGF_BUF
};

u32 SEG_PAGECOUNT[15] = {
    4, 2, 3, 1,
    1, 7, 2, 5,
    3, 1, 1, 4, 
    1, 2, 2
};