#ifdef TARGET_N3DS

// First writable byte of the core game state. This object is linked before
// every core game object; savestates copy from here up to the matching end
// markers in savestate_bounds_end.c. See src/pc/savestate_3ds.c.
//
// Keep the sections writable: they deliberately live in .data/.bss so the
// linker can give us the exact bounds of the game's writable globals without
// relying on N64 segment symbols (the 3DS build does not use sm64.ld).
__attribute__((used)) unsigned char gSavestateDataStart[1] = { 0xA5 };
__attribute__((used)) unsigned char gSavestateBssStart[1];

#endif
