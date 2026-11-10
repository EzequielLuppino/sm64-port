#ifndef SAVESTATE_3DS_H
#define SAVESTATE_3DS_H

#include <stdbool.h>
#include <stddef.h>

#ifdef TARGET_N3DS

#define SAVESTATE_SLOT_COUNT 3

// Registers the main game memory pool with the savestate code. The pool is a
// function-local array in pc_main.c, so it cannot be referenced by name from
// here.
void savestate_3ds_register_pool(void *start, size_t size);

// Save/load the complete in-memory game state: the core game globals plus the
// main game memory pool. Slots are kept in RAM for instant access and mirrored
// to "sdmc:/sm64state<N>.bin" so they survive a restart. The image contains
// absolute pointers, which is safe because a given binary always loads at the
// same addresses on a 3DS; files that do not exactly match the running build
// (version, section sizes, checksum) are rejected.
bool savestate_3ds_save(int slot);
bool savestate_3ds_load(int slot);
bool savestate_3ds_slot_used(int slot);

#endif

#endif
