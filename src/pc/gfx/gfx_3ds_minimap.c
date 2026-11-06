#ifdef TARGET_N3DS

#include <math.h>
#include <string.h>

#include "gfx_3ds.h"
#include "gfx_3ds_common.h"
#include "gfx_3ds_minimap.h"

#include "src/minimap/minimap.h"
#include "src/minimap/textures/mario_t3x.h"
#include "src/minimap/textures/arrow_t3x.h"

static C3D_Mtx modelView, projBottom;

static uint8_t *current_texture;
static size_t current_texture_size;

static uint32_t minimap_color;      // flood-fill background color
static C3D_Tex minimap_mario_tex;   // texture for mario location
static C3D_Tex minimap_arrow_tex;   // texture for heading arrow
static C3D_Tex minimap_tex;         // texture for map background
static bool minimap_tex_loaded = false;
static bool show_minimap = false;

static float mario_x, mario_y, mario_direction = 0.0f;
// 320 - 240 = 80, so add 40 to shift map into middle of the screen
static float x_offset = 40.0f;
static float y_offset = 0.0f;

static u32 buffer_offset = 0;

static const float mario_sprite_size = 16.0f / 2.0f; // 16px but 8px left/right of 0
static const float arrow_sprite_size = 8.0f / 2.0f;  // 8px but 4px left/right of 0

