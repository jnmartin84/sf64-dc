#include "sf64dma.h"

#include "../segments.h"
#if 0
Scene sNoOvl_Logo[1] = {
    { NO_OVERLAY,
      { /* 0x1 */ NO_SEGMENT,
        /* 0x2 */ NO_SEGMENT,
        /* 0x3 */ NO_SEGMENT,
        /* 0x4 */ NO_SEGMENT,
        /* 0x5 */ NO_SEGMENT,
        /* 0x6 */ NO_SEGMENT,
        /* 0x7 */ NO_SEGMENT,
        /* 0x8 */ NO_SEGMENT,
        /* 0x9 */ NO_SEGMENT,
        /* 0xA */ NO_SEGMENT,
        /* 0xB */ NO_SEGMENT,
        /* 0xC */ NO_SEGMENT,
        /* 0xD */ NO_SEGMENT,
        /* 0xE */ NO_SEGMENT,
        /* 0xF */ ROM_SEGMENT(ast_logo) } },
};
Scene sOvlending_Ending[6] = {
    { OVERLAY_OFFSETS(ovl_ending),
      { /* 0x1 */ NO_SEGMENT,
        /* 0x2 */ NO_SEGMENT,
        /* 0x3 */ ROM_SEGMENT(ast_arwing),
        /* 0x4 */ NO_SEGMENT,
        /* 0x5 */ ROM_SEGMENT(ast_text),
        /* 0x6 */ NO_SEGMENT,
        /* 0x7 */ ROM_SEGMENT(ast_ending),
        /* 0x8 */ NO_SEGMENT,
        /* 0x9 */ NO_SEGMENT,
        /* 0xA */ NO_SEGMENT,
        /* 0xB */ NO_SEGMENT,
        /* 0xC */ NO_SEGMENT,
        /* 0xD */ ROM_SEGMENT(ast_allies),
        /* 0xE */ ROM_SEGMENT(ast_great_fox),
        /* 0xF */ NO_SEGMENT } },

    { OVERLAY_OFFSETS(ovl_ending),
      { /* 0x1 */ NO_SEGMENT,
        /* 0x2 */ NO_SEGMENT,
        /* 0x3 */ NO_SEGMENT,
        /* 0x4 */ NO_SEGMENT,
        /* 0x5 */ ROM_SEGMENT(ast_text),
        /* 0x6 */ ROM_SEGMENT(ast_title),
        /* 0x7 */ ROM_SEGMENT(ast_ending),
        /* 0x8 */ ROM_SEGMENT(ast_ending_award_front),
        /* 0x9 */ NO_SEGMENT,
        /* 0xA */ NO_SEGMENT,
        /* 0xB */ NO_SEGMENT,
        /* 0xC */ NO_SEGMENT,
        /* 0xD */ NO_SEGMENT,
        /* 0xE */ NO_SEGMENT,
        /* 0xF */ NO_SEGMENT } },

    { OVERLAY_OFFSETS(ovl_ending),
      { /* 0x1 */ NO_SEGMENT,
        /* 0x2 */ NO_SEGMENT,
        /* 0x3 */ NO_SEGMENT,
        /* 0x4 */ NO_SEGMENT,
        /* 0x5 */ ROM_SEGMENT(ast_text),
        /* 0x6 */ ROM_SEGMENT(ast_title),
        /* 0x7 */ ROM_SEGMENT(ast_ending),
        /* 0x8 */ ROM_SEGMENT(ast_ending_award_back),
        /* 0x9 */ NO_SEGMENT,
        /* 0xA */ NO_SEGMENT,
        /* 0xB */ NO_SEGMENT,
        /* 0xC */ NO_SEGMENT,
        /* 0xD */ NO_SEGMENT,
        /* 0xE */ NO_SEGMENT,
        /* 0xF */ NO_SEGMENT } },
    { OVERLAY_OFFSETS(ovl_ending),
      { /* 0x1 */ NO_SEGMENT,
        /* 0x2 */ NO_SEGMENT,
        /* 0x3 */ NO_SEGMENT,
        /* 0x4 */ NO_SEGMENT,
        /* 0x5 */ ROM_SEGMENT(ast_text),
        /* 0x6 */ ROM_SEGMENT(ast_title),
        /* 0x7 */ ROM_SEGMENT(ast_ending),
        /* 0x8 */ ROM_SEGMENT(ast_ending_expert),
        /* 0x9 */ NO_SEGMENT,
        /* 0xA */ NO_SEGMENT,
        /* 0xB */ NO_SEGMENT,
        /* 0xC */ NO_SEGMENT,
        /* 0xD */ NO_SEGMENT,
        /* 0xE */ NO_SEGMENT,
        /* 0xF */ NO_SEGMENT } },
   { OVERLAY_OFFSETS(ovl_ending),
      { /* 0x1 */ NO_SEGMENT,
        /* 0x2 */ NO_SEGMENT,
        /* 0x3 */ NO_SEGMENT,
        /* 0x4 */ NO_SEGMENT,
        /* 0x5 */ ROM_SEGMENT(ast_text),
        /* 0x6 */ ROM_SEGMENT(ast_title),
        /* 0x7 */ ROM_SEGMENT(ast_ending),
        /* 0x8 */ NO_SEGMENT,
        /* 0x9 */ NO_SEGMENT,
        /* 0xA */ NO_SEGMENT,
        /* 0xB */ NO_SEGMENT,
        /* 0xC */ NO_SEGMENT,
        /* 0xD */ NO_SEGMENT,
        /* 0xE */ ROM_SEGMENT(ast_great_fox),
        /* 0xF */ NO_SEGMENT } },

    { OVERLAY_OFFSETS(ovl_ending),
      { /* 0x1 */ NO_SEGMENT,
        /* 0x2 */ NO_SEGMENT,
        /* 0x3 */ ROM_SEGMENT(ast_arwing),
        /* 0x4 */ NO_SEGMENT,
        /* 0x5 */ ROM_SEGMENT(ast_text),
        /* 0x6 */ NO_SEGMENT,
        /* 0x7 */ ROM_SEGMENT(ast_ending),
        /* 0x8 */ ROM_SEGMENT(ast_ending_expert),
        /* 0x9 */ ROM_SEGMENT(ast_font_3d),
        /* 0xA */ NO_SEGMENT,
        /* 0xB */ NO_SEGMENT,
        /* 0xC */ NO_SEGMENT,
        /* 0xD */ NO_SEGMENT,
        /* 0xE */ ROM_SEGMENT(ast_great_fox),
        /* 0xF */ NO_SEGMENT } },
};



Scene sOvlmenu_Title[1] = {
    { OVERLAY_OFFSETS(ovl_menu),
      { /* 0x1 */ NO_SEGMENT,
        /* 0x2 */ NO_SEGMENT,
        /* 0x3 */ ROM_SEGMENT(ast_arwing),
        /* 0x4 */ NO_SEGMENT,
        /* 0x5 */ ROM_SEGMENT(ast_text),
        /* 0x6 */ ROM_SEGMENT(ast_title),
        /* 0x7 */ NO_SEGMENT,
        /* 0x8 */ NO_SEGMENT,
        /* 0x9 */ NO_SEGMENT,
        /* 0xA */ NO_SEGMENT,
        /* 0xB */ NO_SEGMENT,
        /* 0xC */ NO_SEGMENT,
        /* 0xD */ NO_SEGMENT,
        /* 0xE */ ROM_SEGMENT(ast_great_fox),
        /* 0xF */ NO_SEGMENT } },
};


