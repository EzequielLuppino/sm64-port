#ifdef TARGET_N3DS

// End of the writable core game state. This object is linked after every
// core game object but before src/pc (3DS front-end, renderer, audio backend)
// and the libraries, so the markers only delimit game-owned globals.
// See src/pc/savestate_3ds.c.
__attribute__((used)) unsigned char gSavestateDataEnd[1] = { 0x5A };
__attribute__((used)) unsigned char gSavestateBssEnd[1];

#endif