static const vertex vertex_list_mario[] =
{
    { { -mario_sprite_size, -mario_sprite_size, 0.5f, 1.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { {  mario_sprite_size,  mario_sprite_size, 0.5f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { {  mario_sprite_size, -mario_sprite_size, 0.5f, 1.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },

    { { -mario_sprite_size, -mario_sprite_size, 0.5f, 1.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { { -mario_sprite_size,  mario_sprite_size, 0.5f, 1.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { {  mario_sprite_size,  mario_sprite_size, 0.5f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } }
};

static const vertex vertex_list_arrow[] =
{
    { { -arrow_sprite_size, -arrow_sprite_size, 0.5f, 1.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { {  arrow_sprite_size,  arrow_sprite_size, 0.5f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { {  arrow_sprite_size, -arrow_sprite_size, 0.5f, 1.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },

    { { -arrow_sprite_size, -arrow_sprite_size, 0.5f, 1.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { { -arrow_sprite_size,  arrow_sprite_size, 0.5f, 1.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { {  arrow_sprite_size,  arrow_sprite_size, 0.5f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } }
};

static void minimap_load_new_minimap_texture(void)
{
    if (minimap_tex_loaded)
    {
        C3D_TexDelete(&minimap_tex);
        minimap_tex_loaded = false;
    }

    minimap_get_current_texture(&current_texture, &current_texture_size, &minimap_color);
    minimap_tex_loaded = load_t3x_texture(&minimap_tex, NULL, current_texture, current_texture_size);
    if (minimap_tex_loaded)
        C3D_TexSetFilter(&minimap_tex, GPU_LINEAR, GPU_NEAREST);
}

static void minimap_unload_current_minimap_texture(void)
{
    if (minimap_tex_loaded)
    {
        C3D_TexDelete(&minimap_tex);
        minimap_tex_loaded = false;
    }
}

static void gfx_3ds_minimap_draw_background_color_rgb(float *vbo_buffer, uint32_t color)
{
    Mtx_Identity(&modelView);
    Mtx_OrthoTilt(&projBottom, 0.0, 320.0, 0.0, 240.0, 0.0, 1.0, true);

    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_modelView, &modelView);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &projBottom);

    memcpy(vbo_buffer + buffer_offset * VERTEX_SHADER_SIZE,
           vertex_list_color,
           sizeof(vertex_list_color));

    C3D_TexEnv* env = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env);
    C3D_TexEnvColor(env, gfx_3ds_rgb_to_abgr(color));
    C3D_TexEnvSrc(env, C3D_Both, GPU_CONSTANT, 0, 0);
    C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);

    C3D_DrawArrays(GPU_TRIANGLES, buffer_offset, 6); // 2 triangles
    buffer_offset += 6;
}

static void gfx_3ds_minimap_draw_background_color(float *vbo_buffer)
{
    gfx_3ds_minimap_draw_background_color_rgb(vbo_buffer, minimap_color);
}

static void gfx_3ds_minimap_draw_background(float *vbo_buffer)
{
    Mtx_Identity(&modelView);
    Mtx_Translate(&modelView, x_offset, y_offset, 0.0f, false);

    Mtx_OrthoTilt(&projBottom, 0.0, 320.0, 0.0, 240.0, 0.0, 1.0, true);

    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_modelView, &modelView);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &projBottom);

    memcpy(vbo_buffer + buffer_offset * VERTEX_SHADER_SIZE,
           vertex_list_background,
           sizeof(vertex_list_background));

    C3D_TexBind(0, &minimap_tex);
    C3D_TexFlush(&minimap_tex);

    C3D_TexEnv* env = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env);
    C3D_TexEnvColor(env, 0);
    C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, 0, 0);
    C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);

    C3D_DrawArrays(GPU_TRIANGLES, buffer_offset, 6); // 2 triangles
    buffer_offset += 6;
}

static void gfx_3ds_minimap_draw_mario(float *vbo_buffer)
{
    memcpy(vbo_buffer + buffer_offset * VERTEX_SHADER_SIZE,
           vertex_list_mario,
           sizeof(vertex_list_mario));

    Mtx_Identity(&modelView);
    // subtract y from 240 to flip y-axis
    Mtx_Translate(&modelView, mario_x + x_offset, 240.0f - mario_y + y_offset, 0.0f, false);

    Mtx_OrthoTilt(&projBottom, 0.0, 320.0, 0.0, 240.0, 0.0, 1.0, true);

    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_modelView, &modelView);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &projBottom);

    C3D_TexBind(0, &minimap_mario_tex);
    C3D_TexFlush(&minimap_mario_tex);

    C3D_TexEnv* env = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env);
    C3D_TexEnvColor(env, 0);
    C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
    C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, 0, 0);

    C3D_DrawArrays(GPU_TRIANGLES, buffer_offset, 6); // 2 triangles
    buffer_offset += 6;
}

static void gfx_3ds_minimap_draw_heading(float *vbo_buffer)
{
    memcpy(vbo_buffer + buffer_offset * VERTEX_SHADER_SIZE,
           vertex_list_arrow,
           sizeof(vertex_list_arrow));

    // mario_direction is in turns; convert to radians
    float angle = M_TAU * mario_direction;

    Mtx_Identity(&modelView);
    Mtx_RotateZ(&modelView, angle, false);

    float arrow_x_offset = 16.0f * sinf(angle);
    float arrow_y_offset = -16.0f * cosf(angle);

    Mtx_Translate(&modelView, mario_x + x_offset + arrow_x_offset,
                  240.0f - mario_y + y_offset + arrow_y_offset, 0.0f, false);

    Mtx_OrthoTilt(&projBottom, 0.0, 320.0, 0.0, 240.0, 0.0, 1.0, true);

    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_modelView, &modelView);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &projBottom);

    C3D_TexBind(0, &minimap_arrow_tex);
    C3D_TexFlush(&minimap_arrow_tex);

    C3D_TexEnv* env = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env);
    C3D_TexEnvColor(env, 0);
    C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
    C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, 0, 0);

    C3D_DrawArrays(GPU_TRIANGLES, buffer_offset, 6); // 2 triangles
    buffer_offset += 6;
}

void gfx_3ds_minimap_init(void)
{
    load_t3x_texture(&minimap_mario_tex, NULL, mario_t3x, mario_t3x_size);
    C3D_TexSetFilter(&minimap_mario_tex, GPU_LINEAR, GPU_NEAREST);

    load_t3x_texture(&minimap_arrow_tex, NULL, arrow_t3x, arrow_t3x_size);
    C3D_TexSetFilter(&minimap_arrow_tex, GPU_LINEAR, GPU_NEAREST);
}

int gfx_3ds_minimap_draw(float *vertex_buffer, int vertex_offset)
{
    buffer_offset = vertex_offset;

    if (minimap_has_level_or_area_changed())
    {
        show_minimap = minimap_load_level_and_area();
        if (show_minimap)
            minimap_load_new_minimap_texture();
        else
            minimap_unload_current_minimap_texture();
    }

    if (!show_minimap || !minimap_tex_loaded)
    {
        gfx_3ds_minimap_draw_background_color_rgb(vertex_buffer, 0x000000);
        return buffer_offset;
    }

    gfx_3ds_minimap_draw_background_color(vertex_buffer);
    gfx_3ds_minimap_draw_background(vertex_buffer);

    if (minimap_get_mario_position(&mario_x, &mario_y, &mario_direction))
    {
        gfx_3ds_minimap_draw_mario(vertex_buffer);
        gfx_3ds_minimap_draw_heading(vertex_buffer);
    }

    return buffer_offset;
}

#endif