Scene sOvlmenu_Option[1] = {
    { OVERLAY_OFFSETS(ovl_menu),
      { /* 0x1 */ NO_SEGMENT,
        /* 0x2 */ NO_SEGMENT,
        /* 0x3 */ NO_SEGMENT,
        /* 0x4 */ NO_SEGMENT,
        /* 0x5 */ ROM_SEGMENT(ast_text),
        /* 0x6 */ ROM_SEGMENT(ast_map),
        /* 0x7 */ ROM_SEGMENT(ast_vs_menu),
        /* 0x8 */ ROM_SEGMENT(ast_option),
        /* 0x9 */ ROM_SEGMENT(ast_font_3d),
        /* 0xA */ NO_SEGMENT,
        /* 0xB */ NO_SEGMENT,
        /* 0xC */ NO_SEGMENT,
        /* 0xD */ NO_SEGMENT,
        /* 0xE */ NO_SEGMENT,
        /* 0xF */ NO_SEGMENT } },
};



Scene sOvlmenu_Map[1] = {
    { OVERLAY_OFFSETS(ovl_menu),
      { /* 0x1 */ NO_SEGMENT,
        /* 0x2 */ NO_SEGMENT,
        /* 0x3 */ ROM_SEGMENT(ast_arwing),
        /* 0x4 */ NO_SEGMENT,
        /* 0x5 */ ROM_SEGMENT(ast_text),
        /* 0x6 */ ROM_SEGMENT(ast_map),
        /* 0x7 */ NO_SEGMENT,
        /* 0x8 */ NO_SEGMENT,
        /* 0x9 */ ROM_SEGMENT(ast_font_3d),
        /* 0xA */ NO_SEGMENT,
        /* 0xB */ NO_SEGMENT,
        /* 0xC */ NO_SEGMENT,
        /* 0xD */ NO_SEGMENT,
        /* 0xE */ NO_SEGMENT,
        /* 0xF */ NO_SEGMENT } },
};

Scene sOvlmenu_GameOver[1] = {
    { OVERLAY_OFFSETS(ovl_menu),
      { /* 0x1 */ NO_SEGMENT,
        /* 0x2 */ NO_SEGMENT,
        /* 0x3 */ NO_SEGMENT,
        /* 0x4 */ NO_SEGMENT,
        /* 0x5 */ ROM_SEGMENT(ast_text),
        /* 0x6 */ NO_SEGMENT,
        /* 0x7 */ NO_SEGMENT,
        /* 0x8 */ NO_SEGMENT,
        /* 0x9 */ NO_SEGMENT,
        /* 0xA */ NO_SEGMENT,
        /* 0xB */ NO_SEGMENT,
        /* 0xC */ ROM_SEGMENT(ast_andross),
        /* 0xD */ NO_SEGMENT,
        /* 0xE */ NO_SEGMENT,
        /* 0xF */ NO_SEGMENT } },
};


Scene sOvli1_Corneria[1] = {
    { OVERLAY_OFFSETS(ovl_i1),
      { /* 0x1 */ ROM_SEGMENT(ast_common),
        /* 0x2 */ ROM_SEGMENT(ast_bg_planet),
        /* 0x3 */ ROM_SEGMENT(ast_arwing),
        /* 0x4 */ ROM_SEGMENT(ast_enmy_planet),
        /* 0x5 */ ROM_SEGMENT(ast_text),
        /* 0x6 */ ROM_SEGMENT(ast_corneria),
        /* 0x7 */ NO_SEGMENT,
        /* 0x8 */ NO_SEGMENT,
        /* 0x9 */ NO_SEGMENT,
        /* 0xA */ NO_SEGMENT,
        /* 0xB */ NO_SEGMENT,
        /* 0xC */ NO_SEGMENT,
        /* 0xD */ NO_SEGMENT,
        /* 0xE */ NO_SEGMENT,
        /* 0xF */ NO_SEGMENT } },
};



Scene sOvli2_Meteo[2] = {
    { OVERLAY_OFFSETS(ovl_i2),
      { /* 0x1 */ ROM_SEGMENT(ast_common),
        /* 0x2 */ ROM_SEGMENT(ast_bg_space),
        /* 0x3 */ ROM_SEGMENT(ast_arwing),
        /* 0x4 */ ROM_SEGMENT(ast_enmy_space),
        /* 0x5 */ ROM_SEGMENT(ast_text),
        /* 0x6 */ ROM_SEGMENT(ast_meteo),
        /* 0x7 */ ROM_SEGMENT(ast_warp_zone),
        /* 0x8 */ NO_SEGMENT,
        /* 0x9 */ NO_SEGMENT,
        /* 0xA */ NO_SEGMENT,
        /* 0xB */ NO_SEGMENT,
        /* 0xC */ NO_SEGMENT,
        /* 0xD */ NO_SEGMENT,
        /* 0xE */ ROM_SEGMENT(ast_great_fox),
        /* 0xF */ NO_SEGMENT } },
    { OVERLAY_OFFSETS(ovl_i2),
      { /* 0x1 */ ROM_SEGMENT(ast_common),
        /* 0x2 */ ROM_SEGMENT(ast_bg_space),
        /* 0x3 */ ROM_SEGMENT(ast_arwing),
        /* 0x4 */ ROM_SEGMENT(ast_enmy_space),
        /* 0x5 */ ROM_SEGMENT(ast_text),
        /* 0x6 */ ROM_SEGMENT(ast_meteo),
        /* 0x7 */ ROM_SEGMENT(ast_warp_zone),
        /* 0x8 */ NO_SEGMENT,
        /* 0x9 */ NO_SEGMENT,
        /* 0xA */ NO_SEGMENT,
        /* 0xB */ NO_SEGMENT,
        /* 0xC */ NO_SEGMENT,
        /* 0xD */ NO_SEGMENT,
        /* 0xE */ ROM_SEGMENT(ast_great_fox),
        /* 0xF */ NO_SEGMENT } },
};



