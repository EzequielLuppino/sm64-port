#include "cheats.h"

#include "sm64.h"
#include "game/mario.h"

Cheats gCheats = { 0, 0, 0, 0, 0, 0, CHEAT_POWER_NONE };
u8 gCheatMoonJumpHeld = 0;

static s16 sPrevHealth = 0;
static u32 sForcedCapFlags = 0;

void cheats_pre_mario_update(struct MarioState *m) {
    sPrevHealth = m->health;
}

void cheats_post_mario_update(struct MarioState *m) {
    u32 desiredCapFlags = 0;
    u32 staleCapFlags;

    // No damage: undo anything that still slipped through the interaction
    // handlers (poison gas, wall hits, quicksand, ...).
    if (gCheats.noDamage) {
        if (m->health < sPrevHealth) {
            m->health = sPrevHealth;
            m->hurtCounter = 0;
        }
    }

    if (gCheats.infiniteHealth) {
        m->health = 0x880;
        m->hurtCounter = 0;
    }

    if (gCheats.infiniteLives) {
        m->numLives = 99;
    }

    switch (gCheats.powerUp) {
        case CHEAT_POWER_WING:
            desiredCapFlags = MARIO_WING_CAP;
            break;
        case CHEAT_POWER_METAL:
            desiredCapFlags = MARIO_METAL_CAP;
            break;
        case CHEAT_POWER_VANISH:
            desiredCapFlags = MARIO_VANISH_CAP;
            break;
        default:
            break;
    }

    if (desiredCapFlags != 0) {
        m->flags |= desiredCapFlags | MARIO_CAP_ON_HEAD;

        // Keep the cap timer away from zero so the game never clears the
        // flags we forced and never reaches the flicker threshold.
        if (m->capTimer < 0x41) {
            m->capTimer = 0x100;
        }
    }

    // Remove flags that were forced for the previously selected power-up.
    // CAP_ON_HEAD is deliberately left in place so that turning the cheat off
    // restores Mario's normal cap instead of leaving him bald.
    staleCapFlags = sForcedCapFlags & ~desiredCapFlags;
    if (staleCapFlags != 0) {
        m->flags &= ~staleCapFlags;
    }
    sForcedCapFlags = desiredCapFlags;

    // Moon jump: hold ZR to float upwards, including straight off the ground.
    if (gCheats.moonJump && gCheatMoonJumpHeld) {
        u32 group = m->action & ACT_GROUP_MASK;

        if (group == ACT_GROUP_STATIONARY || group == ACT_GROUP_MOVING || group == ACT_GROUP_AIRBORNE) {
            if (m->vel[1] < 70.0f) {
                m->vel[1] += 10.0f;
                if (m->vel[1] > 70.0f) {
                    m->vel[1] = 70.0f;
                }
            }

            // Leave the ground so the upward velocity actually moves Mario.
            if ((group == ACT_GROUP_STATIONARY || group == ACT_GROUP_MOVING) && m->vel[1] > 0.0f) {
                set_mario_action(m, ACT_JUMP, 0);
            }
        }
    }
}
