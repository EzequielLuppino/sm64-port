#ifndef GFX_3DS_MENU_H
#define GFX_3DS_MENU_H

#include <stdio.h>
#include <stdbool.h>

#include "gfx_3ds_common.h"

#include "src/minimap/textures/mode_400_t3x.h"
#include "src/minimap/textures/mode_800_t3x.h"
#include "src/minimap/textures/aa_on_t3x.h"
#include "src/minimap/textures/aa_off_t3x.h"
#include "src/minimap/textures/resume_t3x.h"
#include "src/minimap/textures/exit_t3x.h"
#include "src/minimap/textures/tab_map_t3x.h"
#include "src/minimap/textures/tab_cheats_t3x.h"
#include "src/minimap/textures/tab_setup_t3x.h"
#include "src/minimap/textures/lbl_inf_health_t3x.h"
#include "src/minimap/textures/lbl_inf_air_t3x.h"
#include "src/minimap/textures/lbl_inf_lives_t3x.h"
#include "src/minimap/textures/lbl_no_damage_t3x.h"
#include "src/minimap/textures/lbl_moon_jump_t3x.h"
#include "src/minimap/textures/lbl_enemies_ignore_t3x.h"
#include "src/minimap/textures/lbl_power_none_t3x.h"
#include "src/minimap/textures/lbl_power_wing_t3x.h"
#include "src/minimap/textures/lbl_power_metal_t3x.h"
#include "src/minimap/textures/lbl_power_vanish_t3x.h"
#include "src/minimap/textures/lbl_on_t3x.h"
#include "src/minimap/textures/lbl_off_t3x.h"

struct gfx_configuration
{
   bool useAA;
   bool useWide;
};

extern struct gfx_configuration gfx_config;

typedef enum {
    DO_NOTHING,
    CONFIG_CHANGED,
    TAB_CHANGED
} menu_action;

typedef enum {
    BOTTOM_TAB_MAP = 0,
    BOTTOM_TAB_CHEATS = 1,
    BOTTOM_TAB_SETUP = 2
} bottom_tab;

extern bottom_tab gBottomScreenTab;

// Position of the finger on the bottom screen while the file-select touchpad
// is active (in touch coordinates, origin top-left). -1 means "no touch yet".
extern int gTouchpadX;
extern int gTouchpadY;
extern bool gTouchpadHeld;

void gfx_3ds_menu_init();
int gfx_3ds_bottom_screen_draw(float *vertex_buffer, int vertex_offset);
menu_action gfx_3ds_menu_on_touch(int x, int y);

#endif
