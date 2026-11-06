#ifdef TARGET_N3DS

#include <string.h>

#include "gfx_3ds.h"
#include "gfx_3ds_common.h"
#include "gfx_3ds_menu.h"
#include "gfx_3ds_minimap.h"
#include "gfx_citro3d.h"

#include "cheats.h"
#include "menu/file_select.h"

struct gfx_configuration gfx_config = {false, false}; // AA off, 800px off

bottom_tab gBottomScreenTab = BOTTOM_TAB_MAP;

int gTouchpadX = -1;
int gTouchpadY = -1;
bool gTouchpadHeld = false;

static C3D_Mtx modelView, projection;
static int buffer_offset;

static C3D_Tex mode_400_tex, mode_800_tex;
static C3D_Tex aa_off_tex, aa_on_tex;
static C3D_Tex resume_tex, exit_tex;

static C3D_Tex tab_tex[3];
static C3D_Tex lbl_inf_health_tex, lbl_inf_air_tex, lbl_inf_lives_tex, lbl_no_damage_tex;
static C3D_Tex lbl_moon_jump_tex, lbl_enemies_ignore_tex;
static C3D_Tex lbl_power_none_tex, lbl_power_wing_tex, lbl_power_metal_tex, lbl_power_vanish_tex;
static C3D_Tex lbl_on_tex, lbl_off_tex;

// Colors (0xRRGGBB)
#define COLOR_BACKGROUND   0x0B1118
#define COLOR_TAB_ACTIVE   0x2E7D32
#define COLOR_TAB_INACTIVE 0x2C3440
#define COLOR_ROW_ON       0x2E7D32
#define COLOR_ROW_OFF      0x232B36

// Layout (touch coordinates: origin top-left, y grows downward)
#define TAB_COUNT 3
static const float sTabX[TAB_COUNT] = { 2.0f, 108.0f, 214.0f };
static const float sTabY = 2.0f;
static const float sTabW = 102.0f;
static const float sTabH = 26.0f;

#define CHEAT_ROW_COUNT 7
static const float sCheatRowX = 8.0f;
static const float sCheatRowY = 30.0f;
static const float sCheatRowW = 304.0f;
static const float sCheatRowH = 26.0f;
static const float sCheatRowPitch = 28.0f;

static const float sSetupColX[2] = { 76.0f, 180.0f };
static const float sSetupRowY[2] = { 50.0f, 150.0f };
static const float sSetupButtonSize = 64.0f;

static void gfx_3ds_menu_set_ortho(void)
{
    Mtx_Identity(&modelView);
    Mtx_OrthoTilt(&projection, 0.0, 320.0, 0.0, 240.0, 0.0, 1.0, true);

    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_modelView, &modelView);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &projection);
}