Scene sOvli5_Titania[6] = {
    { OVERLAY_OFFSETS(ovl_i5),
      { /* 0x1 */ ROM_SEGMENT(ast_common),
        /* 0x2 */ ROM_SEGMENT(ast_bg_planet),
        /* 0x3 */ ROM_SEGMENT(ast_landmaster),
        /* 0x4 */ ROM_SEGMENT(ast_enmy_planet),
        /* 0x5 */ ROM_SEGMENT(ast_text),
        /* 0x6 */ ROM_SEGMENT(ast_titania),
        /* 0x7 */ ROM_SEGMENT(ast_7_ti_1),
        /* 0x8 */ NO_SEGMENT,
        /* 0x9 */ NO_SEGMENT,
        /* 0xA */ NO_SEGMENT,
        /* 0xB */ NO_SEGMENT,
        /* 0xC */ NO_SEGMENT,
        /* 0xD */ NO_SEGMENT,
        /* 0xE */ ROM_SEGMENT(ast_great_fox),
        /* 0xF */ NO_SEGMENT } },
    { OVERLAY_OFFSETS(ovl_i5),
      { /* 0x1 */ ROM_SEGMENT(ast_common),
        /* 0x2 */ ROM_SEGMENT(ast_bg_planet),
        /* 0x3 */ ROM_SEGMENT(ast_landmaster),
        /* 0x4 */ ROM_SEGMENT(ast_enmy_planet),
        /* 0x5 */ ROM_SEGMENT(ast_text),
        /* 0x6 */ ROM_SEGMENT(ast_titania),
        /* 0x7 */ ROM_SEGMENT(ast_7_ti_2),
        /* 0x8 */ NO_SEGMENT,
        /* 0x9 */ NO_SEGMENT,
        /* 0xA */ NO_SEGMENT,
        /* 0xB */ NO_SEGMENT,
        /* 0xC */ NO_SEGMENT,
        /* 0xD */ NO_SEGMENT,
        /* 0xE */ NO_SEGMENT,
        /* 0xF */ NO_SEGMENT } },
    { OVERLAY_OFFSETS(ovl_i5),
      { /* 0x1 */ ROM_SEGMENT(ast_common),
        /* 0x2 */ ROM_SEGMENT(ast_bg_planet),
        /* 0x3 */ ROM_SEGMENT(ast_landmaster),
        /* 0x4 */ ROM_SEGMENT(ast_enmy_planet),
        /* 0x5 */ ROM_SEGMENT(ast_text),
        /* 0x6 */ ROM_SEGMENT(ast_titania),
        /* 0x7 */ ROM_SEGMENT(ast_7_ti_2),
        /* 0x8 */ ROM_SEGMENT(ast_8_ti),
        /* 0x9 */ NO_SEGMENT,
        /* 0xA */ NO_SEGMENT,
        /* 0xB */ NO_SEGMENT,
        /* 0xC */ NO_SEGMENT,
        /* 0xD */ NO_SEGMENT,
        /* 0xE */ NO_SEGMENT,
        /* 0xF */ NO_SEGMENT } },
    { OVERLAY_OFFSETS(ovl_i5),
      { /* 0x1 */ ROM_SEGMENT(ast_common),
        /* 0x2 */ ROM_SEGMENT(ast_bg_planet),
        /* 0x3 */ ROM_SEGMENT(ast_landmaster),
        /* 0x4 */ ROM_SEGMENT(ast_enmy_planet),
        /* 0x5 */ ROM_SEGMENT(ast_text),
        /* 0x6 */ ROM_SEGMENT(ast_titania),
        /* 0x7 */ ROM_SEGMENT(ast_7_ti_2),
        /* 0x8 */ ROM_SEGMENT(ast_8_ti),
        /* 0x9 */ ROM_SEGMENT(ast_9_ti),
        /* 0xA */ NO_SEGMENT,
        /* 0xB */ NO_SEGMENT,
        /* 0xC */ NO_SEGMENT,
        /* 0xD */ NO_SEGMENT,
        /* 0xE */ NO_SEGMENT,
        /* 0xF */ NO_SEGMENT } },
    { OVERLAY_OFFSETS(ovl_i5),
      { /* 0x1 */ ROM_SEGMENT(ast_common),
        /* 0x2 */ ROM_SEGMENT(ast_bg_planet),
        /* 0x3 */ ROM_SEGMENT(ast_landmaster),
        /* 0x4 */ ROM_SEGMENT(ast_enmy_planet),
        /* 0x5 */ ROM_SEGMENT(ast_text),
        /* 0x6 */ ROM_SEGMENT(ast_titania),
        /* 0x7 */ ROM_SEGMENT(ast_7_ti_2),
        /* 0x8 */ ROM_SEGMENT(ast_8_ti),
        /* 0x9 */ ROM_SEGMENT(ast_9_ti),
        /* 0xA */ ROM_SEGMENT(ast_A_ti),
        /* 0xB */ NO_SEGMENT,
        /* 0xC */ NO_SEGMENT,
        /* 0xD */ NO_SEGMENT,
        /* 0xE */ NO_SEGMENT,
        /* 0xF */ NO_SEGMENT } },
    { OVERLAY_OFFSETS(ovl_i5),
      { /* 0x1 */ ROM_SEGMENT(ast_common),
        /* 0x2 */ ROM_SEGMENT(ast_bg_planet),
        /* 0x3 */ ROM_SEGMENT(ast_landmaster),
        /* 0x4 */ ROM_SEGMENT(ast_enmy_planet),
        /* 0x5 */ ROM_SEGMENT(ast_text),
        /* 0x6 */ ROM_SEGMENT(ast_titania),
        /* 0x7 */ ROM_SEGMENT(ast_7_ti_2),
        /* 0x8 */ ROM_SEGMENT(ast_8_ti),
        /* 0x9 */ ROM_SEGMENT(ast_9_ti),
        /* 0xA */ ROM_SEGMENT(ast_A_ti),
        /* 0xB */ NO_SEGMENT,
        /* 0xC */ NO_SEGMENT,
        /* 0xD */ NO_SEGMENT,
        /* 0xE */ ROM_SEGMENT(ast_great_fox),
        /* 0xF */ NO_SEGMENT } },
};

Scene sOvli2_SectorX[2] = {
    { OVERLAY_OFFSETS(ovl_i2),
      { /* 0x1 */ ROM_SEGMENT(ast_common),
        /* 0x2 */ ROM_SEGMENT(ast_bg_space),
        /* 0x3 */ ROM_SEGMENT(ast_arwing),
        /* 0x4 */ ROM_SEGMENT(ast_enmy_space),
        /* 0x5 */ ROM_SEGMENT(ast_text),
        /* 0x6 */ ROM_SEGMENT(ast_sector_x),
        /* 0x7 */ ROM_SEGMENT(ast_warp_zone),
        /* 0x8 */ NO_SEGMENT,
        /* 0x9 */ NO_SEGMENT,
        /* 0xA */ NO_SEGMENT,
        /* 0xB */ NO_SEGMENT,
        /* 0xC */ NO_SEGMENT,
        /* 0xD */ ROM_SEGMENT(ast_allies),
        /* 0xE */ NO_SEGMENT,
        /* 0xF */ NO_SEGMENT } },
    { OVERLAY_OFFSETS(ovl_i2),
      { /* 0x1 */ ROM_SEGMENT(ast_common),
        /* 0x2 */ ROM_SEGMENT(ast_bg_space),
        /* 0x3 */ ROM_SEGMENT(ast_arwing),
        /* 0x4 */ ROM_SEGMENT(ast_enmy_space),
        /* 0x5 */ ROM_SEGMENT(ast_text),
        /* 0x6 */ ROM_SEGMENT(ast_sector_x),
        /* 0x7 */ ROM_SEGMENT(ast_warp_zone),
        /* 0x8 */ NO_SEGMENT,
        /* 0x9 */ NO_SEGMENT,
        /* 0xA */ NO_SEGMENT,
        /* 0xB */ NO_SEGMENT,
        /* 0xC */ NO_SEGMENT,
        /* 0xD */ ROM_SEGMENT(ast_allies),
        /* 0xE */ ROM_SEGMENT(ast_great_fox),
        /* 0xF */ NO_SEGMENT } },
};

