#ifdef TARGET_N3DS

// Must be included before the game headers: the 3DS SDK redefines the
// fixed-width type names used by the game (see audio_3ds_threading.h).
#include "audio/audio_3ds_threading.h"

#include <math.h>

#include "camera_3ds.h"

#include "game/camera.h"
#include "engine/math_util.h"
#include "types.h"

// The C-Stick is reported as a circlePosition with the same -156..156 range as
// the circle pad. The nub is small and stiff, so a generous radial deadzone
// keeps it from drifting the camera while untouched.
#define CSTICK_MAX_VALUE   156.0f
#define CSTICK_DEADZONE     20.0f

// Rotation per 30 Hz game frame at full deflection:
//   0x400 = 5.6 degrees -> ~170 deg/s yaw
//   0x200 = 2.8 degrees -> ~85 deg/s pitch
#define CSTICK_YAW_RATE    0x400
#define CSTICK_PITCH_RATE  0x200

// Vertical range the vanilla behind-Mario camera allows (~67.5 degrees).
#define CSTICK_PITCH_LIMIT 0x3000

// Radial cameras clamp their offset to +-105 degrees (the widest vanilla
// radial rotation) to stay inside the level's designed framing.
#define CSTICK_RADIAL_YAW_LIMIT DEGREES(105)

// Sticky: once the player aims the camera by hand, the vanilla idle
// auto-recenter stops fighting them for the rest of the session.
static bool sManualCameraActive;

// Reads the C-Stick, applies the radial deadzone and returns a unit direction
// scaled by 0..1. Returns false while the stick is centered.
static bool cstick_read(f32 *outX, f32 *outY)
{
    circlePosition stick;
    f32 x;
    f32 y;
    f32 rawMag;
    f32 mag;
    f32 amount;

    // Reads the value cached by irrstScanInput() in controller_3ds_read().
    hidCstickRead(&stick);
    x = (f32) stick.dx;
    y = (f32) stick.dy;

    rawMag = sqrtf(x * x + y * y);
    if (rawMag <= CSTICK_DEADZONE)
        return false;

    sManualCameraActive = true;

    mag = rawMag;
    if (mag > CSTICK_MAX_VALUE)
        mag = CSTICK_MAX_VALUE;
    amount = (mag - CSTICK_DEADZONE) / (CSTICK_MAX_VALUE - CSTICK_DEADZONE);

    *outX = x / rawMag * amount;
    *outY = y / rawMag * amount;
    return true;
}

void camera_3ds_update_cstick(struct Camera *c)
{
    f32 x;
    f32 y;
    s16 yaw;
    s16 pitch;
    f32 dist;

    if (c->cutscene != 0)
        return;

    // Only the normal follow cameras are free. Scripted cameras (fixed,
    // 8-direction, cannon, spiral stairs, outward-radial, ...) keep full
    // control of their framing. Radial gets its own yaw hook below.
    switch (c->mode)
    {
        case CAMERA_MODE_NONE:
        case CAMERA_MODE_BEHIND_MARIO:
        case CAMERA_MODE_CLOSE:
        case CAMERA_MODE_WATER_SURFACE:
        case CAMERA_MODE_FREE_ROAM:
            break;
        default:
            return;
    }

    if (!cstick_read(&x, &y))
        return;

    // Decompose the camera's current orbit around the focus, apply the stick,
    // and rebuild the position at the same distance.
    vec3f_get_dist_and_angle(c->focus, c->pos, &dist, &pitch, &yaw);

    // Right on the stick pans the view right; up on the stick looks up (the
    // camera itself moves down), matching modern non-inverted controls.
    yaw += (s16) (x * CSTICK_YAW_RATE);
    pitch -= (s16) (y * CSTICK_PITCH_RATE);

    if (pitch > CSTICK_PITCH_LIMIT)
        pitch = CSTICK_PITCH_LIMIT;
    if (pitch < -CSTICK_PITCH_LIMIT)
        pitch = -CSTICK_PITCH_LIMIT;

    vec3f_set_dist_and_angle(c->focus, c->pos, dist, pitch, yaw);

    c->nextYaw = calculate_yaw(c->focus, c->pos);
    c->yaw = c->nextYaw;
}

void camera_3ds_update_radial(s16 *yawOffset)
{
    f32 x;
    f32 y;
    s16 value;

    // Vertical framing in radial mode follows the terrain, so only yaw is
    // consumed here.
    if (!cstick_read(&x, &y))
        return;
    (void) y;

    value = *yawOffset + (s16) (x * CSTICK_YAW_RATE);
    if (value > CSTICK_RADIAL_YAW_LIMIT)
        value = CSTICK_RADIAL_YAW_LIMIT;
    if (value < -CSTICK_RADIAL_YAW_LIMIT)
        value = -CSTICK_RADIAL_YAW_LIMIT;
    *yawOffset = value;
}

bool camera_3ds_manual_camera_active(void)
{
    return sManualCameraActive;
}

#endif
