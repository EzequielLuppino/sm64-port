#ifndef CAMERA_3DS_H
#define CAMERA_3DS_H

#include <stdbool.h>

#include "types.h"

#ifdef TARGET_N3DS

struct Camera;

// Applies New 3DS C-Stick input to the camera's orbit around the current focus
// point. Called from update_camera() before the active camera mode runs, so the
// mode's floor/wall collision handling sees the rotated position and the
// rotation persists through the mode's own position updates.
void camera_3ds_update_cstick(struct Camera *c);

// Radial cameras (e.g. Bob-omb Battlefield) build their position from
// sModeOffsetYaw instead of the previous camera position, so the analog input
// has to adjust that offset. Called from mode_radial_camera() after the mode
// has made its own adjustments for the frame.
void camera_3ds_update_radial(s16 *yawOffset);

// True once the player has used the C-Stick. Used to suppress the vanilla idle
// auto-recenter so the camera stays where the player put it.
bool camera_3ds_manual_camera_active(void);

#endif

#endif