Scene sOvli4_SectorZ[1] = {
    { OVERLAY_OFFSETS(ovl_i4),
      { /* 0x1 */ ROM_SEGMENT(ast_common),
        /* 0x2 */ ROM_SEGMENT(ast_bg_space),
        /* 0x3 */ ROM_SEGMENT(ast_arwing),
        /* 0x4 */ ROM_SEGMENT(ast_enmy_space),
        /* 0x5 */ ROM_SEGMENT(ast_text),
        /* 0x6 */ ROM_SEGMENT(ast_sector_z),
        /* 0x7 */ NO_SEGMENT,
        /* 0x8 */ NO_SEGMENT,
        /* 0x9 */ NO_SEGMENT,
        /* 0xA */ NO_SEGMENT,
        /* 0xB */ NO_SEGMENT,
        /* 0xC */ NO_SEGMENT,
        /* 0xD */ ROM_SEGMENT(ast_allies),
        /* 0xE */ ROM_SEGMENT(ast_great_fox),
        /* 0xF */ NO_SEGMENT } },
};



Scene sOvli3_Aquas[1] = {
    { OVERLAY_OFFSETS(ovl_i3),
      { /* 0x1 */ ROM_SEGMENT(ast_common),
        /* 0x2 */ ROM_SEGMENT(ast_bg_planet),
        /* 0x3 */ ROM_SEGMENT(ast_blue_marine),
        /* 0x4 */ ROM_SEGMENT(ast_enmy_planet),
        /* 0x5 */ ROM_SEGMENT(ast_text),
        /* 0x6 */ ROM_SEGMENT(ast_aquas),
        /* 0x7 */ NO_SEGMENT,
        /* 0x8 */ NO_SEGMENT,
        /* 0x9 */ NO_SEGMENT,
        /* 0xA */ NO_SEGMENT,
        /* 0xB */ NO_SEGMENT,
        /* 0xC */ NO_SEGMENT,
        /* 0xD */ NO_SEGMENT,
        /* 0xE */ ROM_SEGMENT(ast_great_fox),
        /* 0xF */ NO_SEGMENT } },
};


Scene sOvli3_Area6[1] = {
    { OVERLAY_OFFSETS(ovl_i3),
      { /* 0x1 */ ROM_SEGMENT(ast_common),
        /* 0x2 */ ROM_SEGMENT(ast_bg_space),
        /* 0x3 */ ROM_SEGMENT(ast_arwing),
        /* 0x4 */ ROM_SEGMENT(ast_enmy_space),
        /* 0x5 */ ROM_SEGMENT(ast_text),
        /* 0x6 */ ROM_SEGMENT(ast_area_6),
        /* 0x7 */ NO_SEGMENT,
        /* 0x8 */ NO_SEGMENT,
        /* 0x9 */ NO_SEGMENT,
        /* 0xA */ NO_SEGMENT,
        /* 0xB */ NO_SEGMENT,
        /* 0xC */ NO_SEGMENT,
        /* 0xD */ NO_SEGMENT,
        /* 0xE */ ROM_SEGMENT(ast_great_fox),
        /* 0xF */ NO_SEGMENT } },
};


Scene sOvli4_Fortuna[2] = {
    { OVERLAY_OFFSETS(ovl_i4),
      { /* 0x1 */ ROM_SEGMENT(ast_common),
        /* 0x2 */ ROM_SEGMENT(ast_bg_planet),
        /* 0x3 */ ROM_SEGMENT(ast_arwing),
        /* 0x4 */ ROM_SEGMENT(ast_enmy_planet),
        /* 0x5 */ ROM_SEGMENT(ast_text),
        /* 0x6 */ ROM_SEGMENT(ast_fortuna),
        /* 0x7 */ NO_SEGMENT,
        /* 0x8 */ NO_SEGMENT,
        /* 0x9 */ NO_SEGMENT,
        /* 0xA */ NO_SEGMENT,
        /* 0xB */ NO_SEGMENT,
        /* 0xC */ NO_SEGMENT,
        /* 0xD */ NO_SEGMENT,
        /* 0xE */ NO_SEGMENT,
        /* 0xF */ ROM_SEGMENT(ast_star_wolf) } },
    { OVERLAY_OFFSETS(ovl_i4),
      { /* 0x1 */ ROM_SEGMENT(ast_common),
        /* 0x2 */ ROM_SEGMENT(ast_bg_planet),
        /* 0x3 */ ROM_SEGMENT(ast_arwing),
        /* 0x4 */ ROM_SEGMENT(ast_enmy_planet),
        /* 0x5 */ ROM_SEGMENT(ast_text),
        /* 0x6 */ ROM_SEGMENT(ast_fortuna),
        /* 0x7 */ NO_SEGMENT,
        /* 0x8 */ NO_SEGMENT,
        /* 0x9 */ NO_SEGMENT,
        /* 0xA */ NO_SEGMENT,
        /* 0xB */ NO_SEGMENT,
        /* 0xC */ NO_SEGMENT,
        /* 0xD */ NO_SEGMENT,
        /* 0xE */ ROM_SEGMENT(ast_great_fox),
        /* 0xF */ NO_SEGMENT } },
};


Scene sOvli3_Unk4[1] = {
    { OVERLAY_OFFSETS(ovl_i3),
      { /* 0x1 */ ROM_SEGMENT(ast_common),
        /* 0x2 */ ROM_SEGMENT(ast_bg_space),
        /* 0x3 */ ROM_SEGMENT(ast_arwing),
        /* 0x4 */ ROM_SEGMENT(ast_enmy_space),
        /* 0x5 */ ROM_SEGMENT(ast_text),
        /* 0x6 */ ROM_SEGMENT(ast_area_6),
        /* 0x7 */ NO_SEGMENT,
        /* 0x8 */ NO_SEGMENT,
        /* 0x9 */ NO_SEGMENT,
        /* 0xA */ NO_SEGMENT,
        /* 0xB */ NO_SEGMENT,
        /* 0xC */ NO_SEGMENT,
        /* 0xD */ NO_SEGMENT,
        /* 0xE */ ROM_SEGMENT(ast_great_fox),
        /* 0xF */ NO_SEGMENT } },
};


Scene sOvli6_SectorY[1] = {
    { OVERLAY_OFFSETS(ovl_i6),
      { /* 0x1 */ ROM_SEGMENT(ast_common),
        /* 0x2 */ ROM_SEGMENT(ast_bg_space),
        /* 0x3 */ ROM_SEGMENT(ast_arwing),
        /* 0x4 */ ROM_SEGMENT(ast_enmy_space),
        /* 0x5 */ ROM_SEGMENT(ast_text),
        /* 0x6 */ ROM_SEGMENT(ast_sector_y),
        /* 0x7 */ NO_SEGMENT,
        /* 0x8 */ NO_SEGMENT,
        /* 0x9 */ NO_SEGMENT,
        /* 0xA */ NO_SEGMENT,
        /* 0xB */ NO_SEGMENT,
        /* 0xC */ NO_SEGMENT,
        /* 0xD */ NO_SEGMENT,
        /* 0xE */ NO_SEGMENT,
        /* 0xF */ NO_SEGMENT } },
};

