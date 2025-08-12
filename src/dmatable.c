#include "sf64dma.h"

// NO SEGMENT 0

// common
#define SEG1_BUF_SIZE 207520
u8 __attribute__((aligned(32))) SEG1_BUF[SEG1_BUF_SIZE];

// max(bg_space,bg_planet)
// max(30576,72384)
#define SEG2_BUF_SIZE 72384
u8 __attribute__((aligned(32))) SEG2_BUF[SEG2_BUF_SIZE];

// max(arwing,blue_marine,landmaster)
// max(105836,29496,34144)
#define SEG3_BUF_SIZE 105836
u8 __attribute__((aligned(32))) SEG3_BUF[SEG3_BUF_SIZE];

// max(ast_enmy_space,ast_enmy_planet)
// max(50362,41622)
#define SEG4_BUF_SIZE 50362
u8 __attribute__((aligned(32))) SEG4_BUF[SEG4_BUF_SIZE];

// text
#define SEG5_BUF_SIZE 47216
u8 __attribute__((aligned(32))) SEG5_BUF[SEG5_BUF_SIZE];

// max(title,map,corneria,meteo,titania,sector_x,aquas,area_6,fortuna,
// sector_y,solar,zoness,venom_1,venom_2,ve1_boss,bolse,katina,macbeth,
// training)
#define SEG6_BUF_SIZE 396660
u8 __attribute__((aligned(32))) SEG6_BUF[SEG6_BUF_SIZE];

// max(ending,vs_menu,warp_zone,7_ti_1,7_ti_2)
// 114092,74984,7632,60628,57876
#define SEG7_BUF_SIZE 114092
u8 __attribute__((aligned(32))) SEG7_BUF[SEG7_BUF_SIZE];

// max(ending_award_front,ending_award_back,ending_expert,option,8_ti
#define SEG8_BUF_SIZE 303360
u8 __attribute__((aligned(32))) SEG8_BUF[SEG8_BUF_SIZE];

// max(font_3d,9_ti,ve1_boss)
//49416 65764 149596
#define SEG9_BUF_SIZE 149596
u8 __attribute__((aligned(32))) SEG9_BUF[SEG9_BUF_SIZE];

// A_ti
#define SEGA_BUF_SIZE 39272
u8 __attribute__((aligned(32))) SEGA_BUF[SEGA_BUF_SIZE];

// NO SEGMENT B

// andross
#define SEGC_BUF_SIZE 241992
u8 __attribute__((aligned(32))) SEGC_BUF[SEGC_BUF_SIZE];

// allies
#define SEGD_BUF_SIZE 51800
u8 __attribute__((aligned(32))) SEGD_BUF[SEGD_BUF_SIZE];

// great_fox
#define SEGE_BUF_SIZE 73336
u8 __attribute__((aligned(32))) SEGE_BUF[SEGE_BUF_SIZE];

// max(logo,star_wolf)
// max(9472,83856)
#define SEGF_BUF_SIZE 83856
u8 __attribute__((aligned(32))) SEGF_BUF[SEGF_BUF_SIZE];


//u8 SEG_BUF[15][0x60D80];

u8 *SEG_BUF[15] = {
    SEG1_BUF, SEG2_BUF, SEG3_BUF, SEG4_BUF,
    SEG5_BUF, SEG6_BUF, SEG7_BUF, SEG8_BUF,
    SEG9_BUF, SEGA_BUF, SEG6_BUF, SEGC_BUF,
    SEGD_BUF, SEGE_BUF, SEGF_BUF
};


#if 0
DmaEntry gDmaTable[81] = {
    DMA_ENTRY(makerom),
    DMA_ENTRY(main),
    DMA_ENTRY(dma_table),
    DMA_ENTRY(audio_seq),
    DMA_ENTRY(audio_bank),
    DMA_ENTRY(audio_table),
    DMA_ENTRY(ast_common),
    DMA_ENTRY(ast_bg_space),
    DMA_ENTRY(ast_bg_planet),
    DMA_ENTRY(ast_arwing),
    DMA_ENTRY(ast_landmaster),
    DMA_ENTRY(ast_blue_marine),
    DMA_ENTRY(ast_versus),
    DMA_ENTRY(ast_enmy_planet),
    DMA_ENTRY(ast_enmy_space),
    DMA_ENTRY(ast_great_fox),
    DMA_ENTRY(ast_star_wolf),
    DMA_ENTRY(ast_allies),
    DMA_ENTRY(ast_corneria),
    DMA_ENTRY(ast_meteo),
    DMA_ENTRY(ast_titania),
    DMA_ENTRY(ast_7_ti_2),
    DMA_ENTRY(ast_8_ti),
    DMA_ENTRY(ast_9_ti),
    DMA_ENTRY(ast_A_ti),
    DMA_ENTRY(ast_7_ti_1),
    DMA_ENTRY(ast_sector_x),
    DMA_ENTRY(ast_sector_z),
    DMA_ENTRY(ast_aquas),
    DMA_ENTRY(ast_area_6),
    DMA_ENTRY(ast_venom_1),
    DMA_ENTRY(ast_venom_2),
    DMA_ENTRY(ast_ve1_boss),
    DMA_ENTRY(ast_bolse),
    DMA_ENTRY(ast_fortuna),
    DMA_ENTRY(ast_sector_y),
    DMA_ENTRY(ast_solar),
    DMA_ENTRY(ast_zoness),
    DMA_ENTRY(ast_katina),
    DMA_ENTRY(ast_macbeth),
    DMA_ENTRY(ast_warp_zone),
    DMA_ENTRY(ast_title),
    DMA_ENTRY(ast_map),
    DMA_ENTRY(ast_option),
    DMA_ENTRY(ast_vs_menu),
    DMA_ENTRY(ast_text),
    DMA_ENTRY(ast_font_3d),
    DMA_ENTRY(ast_andross),
    DMA_ENTRY(ast_logo),
    DMA_ENTRY(ast_ending),
    DMA_ENTRY(ast_ending_award_front),
    DMA_ENTRY(ast_ending_award_back),
    DMA_ENTRY(ast_ending_expert),
    DMA_ENTRY(ast_training),
    DMA_ENTRY(ast_radio),
#if 0
    DMA_ENTRY(ovl_i1),
    DMA_ENTRY(ovl_i2),
    DMA_ENTRY(ovl_i3),
    DMA_ENTRY(ovl_i4),
    DMA_ENTRY(ovl_i5),
    DMA_ENTRY(ovl_i6),
    DMA_ENTRY(ovl_menu),
    DMA_ENTRY(ovl_ending),
    DMA_ENTRY(ovl_unused),
#endif
};
#endif