static void gfx_3ds_menu_draw_rect(float *vbo_buffer, float x, float y, float width, float height, u32 color)
{
    // x/y are the top-left corner; convert to the y-up projection space.
    float bottom = 240.0f - (y + height);
    float top = 240.0f - y;

    vertex verts[] =
    {
        { { x,         bottom, 0.5f, 1.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
        { { x + width, top,    0.5f, 1.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
        { { x + width, bottom, 0.5f, 1.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },

        { { x,         bottom, 0.5f, 1.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
        { { x,         top,    0.5f, 1.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
        { { x + width, top,    0.5f, 1.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } }
    };

    memcpy(vbo_buffer + buffer_offset * VERTEX_SHADER_SIZE, verts, sizeof(verts));

    C3D_TexEnv* env = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env);
    C3D_TexEnvColor(env, color);
    C3D_TexEnvSrc(env, C3D_Both, GPU_CONSTANT, 0, 0);
    C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);

    C3D_DrawArrays(GPU_TRIANGLES, buffer_offset, 6);
    buffer_offset += 6;
}

static void gfx_3ds_menu_draw_texture(float *vbo_buffer, C3D_Tex *texture, float x, float y, float width, float height)
{
    float bottom = 240.0f - (y + height);
    float top = 240.0f - y;

    vertex verts[] =
    {
        { { x,         bottom, 0.5f, 1.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
        { { x + width, top,    0.5f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
        { { x + width, bottom, 0.5f, 1.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },

        { { x,         bottom, 0.5f, 1.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
        { { x,         top,    0.5f, 1.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
        { { x + width, top,    0.5f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } }
    };

    memcpy(vbo_buffer + buffer_offset * VERTEX_SHADER_SIZE, verts, sizeof(verts));

    C3D_TexBind(0, texture);
    C3D_TexFlush(texture);

    C3D_TexEnv* env = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env);
    C3D_TexEnvColor(env, 0);
    C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, 0, 0);
    C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);

    C3D_DrawArrays(GPU_TRIANGLES, buffer_offset, 6);
    buffer_offset += 6;
}

static C3D_Tex *gfx_3ds_menu_power_label(void)
{
    switch (gCheats.powerUp)
    {
        case CHEAT_POWER_WING:
            return &lbl_power_wing_tex;
        case CHEAT_POWER_METAL:
            return &lbl_power_metal_tex;
        case CHEAT_POWER_VANISH:
            return &lbl_power_vanish_tex;
        default:
            return &lbl_power_none_tex;
    }
}

static void gfx_3ds_menu_draw_cheats(float *vbo_buffer)
{
    int i;

    for (i = 0; i < CHEAT_ROW_COUNT; i++)
    {
        bool on;
        C3D_Tex *label;

        switch (i)
        {
            case 0:
                on = gCheats.infiniteHealth;
                label = &lbl_inf_health_tex;
                break;
            case 1:
                on = gCheats.infiniteAir;
                label = &lbl_inf_air_tex;
                break;
            case 2:
                on = gCheats.infiniteLives;
                label = &lbl_inf_lives_tex;
                break;
            case 3:
                on = gCheats.noDamage;
                label = &lbl_no_damage_tex;
                break;
            case 4:
                on = gCheats.moonJump;
                label = &lbl_moon_jump_tex;
                break;
            case 5:
                on = gCheats.enemiesIgnore;
                label = &lbl_enemies_ignore_tex;
                break;
            default:
                on = gCheats.powerUp != CHEAT_POWER_NONE;
                label = gfx_3ds_menu_power_label();
                break;
        }

        float y = sCheatRowY + (sCheatRowPitch * i);

        gfx_3ds_menu_draw_rect(vbo_buffer, sCheatRowX, y, sCheatRowW, sCheatRowH,
                               gfx_3ds_rgb_to_abgr(on ? COLOR_ROW_ON : COLOR_ROW_OFF));
        gfx_3ds_menu_draw_texture(vbo_buffer, label, sCheatRowX + 8.0f, y + 5.0f, 256.0f, 16.0f);
        gfx_3ds_menu_draw_texture(vbo_buffer, on ? &lbl_on_tex : &lbl_off_tex,
                                  sCheatRowX + 232.0f, y + 5.0f, 64.0f, 16.0f);
    }
}

static void gfx_3ds_menu_draw_setup(float *vbo_buffer)
{
    gfx_3ds_menu_draw_texture(vbo_buffer, gfx_config.useAA ? &aa_on_tex : &aa_off_tex,
                              sSetupColX[0], sSetupRowY[0], sSetupButtonSize, sSetupButtonSize);
    gfx_3ds_menu_draw_texture(vbo_buffer, gfx_config.useWide ? &mode_800_tex : &mode_400_tex,
                              sSetupColX[1], sSetupRowY[0], sSetupButtonSize, sSetupButtonSize);

    // resume = back to the map tab (the bottom screen always shows the UI now)
    gfx_3ds_menu_draw_texture(vbo_buffer, &resume_tex,
                              sSetupColX[0], sSetupRowY[1], sSetupButtonSize, sSetupButtonSize);
    gfx_3ds_menu_draw_texture(vbo_buffer, &exit_tex,
                              sSetupColX[1], sSetupRowY[1], sSetupButtonSize, sSetupButtonSize);
}

static void gfx_3ds_menu_draw_tab_bar(float *vbo_buffer)
{
    int i;

    // The minimap changes the model-view and projection uniforms, so restore
    // the screen-space orthographic setup before drawing the tab bar.
    gfx_3ds_menu_set_ortho();

    for (i = 0; i < TAB_COUNT; i++)
    {
        bool active = ((int) gBottomScreenTab == i);

        gfx_3ds_menu_draw_rect(vbo_buffer, sTabX[i], sTabY, sTabW, sTabH,
                               gfx_3ds_rgb_to_abgr(active ? COLOR_TAB_ACTIVE : COLOR_TAB_INACTIVE));
        gfx_3ds_menu_draw_texture(vbo_buffer, &tab_tex[i], sTabX[i], sTabY, sTabW, sTabH);
    }
}

static bool is_inside_box(int pos_x, int pos_y, int x, int y, int width, int height)
{
    return pos_x >= x && pos_x < (x + width) && pos_y >= y && pos_y < (y + height);
}

menu_action gfx_3ds_menu_on_touch(int touch_x, int touch_y)
{
    int i;

    // tab bar
    for (i = 0; i < TAB_COUNT; i++)
    {
        if (is_inside_box(touch_x, touch_y, (int) sTabX[i], (int) sTabY, (int) sTabW, (int) sTabH))
        {
            if ((int) gBottomScreenTab != i)
            {
                gBottomScreenTab = (bottom_tab) i;
                return TAB_CHANGED;
            }
            return DO_NOTHING;
        }
    }

    switch (gBottomScreenTab)
    {
        case BOTTOM_TAB_CHEATS:
        {
            for (i = 0; i < CHEAT_ROW_COUNT; i++)
            {
                int row_y = (int) (sCheatRowY + (sCheatRowPitch * i));

                if (is_inside_box(touch_x, touch_y, (int) sCheatRowX, row_y, (int) sCheatRowW, (int) sCheatRowH))
                {
                    switch (i)
                    {
                        case 0:
                            gCheats.infiniteHealth = !gCheats.infiniteHealth;
                            break;
                        case 1:
                            gCheats.infiniteAir = !gCheats.infiniteAir;
                            break;
                        case 2:
                            gCheats.infiniteLives = !gCheats.infiniteLives;
                            break;
                        case 3:
                            gCheats.noDamage = !gCheats.noDamage;
                            break;
                        case 4:
                            gCheats.moonJump = !gCheats.moonJump;
                            break;
                        case 5:
                            gCheats.enemiesIgnore = !gCheats.enemiesIgnore;
                            break;
                        default:
                            gCheats.powerUp++;
                            if (gCheats.powerUp > CHEAT_POWER_VANISH)
                                gCheats.powerUp = CHEAT_POWER_NONE;
                            break;
                    }

                    return TAB_CHANGED;
                }
            }
            break;
        }

        case BOTTOM_TAB_SETUP:
        {
            // toggle anti-aliasing (not available in 400px mode or 3D mode)
            if (!gGfx3DEnabled
                && is_inside_box(touch_x, touch_y, (int) sSetupColX[0], (int) sSetupRowY[0],
                                 (int) sSetupButtonSize, (int) sSetupButtonSize))
            {
                if (gfx_config.useWide)
                {
                    gfx_config.useAA = !gfx_config.useAA;
                    return CONFIG_CHANGED;
                }
                return DO_NOTHING;
            }

            // 400px vs 800px
            if (!gGfx3DEnabled
                && is_inside_box(touch_x, touch_y, (int) sSetupColX[1], (int) sSetupRowY[0],
                                 (int) sSetupButtonSize, (int) sSetupButtonSize))
            {
                gfx_config.useWide = !gfx_config.useWide;

                // disable AA if 400px
                if (!gfx_config.useWide && gfx_config.useAA)
                    gfx_config.useAA = false;

                return CONFIG_CHANGED;
            }

            // back to the map
            if (is_inside_box(touch_x, touch_y, (int) sSetupColX[0], (int) sSetupRowY[1],
                              (int) sSetupButtonSize, (int) sSetupButtonSize))
            {
                gBottomScreenTab = BOTTOM_TAB_MAP;
                return TAB_CHANGED;
            }

            // exit to loader
            if (is_inside_box(touch_x, touch_y, (int) sSetupColX[1], (int) sSetupRowY[1],
                              (int) sSetupButtonSize, (int) sSetupButtonSize))
            {
                gShouldRun = false;
            }
            break;
        }

        default:
            break;
    }

    return DO_NOTHING;
}

void gfx_3ds_menu_init()
{
    // load all the textures
    load_t3x_texture(&mode_400_tex, NULL, mode_400_t3x, mode_400_t3x_size);
    C3D_TexSetFilter(&mode_400_tex, GPU_LINEAR, GPU_NEAREST);

    load_t3x_texture(&mode_800_tex, NULL, mode_800_t3x, mode_800_t3x_size);
    C3D_TexSetFilter(&mode_800_tex, GPU_LINEAR, GPU_NEAREST);

    load_t3x_texture(&aa_on_tex, NULL, aa_on_t3x, aa_on_t3x_size);
    C3D_TexSetFilter(&aa_on_tex, GPU_LINEAR, GPU_NEAREST);

    load_t3x_texture(&aa_off_tex, NULL, aa_off_t3x, aa_off_t3x_size);
    C3D_TexSetFilter(&aa_off_tex, GPU_LINEAR, GPU_NEAREST);

    load_t3x_texture(&resume_tex, NULL, resume_t3x, resume_t3x_size);
    C3D_TexSetFilter(&resume_tex, GPU_LINEAR, GPU_NEAREST);

    load_t3x_texture(&exit_tex, NULL, exit_t3x, exit_t3x_size);
    C3D_TexSetFilter(&exit_tex, GPU_LINEAR, GPU_NEAREST);

    load_t3x_texture(&tab_tex[0], NULL, tab_map_t3x, tab_map_t3x_size);
    C3D_TexSetFilter(&tab_tex[0], GPU_LINEAR, GPU_NEAREST);
    load_t3x_texture(&tab_tex[1], NULL, tab_cheats_t3x, tab_cheats_t3x_size);
    C3D_TexSetFilter(&tab_tex[1], GPU_LINEAR, GPU_NEAREST);
    load_t3x_texture(&tab_tex[2], NULL, tab_setup_t3x, tab_setup_t3x_size);
    C3D_TexSetFilter(&tab_tex[2], GPU_LINEAR, GPU_NEAREST);

    load_t3x_texture(&lbl_inf_health_tex, NULL, lbl_inf_health_t3x, lbl_inf_health_t3x_size);
    C3D_TexSetFilter(&lbl_inf_health_tex, GPU_LINEAR, GPU_NEAREST);
    load_t3x_texture(&lbl_inf_air_tex, NULL, lbl_inf_air_t3x, lbl_inf_air_t3x_size);
    C3D_TexSetFilter(&lbl_inf_air_tex, GPU_LINEAR, GPU_NEAREST);
    load_t3x_texture(&lbl_inf_lives_tex, NULL, lbl_inf_lives_t3x, lbl_inf_lives_t3x_size);
    C3D_TexSetFilter(&lbl_inf_lives_tex, GPU_LINEAR, GPU_NEAREST);
    load_t3x_texture(&lbl_no_damage_tex, NULL, lbl_no_damage_t3x, lbl_no_damage_t3x_size);
    C3D_TexSetFilter(&lbl_no_damage_tex, GPU_LINEAR, GPU_NEAREST);
    load_t3x_texture(&lbl_moon_jump_tex, NULL, lbl_moon_jump_t3x, lbl_moon_jump_t3x_size);
    C3D_TexSetFilter(&lbl_moon_jump_tex, GPU_LINEAR, GPU_NEAREST);
    load_t3x_texture(&lbl_enemies_ignore_tex, NULL, lbl_enemies_ignore_t3x, lbl_enemies_ignore_t3x_size);
    C3D_TexSetFilter(&lbl_enemies_ignore_tex, GPU_LINEAR, GPU_NEAREST);
    load_t3x_texture(&lbl_power_none_tex, NULL, lbl_power_none_t3x, lbl_power_none_t3x_size);
    C3D_TexSetFilter(&lbl_power_none_tex, GPU_LINEAR, GPU_NEAREST);
    load_t3x_texture(&lbl_power_wing_tex, NULL, lbl_power_wing_t3x, lbl_power_wing_t3x_size);
    C3D_TexSetFilter(&lbl_power_wing_tex, GPU_LINEAR, GPU_NEAREST);
    load_t3x_texture(&lbl_power_metal_tex, NULL, lbl_power_metal_t3x, lbl_power_metal_t3x_size);
    C3D_TexSetFilter(&lbl_power_metal_tex, GPU_LINEAR, GPU_NEAREST);
    load_t3x_texture(&lbl_power_vanish_tex, NULL, lbl_power_vanish_t3x, lbl_power_vanish_t3x_size);
    C3D_TexSetFilter(&lbl_power_vanish_tex, GPU_LINEAR, GPU_NEAREST);
    load_t3x_texture(&lbl_on_tex, NULL, lbl_on_t3x, lbl_on_t3x_size);
    C3D_TexSetFilter(&lbl_on_tex, GPU_LINEAR, GPU_NEAREST);
    load_t3x_texture(&lbl_off_tex, NULL, lbl_off_t3x, lbl_off_t3x_size);
    C3D_TexSetFilter(&lbl_off_tex, GPU_LINEAR, GPU_NEAREST);

    gfx_3ds_minimap_init();
}

// Simple touchpad shown while the file-select cursor is on screen. Dragging
// aims the menu cursor on the top screen; lifting the finger clicks it.
static void gfx_3ds_menu_draw_touchpad(float *vbo_buffer)
{
    u32 border = gfx_3ds_rgb_to_abgr(COLOR_TAB_ACTIVE);

    gfx_3ds_menu_draw_rect(vbo_buffer, 0.0f, 0.0f, 320.0f, 3.0f, border);
    gfx_3ds_menu_draw_rect(vbo_buffer, 0.0f, 237.0f, 320.0f, 3.0f, border);
    gfx_3ds_menu_draw_rect(vbo_buffer, 0.0f, 0.0f, 3.0f, 240.0f, border);
    gfx_3ds_menu_draw_rect(vbo_buffer, 317.0f, 0.0f, 3.0f, 240.0f, border);

    if (gTouchpadX >= 0)
    {
        u32 color = gfx_3ds_rgb_to_abgr(gTouchpadHeld ? 0xE6F0FA : 0x5A6B7D);
        float cx = (float) gTouchpadX;
        float cy = (float) gTouchpadY;

        // crosshair at the last touch position (where a click will land)
        gfx_3ds_menu_draw_rect(vbo_buffer, cx - 12.0f, cy - 1.0f, 24.0f, 2.0f, color);
        gfx_3ds_menu_draw_rect(vbo_buffer, cx - 1.0f, cy - 12.0f, 2.0f, 24.0f, color);
        gfx_3ds_menu_draw_rect(vbo_buffer, cx - 3.0f, cy - 3.0f, 6.0f, 6.0f, color);
    }
}

int gfx_3ds_bottom_screen_draw(float *vertex_buffer, int vertex_offset)
{
    C3D_FrameDrawOn(gTargetBottom);

    C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_ALL);
    C3D_AlphaTest(true, GPU_GREATER, 0x00);
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA,
                   GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);

    gfx_3ds_menu_set_ortho();

    buffer_offset = vertex_offset;

    // the bottom screen is always drawn, so paint over the previous frame
    gfx_3ds_menu_draw_rect(vertex_buffer, 0.0f, 0.0f, 320.0f, 240.0f,
                           gfx_3ds_rgb_to_abgr(COLOR_BACKGROUND));

    // While the file-select cursor is active, the bottom screen is a touchpad
    // for it instead of the MAP/CHEATS/SETUP interface.
    if (gFileSelectCursorActive)
    {
        gfx_3ds_menu_draw_touchpad(vertex_buffer);
        return buffer_offset;
    }

    switch (gBottomScreenTab)
    {
        case BOTTOM_TAB_MAP:
            buffer_offset = gfx_3ds_minimap_draw(vertex_buffer, buffer_offset);
            break;
        case BOTTOM_TAB_CHEATS:
            gfx_3ds_menu_draw_cheats(vertex_buffer);
            break;
        case BOTTOM_TAB_SETUP:
            gfx_3ds_menu_draw_setup(vertex_buffer);
            break;
    }

    gfx_3ds_menu_draw_tab_bar(vertex_buffer);

    return buffer_offset;
}

#endif