Scene sOvli3_Solar[1] = {
    { OVERLAY_OFFSETS(ovl_i3),
      { /* 0x1 */ ROM_SEGMENT(ast_common),
        /* 0x2 */ ROM_SEGMENT(ast_bg_planet),
        /* 0x3 */ ROM_SEGMENT(ast_arwing),
        /* 0x4 */ ROM_SEGMENT(ast_enmy_planet),
        /* 0x5 */ ROM_SEGMENT(ast_text),
        /* 0x6 */ ROM_SEGMENT(ast_solar),
        /* 0x7 */ NO_SEGMENT,
        /* 0x8 */ NO_SEGMENT,
        /* 0x9 */ NO_SEGMENT,
        /* 0xA */ NO_SEGMENT,
        /* 0xB */ NO_SEGMENT,
        /* 0xC */ NO_SEGMENT,
        /* 0xD */ ROM_SEGMENT(ast_allies),
        /* 0xE */ NO_SEGMENT,
        /* 0xF */ NO_SEGMENT } },
};

Scene sOvli3_Zoness[1] = {
    { OVERLAY_OFFSETS(ovl_i3),
      { /* 0x1 */ ROM_SEGMENT(ast_common),
        /* 0x2 */ ROM_SEGMENT(ast_bg_planet),
        /* 0x3 */ ROM_SEGMENT(ast_arwing),
        /* 0x4 */ ROM_SEGMENT(ast_enmy_planet),
        /* 0x5 */ ROM_SEGMENT(ast_text),
        /* 0x6 */ ROM_SEGMENT(ast_zoness),
        /* 0x7 */ NO_SEGMENT,
        /* 0x8 */ NO_SEGMENT,
        /* 0x9 */ NO_SEGMENT,
        /* 0xA */ NO_SEGMENT,
        /* 0xB */ NO_SEGMENT,
        /* 0xC */ NO_SEGMENT,
        /* 0xD */ ROM_SEGMENT(ast_allies),
        /* 0xE */ NO_SEGMENT,
        /* 0xF */ NO_SEGMENT } },
};


Scene sOvli1_Venom1[1] = {
    { OVERLAY_OFFSETS(ovl_i1),
      { /* 0x1 */ ROM_SEGMENT(ast_common),
        /* 0x2 */ ROM_SEGMENT(ast_bg_planet),
        /* 0x3 */ ROM_SEGMENT(ast_arwing),
        /* 0x4 */ ROM_SEGMENT(ast_enmy_planet),
        /* 0x5 */ ROM_SEGMENT(ast_text),
        /* 0x6 */ ROM_SEGMENT(ast_venom_1),
        /* 0x7 */ NO_SEGMENT,
        /* 0x8 */ NO_SEGMENT,
        /* 0x9 */ ROM_SEGMENT(ast_ve1_boss),
        /* 0xA */ NO_SEGMENT,
        /* 0xB */ NO_SEGMENT,
        /* 0xC */ NO_SEGMENT,
        /* 0xD */ ROM_SEGMENT(ast_allies),
        /* 0xE */ NO_SEGMENT,
        /* 0xF */ NO_SEGMENT } },
};



Scene sOvli6_Andross[1] = {
    { OVERLAY_OFFSETS(ovl_i6),
      { /* 0x1 */ ROM_SEGMENT(ast_common),
        /* 0x2 */ ROM_SEGMENT(ast_bg_planet),
        /* 0x3 */ ROM_SEGMENT(ast_arwing),
        /* 0x4 */ NO_SEGMENT,
        /* 0x5 */ ROM_SEGMENT(ast_text),
        /* 0x6 */ ROM_SEGMENT(ast_venom_2),
        /* 0x7 */ NO_SEGMENT,
        /* 0x8 */ NO_SEGMENT,
        /* 0x9 */ NO_SEGMENT,
        /* 0xA */ NO_SEGMENT,
        /* 0xB */ NO_SEGMENT,
        /* 0xC */ ROM_SEGMENT(ast_andross),
        /* 0xD */ ROM_SEGMENT(ast_allies),
        /* 0xE */ NO_SEGMENT,
        /* 0xF */ NO_SEGMENT } },
};


Scene sOvli6_Venom2[2] = {
    { OVERLAY_OFFSETS(ovl_i6),
      { /* 0x1 */ ROM_SEGMENT(ast_common),
        /* 0x2 */ ROM_SEGMENT(ast_bg_planet),
        /* 0x3 */ ROM_SEGMENT(ast_arwing),
        /* 0x4 */ ROM_SEGMENT(ast_enmy_planet),
        /* 0x5 */ ROM_SEGMENT(ast_text),
        /* 0x6 */ ROM_SEGMENT(ast_venom_2),
        /* 0x7 */ NO_SEGMENT,
        /* 0x8 */ NO_SEGMENT,
        /* 0x9 */ NO_SEGMENT,
        /* 0xA */ NO_SEGMENT,
        /* 0xB */ NO_SEGMENT,
        /* 0xC */ NO_SEGMENT,
        /* 0xD */ NO_SEGMENT,
        /* 0xE */ NO_SEGMENT,
        /* 0xF */ ROM_SEGMENT(ast_star_wolf) } },
    { OVERLAY_OFFSETS(ovl_i6),
      { /* 0x1 */ ROM_SEGMENT(ast_common),
        /* 0x2 */ ROM_SEGMENT(ast_bg_planet),
        /* 0x3 */ ROM_SEGMENT(ast_arwing),
        /* 0x4 */ ROM_SEGMENT(ast_enmy_planet),
        /* 0x5 */ ROM_SEGMENT(ast_text),
        /* 0x6 */ ROM_SEGMENT(ast_venom_2),
        /* 0x7 */ NO_SEGMENT,
        /* 0x8 */ NO_SEGMENT,
        /* 0x9 */ NO_SEGMENT,
        /* 0xA */ NO_SEGMENT,
        /* 0xB */ NO_SEGMENT,
        /* 0xC */ NO_SEGMENT,
        /* 0xD */ NO_SEGMENT,
        /* 0xE */ ROM_SEGMENT(ast_great_fox),
        /* 0xF */ NO_SEGMENT } },
};




Scene sOvli2_Setup20[1] = {
    { OVERLAY_OFFSETS(ovl_i2),
      { /* 0x1 */ ROM_SEGMENT(ast_common),
        /* 0x2 */ ROM_SEGMENT(ast_bg_planet),
        /* 0x3 */ ROM_SEGMENT(ast_arwing),
        /* 0x4 */ ROM_SEGMENT(ast_enmy_planet),
        /* 0x5 */ ROM_SEGMENT(ast_text),
        /* 0x6 */ ROM_SEGMENT(ast_ve1_boss),
        /* 0x7 */ NO_SEGMENT,
        /* 0x8 */ NO_SEGMENT,
        /* 0x9 */ NO_SEGMENT,
        /* 0xA */ NO_SEGMENT,
        /* 0xB */ NO_SEGMENT,
        /* 0xC */ NO_SEGMENT,
        /* 0xD */ NO_SEGMENT,
        /* 0xE */ NO_SEGMENT,
        /* 0xF */ NO_SEGMENT } },
};

