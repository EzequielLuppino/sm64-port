#ifdef TARGET_N3DS

// hack for redefinition of types in libctru
// All 3DS includes must be done inside of an equivalent
// #define/undef block to avoid type redefinition issues.
#define u64 __3ds_u64
#define s64 __3ds_s64
#define u32 __3ds_u32
#define vu32 __3ds_vu32
#define vs32 __3ds_vs32
#define s32 __3ds_s32
#define u16 __3ds_u16
#define s16 __3ds_s16
#define u8 __3ds_u8
#define s8 __3ds_s8
#include <3ds/types.h>
#include <3ds.h>
#undef u64
#undef s64
#undef u32
#undef vu32
#undef vs32
#undef s32
#undef u16
#undef s16
#undef u8
#undef s8

#include <ultra64.h>

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#include "controller_api.h"
#include "cheats.h"
#include "debug_log.h"
#include "menu/file_select.h"

#include "../configfile.h"

static int button_mapping[14][2];

static void set_button_mapping(int index, int mask_n64, int mask_3ds)
{
    button_mapping[index][0] = mask_3ds;
    button_mapping[index][1] = mask_n64;
}

static u32 controller_3ds_get_held(void)
{
    u32 res = 0;
    hidScanInput();
    u32 kDown = keysHeld();

    // Moon jump cheat: read the raw 3DS A button directly, independently of
    // the remappable N64 A mapping.
    gCheatMoonJumpHeld = (kDown & KEY_A) != 0;

    for (size_t i = 0; i < sizeof(button_mapping) / sizeof(button_mapping[0]); i++)
    {
        if (button_mapping[i][0] & kDown) {
            res |= button_mapping[i][1];
        }
    }

    return res;
}

static void controller_3ds_init(void)
{
    u32 i = 0;
    DBG_LOG("3ds controller init: key_a=0x%x key_b=0x%x key_start=0x%x key_z=0x%x",
            configKeyA, configKeyB, configKeyStart, configKeyZ);
    set_button_mapping(i++, A_BUTTON,     configKeyA); // n64 button => configured button
    set_button_mapping(i++, B_BUTTON,     configKeyB);
    set_button_mapping(i++, START_BUTTON, configKeyStart);
    set_button_mapping(i++, L_TRIG,       configKeyL);
    set_button_mapping(i++, R_TRIG,       configKeyR);
    set_button_mapping(i++, Z_TRIG,       configKeyZ);
    set_button_mapping(i++, U_CBUTTONS,   configKeyCUp);
    set_button_mapping(i++, D_CBUTTONS,   configKeyCDown);
    set_button_mapping(i++, L_CBUTTONS,   configKeyCLeft);
    set_button_mapping(i++, R_CBUTTONS,   configKeyCRight);
    set_button_mapping(i++, U_JPAD,       configKeyDUp);
    set_button_mapping(i++, D_JPAD,       configKeyDDown);
    set_button_mapping(i++, L_JPAD,       configKeyDLeft);
    set_button_mapping(i++, R_JPAD,       configKeyDRight);
}

static void controller_3ds_read(OSContPad *pad)
{
    pad->button = controller_3ds_get_held();

    circlePosition pos;
    hidCircleRead(&pos);
    pad->stick_x = pos.dx / 2;
    pad->stick_y = pos.dy / 2;

    // In the file-select menus the D-pad nudges the cursor precisely.
    // (24 raw units -> 3 pixels per frame in handle_controller_cursor_input.)
    // Uses the raw HID bits because the D-pad keys are unmapped by default.
    if (gFileSelectCursorActive) {
        u32 dpad = keysHeld();

        if (dpad & KEY_DRIGHT) {
            pad->stick_x = 24;
        } else if (dpad & KEY_DLEFT) {
            pad->stick_x = -24;
        }
        if (dpad & KEY_DUP) {
            pad->stick_y = 24;
        } else if (dpad & KEY_DDOWN) {
            pad->stick_y = -24;
        }
    }
}

struct ControllerAPI controller_3ds = {
    controller_3ds_init,
    controller_3ds_read
};

#endif
