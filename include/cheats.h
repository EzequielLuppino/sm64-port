#ifndef CHEATS_H
#define CHEATS_H

#include "types.h"

struct MarioState;

typedef enum {
    CHEAT_POWER_NONE = 0,
    CHEAT_POWER_WING,
    CHEAT_POWER_METAL,
    CHEAT_POWER_VANISH
} CheatPowerUp;

typedef struct {
    u8 infiniteHealth;
    u8 infiniteAir;
    u8 infiniteLives;
    u8 noDamage;
    u8 moonJump;
    u8 enemiesIgnore;
    s16 powerUp; // CheatPowerUp
} Cheats;

extern Cheats gCheats;
extern u8 gCheatMoonJumpHeld;

void cheats_pre_mario_update(struct MarioState *m);
void cheats_post_mario_update(struct MarioState *m);

#endif