Scene sOvli4_Bolse[1] = {
    { OVERLAY_OFFSETS(ovl_i4),
      { /* 0x1 */ ROM_SEGMENT(ast_common),
        /* 0x2 */ ROM_SEGMENT(ast_bg_space),
        /* 0x3 */ ROM_SEGMENT(ast_arwing),
        /* 0x4 */ ROM_SEGMENT(ast_enmy_space),
        /* 0x5 */ ROM_SEGMENT(ast_text),
        /* 0x6 */ ROM_SEGMENT(ast_bolse),
        /* 0x7 */ NO_SEGMENT,
        /* 0x8 */ NO_SEGMENT,
        /* 0x9 */ NO_SEGMENT,
        /* 0xA */ NO_SEGMENT,
        /* 0xB */ NO_SEGMENT,
        /* 0xC */ NO_SEGMENT,
        /* 0xD */ NO_SEGMENT,
        /* 0xE */ NO_SEGMENT,
        /* 0xF */ ROM_SEGMENT(ast_star_wolf) } },
};

Scene sOvli4_Katina[1] = {
    { OVERLAY_OFFSETS(ovl_i4),
      { /* 0x1 */ ROM_SEGMENT(ast_common),
        /* 0x2 */ ROM_SEGMENT(ast_bg_planet),
        /* 0x3 */ ROM_SEGMENT(ast_arwing),
        /* 0x4 */ ROM_SEGMENT(ast_enmy_planet),
        /* 0x5 */ ROM_SEGMENT(ast_text),
        /* 0x6 */ ROM_SEGMENT(ast_katina),
        /* 0x7 */ NO_SEGMENT,
        /* 0x8 */ NO_SEGMENT,
        /* 0x9 */ NO_SEGMENT,
        /* 0xA */ NO_SEGMENT,
        /* 0xB */ NO_SEGMENT,
        /* 0xC */ NO_SEGMENT,
        /* 0xD */ ROM_SEGMENT(ast_allies),
        /* 0xE */ NO_SEGMENT,
        /* 0xF */ ROM_SEGMENT(ast_star_wolf) } },
};

Scene sOvli5_Macbeth[2] = {
    { OVERLAY_OFFSETS(ovl_i5),
      { /* 0x1 */ ROM_SEGMENT(ast_common),
        /* 0x2 */ ROM_SEGMENT(ast_bg_planet),
        /* 0x3 */ ROM_SEGMENT(ast_landmaster),
        /* 0x4 */ ROM_SEGMENT(ast_enmy_planet),
        /* 0x5 */ ROM_SEGMENT(ast_text),
        /* 0x6 */ ROM_SEGMENT(ast_macbeth),
        /* 0x7 */ NO_SEGMENT,
        /* 0x8 */ NO_SEGMENT,
        /* 0x9 */ NO_SEGMENT,
        /* 0xA */ NO_SEGMENT,
        /* 0xB */ NO_SEGMENT,
        /* 0xC */ NO_SEGMENT,
        /* 0xD */ ROM_SEGMENT(ast_allies),
        /* 0xE */ NO_SEGMENT,
        /* 0xF */ NO_SEGMENT } },
    { OVERLAY_OFFSETS(ovl_i5),
      { /* 0x1 */ ROM_SEGMENT(ast_common),
        /* 0x2 */ ROM_SEGMENT(ast_bg_planet),
        /* 0x3 */ ROM_SEGMENT(ast_landmaster),
        /* 0x4 */ ROM_SEGMENT(ast_enmy_planet),
        /* 0x5 */ ROM_SEGMENT(ast_text),
        /* 0x6 */ ROM_SEGMENT(ast_macbeth),
        /* 0x7 */ NO_SEGMENT,
        /* 0x8 */ NO_SEGMENT,
        /* 0x9 */ NO_SEGMENT,
        /* 0xA */ NO_SEGMENT,
        /* 0xB */ NO_SEGMENT,
        /* 0xC */ NO_SEGMENT,
        /* 0xD */ NO_SEGMENT,
        /* 0xE */ ROM_SEGMENT(ast_great_fox),
        /* 0xF */ NO_SEGMENT } },
};

Scene sOvli1_Training[1] = {
    { OVERLAY_OFFSETS(ovl_i1),
      { /* 0x1 */ ROM_SEGMENT(ast_common),
        /* 0x2 */ ROM_SEGMENT(ast_bg_planet),
        /* 0x3 */ ROM_SEGMENT(ast_arwing),
        /* 0x4 */ ROM_SEGMENT(ast_enmy_planet),
        /* 0x5 */ ROM_SEGMENT(ast_text),
        /* 0x6 */ ROM_SEGMENT(ast_training),
        /* 0x7 */ NO_SEGMENT,
        /* 0x8 */ NO_SEGMENT,
        /* 0x9 */ NO_SEGMENT,
        /* 0xA */ NO_SEGMENT,
        /* 0xB */ NO_SEGMENT,
        /* 0xC */ NO_SEGMENT,
        /* 0xD */ NO_SEGMENT,
        /* 0xE */ NO_SEGMENT,
        /* 0xF */ ROM_SEGMENT(ast_star_wolf) } },
};


Scene sOvli2_Versus[2] = {
    { OVERLAY_OFFSETS(ovl_i2),
      { /* 0x1 */ ROM_SEGMENT(ast_common),
        /* 0x2 */ ROM_SEGMENT(ast_bg_planet),
        /* 0x3 */ ROM_SEGMENT(ast_versus),
        /* 0x4 */ ROM_SEGMENT(ast_enmy_planet),
        /* 0x5 */ ROM_SEGMENT(ast_text),
        /* 0x6 */ NO_SEGMENT,
        /* 0x7 */ ROM_SEGMENT(ast_vs_menu),
        /* 0x8 */ NO_SEGMENT,
        /* 0x9 */ NO_SEGMENT,
        /* 0xA */ NO_SEGMENT,
        /* 0xB */ NO_SEGMENT,
        /* 0xC */ NO_SEGMENT,
        /* 0xD */ NO_SEGMENT,
        /* 0xE */ NO_SEGMENT,
        /* 0xF */ NO_SEGMENT } },
    { OVERLAY_OFFSETS(ovl_i2),
      { /* 0x1 */ ROM_SEGMENT(ast_common),
        /* 0x2 */ ROM_SEGMENT(ast_bg_space),
        /* 0x3 */ ROM_SEGMENT(ast_versus),
        /* 0x4 */ ROM_SEGMENT(ast_enmy_space),
        /* 0x5 */ ROM_SEGMENT(ast_text),
        /* 0x6 */ NO_SEGMENT,
        /* 0x7 */ ROM_SEGMENT(ast_vs_menu),
        /* 0x8 */ NO_SEGMENT,
        /* 0x9 */ NO_SEGMENT,
        /* 0xA */ NO_SEGMENT,
        /* 0xB */ NO_SEGMENT,
        /* 0xC */ NO_SEGMENT,
        /* 0xD */ NO_SEGMENT,
        /* 0xE */ NO_SEGMENT,
        /* 0xF */ NO_SEGMENT } },
};







        #endif

enum NewSceneId {
    sERROR = -1,
    sNoOvl_Logo = 0, // 1
    sOvlending_Ending, // 6
    sOvlmenu_Title, // 1
    sOvlmenu_Option, // 1
    sOvlmenu_Map, // 1
    sOvlmenu_GameOver, // 1
    sOvli1_Corneria, // 1
    sOvli2_Meteo, // 2
    sOvli5_Titania, // 6
    sOvli2_SectorX, // 2
    sOvli4_SectorZ, //1
    sOvli3_Aquas, // 1
    sOvli3_Area6, //1
    sOvli4_Fortuna,//2
    sOvli3_Unk4,//1
    sOvli6_SectorY,//1
    sOvli3_Solar,//1
    sOvli3_Zoness,//1
    sOvli1_Venom1,//1
    sOvli6_Andross,//1
    sOvli6_Venom2,//2
    sOvli2_Setup20,//1
    sOvli4_Bolse,//1
    sOvli4_Katina,//1
    sOvli5_Macbeth,//2
    sOvli1_Training,//1
    sOvli2_Versus,//2
    sOvlUnused_Unk,//1
};


typedef struct NewScene_s {

    enum NewSceneId id;
    int snum;
    char segs[16][32];
    
} NewScene;

NewScene ns_logo[1] = {
  {sNoOvl_Logo,
  0,
  {
    "","","","",
    "","","","",
    "","","","",
    "","","","ast_logo",
  }}
};

NewScene ns_ending[6] = {
  {sOvlending_Ending,
  0,
  {
    "","","","ast_arwing",
    "","ast_text","","ast_ending",
    "","","","",
    "","ast_allies","ast_great_fox","",
  }},

  {sOvlending_Ending,
  1,
  {
    "","","","",
    "","ast_text","ast_title","ast_ending",
    "ast_ending_award_front","","","",
    "","","","",
  }},

  {sOvlending_Ending,
  2,
  {
    "","","","",
    "","ast_text","ast_title","ast_ending",
    "ast_ending_award_back","","","",
    "","","","",
  }},

    {sOvlending_Ending,
  3,
  {
    "","","","",
    "","ast_text","ast_title","ast_ending",
    "ast_ending_award_expert","","","",
    "","","","",
  }},

    {sOvlending_Ending,
  4,
  {
    "","","","",
    "","ast_text","ast_title","ast_ending",
    "","","","",
    "","","ast_great_fox","",
  }},

    {sOvlending_Ending,
  5,
  {
    "","","","ast_arwing",
    "","ast_text","","ast_ending",
    "ast_ending_expert","ast_font_3d","","",
    "","","ast_great_fox","",
  }},

};

NewScene ns_title[1] = {
  {sOvlmenu_Title,
  0,
  {
    "","","","ast_arwing",
    "","ast_text","ast_title","",
    "","","","",
    "","","ast_great_fox","",
  }
  }
};


NewScene ns_option[1] = {
  {sOvlmenu_Option,
  0,
  {
    "","","","",
    "","ast_text","ast_map","ast_vs_menu",
    "ast_option","ast_font_3d","","",
    "","","","",
  }
  }
};

NewScene ns_map[1] = {
  {sOvlmenu_Map,
  0,
  {
    "","","","ast_arwing",
    "","ast_text","ast_map","",
    "","ast_font_3d","","",
    "","","","",
  }
  }
};


NewScene ns_gameover[1] = {
  {sOvlmenu_GameOver,
  0,
  {
    "","","","",
    "","ast_text","","",
    "","","","",
    "ast_andross","","","",
  }
  }
};

NewScene ns_corneria[1] = {
{sOvli1_Corneria,
  0,
      { "",
        /* 0x1 */ "ast_common",
        /* 0x2 */ "ast_bg_planet",
        /* 0x3 */ "ast_arwing",
        /* 0x4 */ "ast_enmy_planet",
        /* 0x5 */ "ast_text",
        /* 0x6 */ "ast_corneria",
        /* 0x7 */ "",
        /* 0x8 */ "",
        /* 0x9 */ "",
        /* 0xA */ "",
        /* 0xB */ "",
        /* 0xC */ "",
        /* 0xD */ "",
        /* 0xE */ "",
        /* 0xF */ "" } },
};


NewScene ns_meteo[2] = {
{sOvli2_Meteo,0,
  {"",/* 0x1 */ "ast_common",
        /* 0x2 */ "ast_bg_space",
        /* 0x3 */ "ast_arwing",
        /* 0x4 */ "ast_enmy_space",
        /* 0x5 */ "ast_text",
        /* 0x6 */ "ast_meteo",
        /* 0x7 */ "ast_warp_zone",
        /* 0x8 */ "",
        /* 0x9 */ "",
        /* 0xA */ "",
        /* 0xB */ "",
        /* 0xC */ "",
        /* 0xD */ "",
        /* 0xE */ "ast_great_fox",
        /* 0xF */ ""}},


{sOvli2_Meteo,1,
  
  { "",/* 0x1 */ "ast_common",
        /* 0x2 */ "ast_bg_space",
        /* 0x3 */ "ast_arwing",
        /* 0x4 */ "ast_enmy_space",
        /* 0x5 */ "ast_text",
        /* 0x6 */ "ast_meteo",
        /* 0x7 */ "ast_warp_zone",
        /* 0x8 */ "",
        /* 0x9 */ "",
        /* 0xA */ "",
        /* 0xB */ "",
        /* 0xC */ "",
        /* 0xD */ "",
        /* 0xE */ "ast_great_fox",
        /* 0xF */ ""}},

};

NewScene ns_titania[6]= {
{sOvli5_Titania,0,
{"",
  /* 0x1 */"ast_common",
  /* 0x2 */"ast_bg_planet",
  /* 0x3 */"ast_landmaster",
  /* 0x4 */"ast_enmy_planet",
  /* 0x5 */"ast_text",
  /* 0x6 */"ast_titania",
  /* 0x7 */"ast_7_ti_1",
  /* 0x8 */"",
  /* 0x9 */"",
  /* 0xa */"",
  /* 0xb */"",
  /* 0xc */"",
  /* 0xd */"",
  /* 0xe */"ast_great_fox",
  /* 0xf */"",
}
},
{sOvli5_Titania,1,{
  "",
  /* 0x1 */"ast_common",
  /* 0x2 */"ast_bg_planet",
  /* 0x3 */"ast_landmaster",
  /* 0x4 */"ast_enmy_planet",
  /* 0x5 */"ast_text",
  /* 0x6 */"ast_titania",
  /* 0x7 */"ast_7_ti_2",
  /* 0x8 */"",
  /* 0x9 */"",
  /* 0xa */"",
  /* 0xb */"",
  /* 0xc */"",
  /* 0xd */"",
  /* 0xe */"",
  /* 0xf */"",
}
},
{sOvli5_Titania,2,{"","ast_common","ast_bg_planet","ast_landmaster",
  "ast_enmy_planet","ast_text","ast_titania","ast_7_ti_2",
  "ast_8_ti","","","",
  "","","","",
}
},
{sOvli5_Titania,3,{"","ast_common","ast_bg_planet","ast_landmaster",
  "ast_enmy_planet","ast_text","ast_titania","ast_7_ti_2",
  "ast_8_ti","ast_9_ti","","",
  "","","","",
}
},
{sOvli5_Titania,4,{"","ast_common","ast_bg_planet","ast_landmaster",
  "ast_enmy_planet","ast_text","ast_titania","ast_7_ti_2",
  "ast_8_ti","ast_9_ti","ast_A_ti","",
  "","","","",
}
},
{sOvli5_Titania,5,{"","ast_common","ast_bg_planet","ast_landmaster",
  "ast_enmy_planet","ast_text","ast_titania","ast_7_ti_2",
  "ast_8_ti","ast_9_ti","ast_A_ti","",
  "","","ast_great_fox","",
}
},

};


NewScene ns_sectorx[2] = {
{sOvli2_SectorX
,0,
{
    "","ast_common","ast_bg_space","ast_arwing",
  "ast_enmy_space","ast_text","ast_sector_x","ast_warp_zone",
  "","","","",
  "","ast_allies","","",

}
},

{sOvli2_SectorX
,1,
{
   "","ast_common","ast_bg_space","ast_arwing",
  "ast_enmy_space","ast_text","ast_sector_x","ast_warp_zone",
  "","","","",
  "","ast_allies","ast_great_fox","",

}



}
};


NewScene ns_sectorz[1] = {
{sOvli4_SectorZ
,0,
{
    "","ast_common","ast_bg_space","ast_arwing",
  "ast_enmy_space","ast_text","ast_sector_z","",
  "","","","",
  "","ast_allies","ast_great_fox","",

}
}
};

NewScene ns_aquas[1] = {
{sOvli3_Aquas
,0,
{
    "","ast_common","ast_bg_planet","ast_blue_marine",
  "ast_enmy_planet","ast_text","ast_aquas","",
  "","","","",
  "","","ast_great_fox","",

}
}
};




NewScene ns_area6[1] = {
{sOvli3_Area6
,0,
{
    "","ast_common","ast_bg_space","ast_arwing",
  "ast_enmy_space","ast_text","ast_area_6","",
  "","","","",
  "","","ast_great_fox","",

}
}
};




NewScene ns_fortuna[2] = {
{sOvli4_Fortuna
,0,
{
    "","ast_common","ast_bg_planet","ast_arwing",
  "ast_enmy_planet","ast_text","ast_fortuna","",
  "","","","",
  "","","","ast_star_wolf",

}
},

{sOvli4_Fortuna
,1,
{
   "","ast_common","ast_bg_planet","ast_arwing",
  "ast_enmy_planet","ast_text","ast_fortuna","",
  "","","","",
  "","","ast_great_fox","",

}



}
};





NewScene ns_unk4[1] = {
{sOvli3_Unk4
,0,
{
    "","ast_common","ast_bg_space","ast_arwing",
  "ast_enmy_space","ast_text","ast_area_6","",
  "","","","",
  "","","ast_great_fox","",

}
}
};



NewScene ns_sectory[1] = {
{sOvli6_SectorY
,0,
{
    "","ast_common","ast_bg_space","ast_arwing",
  "ast_enmy_space","ast_text","ast_sector_y","",
  "","","","",
  "","","","",

}
}
};




NewScene ns_solar[1] = {
{sOvli3_Solar
,0,
{
    "","ast_common","ast_bg_planet","ast_arwing",
  "ast_enmy_planet","ast_text","ast_solar","",
  "","","","",
  "","ast_allies","","",

}
}
};



NewScene ns_zoness[1] = {
{sOvli3_Zoness
,0,
{
    "","ast_common","ast_bg_planet","ast_arwing",
  "ast_enmy_planet","ast_text","ast_zoness","",
  "","","","",
  "","ast_allies","","",

}
}
};


NewScene ns_venom1[1] = {
{sOvli1_Venom1
,0,
{
    "","ast_common","ast_bg_planet","ast_arwing",
  "ast_enmy_planet","ast_text","ast_venom_1","",
  "","ast_ve1_boss","","",
  "","ast_allies","","",

}
}
};


NewScene ns_andross[1] = {
{sOvli6_Andross
,0,
{
    "","ast_common","ast_bg_planet","ast_arwing",
  "","ast_text","ast_venom_2","",
  "","","","",
  "ast_andross","ast_allies","","",

}
}
};


NewScene ns_venom2[2] = {
{sOvli6_Venom2,0,
{
    "","ast_common","ast_bg_planet","ast_arwing",
  "ast_enmy_planet","ast_text","ast_venom_2","",
  "","","","",
  "","","","ast_star_wolf",

}
},

{sOvli6_Venom2,1,
{
    "","ast_common","ast_bg_planet","ast_arwing",
  "ast_enmy_planet","ast_text","ast_venom_2","",
  "","","","",
  "","","ast_great_fox","",

}
},
};

NewScene ns_setup20[1] = {
{
sOvli2_Setup20,0,
{
     "","ast_common","ast_bg_planet","ast_arwing",
  "ast_enmy_planet","ast_text","ast_ve1_boss","",
  "","","","",
  "","","","",

}

}

};



NewScene ns_bolse[1] = {
{
sOvli4_Bolse,0,
{
     "","ast_common","ast_bg_space","ast_arwing",
  "ast_enmy_space","ast_text","ast_bolse","",
  "","","","",
  "","","","ast_star_wolf",

}

}

};


NewScene ns_katina[1] = {
{
sOvli4_Katina,0,
{
     "","ast_common","ast_bg_planet","ast_arwing",
  "ast_enmy_planet","ast_text","ast_katina","",
  "","","","",
  "","ast_allies","","ast_star_wolf",

}

}

};



NewScene ns_macbeth[2] = {
{sOvli5_Macbeth,0,
{
     "","ast_common","ast_bg_planet","ast_landmaster",
  "ast_enmy_planet","ast_text","ast_macbeth","",
  "","","","",
  "","ast_allies","","",

}
},

{sOvli5_Macbeth,1,

{
     "","ast_common","ast_bg_planet","ast_landmaster",
  "ast_enmy_planet","ast_text","ast_macbeth","",
  "","","","",
  "","","ast_great_fox","",

}
}



};




NewScene ns_training[1] = {
{
sOvli1_Training,0,
{
     "","ast_common","ast_bg_planet","ast_arwing",
  "ast_enmy_planet","ast_text","ast_training","",
  "","","","",
  "","","","ast_star_wolf",

}

}

};




NewScene ns_versus[2] = {
{sOvli2_Versus,0,
{
     "","ast_common","ast_bg_planet","ast_versus",
  "ast_enmy_planet","ast_text","","ast_vs_menu",
  "","","","",
  "","ast_allies","","",

}
},

{sOvli2_Versus,1,

{
     "","ast_common","ast_bg_space","ast_versus",
  "ast_enmy_space","ast_text","","ast_vs_menu",
  "","","","",
  "","","","",

}
}



};



NewScene ns_unusedunk1[1] = {
{
sOvlUnused_Unk,0,
{
     "","","","",
  "","","","",
  "","","","",
  "","","","",

}

}

};



#if 0
 




Scene sOvlUnused_Unk[1] = {
    { OVERLAY_OFFSETS(ovl_unused),
      { /* 0x1 */ NO_SEGMENT,
        /* 0x2 */ NO_SEGMENT,
        /* 0x3 */ NO_SEGMENT,
        /* 0x4 */ NO_SEGMENT,
        /* 0x5 */ NO_SEGMENT,
        /* 0x6 */ NO_SEGMENT,
        /* 0x7 */ NO_SEGMENT,
        /* 0x8 */ NO_SEGMENT,
        /* 0x9 */ NO_SEGMENT,
        /* 0xA */ NO_SEGMENT,
        /* 0xB */ NO_SEGMENT,
        /* 0xC */ NO_SEGMENT,
        /* 0xD */ NO_SEGMENT,
        /* 0xE */ NO_SEGMENT,
        /* 0xF */ NO_SEGMENT } },
};
#endif