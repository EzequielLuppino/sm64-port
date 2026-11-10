#ifdef TARGET_N3DS

// Must be included before the game headers: it includes 3ds.h inside a
// type-redefinition guard. The game headers use typedefs (u8 and friends)
// and the 3DS SDK redeclares those names.
#include "audio/audio_3ds_threading.h"

#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "savestate_3ds.h"
#include "types.h"
#include "debug_log.h"

#include "audio/audio_3ds.h"

// Linker-placed markers. See 3ds/savestate_bounds_start.c and
// 3ds/savestate_bounds_end.c, and the Makefile link-order setup.
extern unsigned char gSavestateDataStart[];
extern unsigned char gSavestateDataEnd[];
extern unsigned char gSavestateBssStart[];
extern unsigned char gSavestateBssEnd[];

// Enough room for the game's writable globals plus the main memory pool.
// Actual sizes are computed from the linker markers at runtime. If a future
// change outgrows this, saving fails cleanly and logs the required size.
#define SAVESTATE_SLOT_CAPACITY (6u * 1024u * 1024u)

// Deliberately in custom .bss.* sections instead of plain .bss: everything in
// plain .bss after the start marker belongs to the game and is copied, while
// 3dsx.ld collects .bss.* sections after all plain .bss sections. These
// buffers are therefore never part of a snapshot even though this file is
// linked into the game image.
static u8 sSlotBuffers[SAVESTATE_SLOT_COUNT][SAVESTATE_SLOT_CAPACITY]
    __attribute__((section(".bss.savestate_buffers"), aligned(8)));

static u32 sSlotSizes[SAVESTATE_SLOT_COUNT]
    __attribute__((section(".bss.savestate_state")));
static u8 sSlotUsed[SAVESTATE_SLOT_COUNT]
    __attribute__((section(".bss.savestate_state")));

static u8 *sPoolStart;
static u32 sPoolSize;
static bool sPoolRegistered;

static u32 savestate_data_size(void)
{
    return (u32) ((uintptr_t) gSavestateDataEnd - (uintptr_t) gSavestateDataStart);
}

static u32 savestate_bss_size(void)
{
    return (u32) ((uintptr_t) gSavestateBssEnd - (uintptr_t) gSavestateBssStart);
}

// ---------------------------------------------------------------------------
// SD card persistence
//
// Slots are mirrored to "sdmc:/sm64state<N>.bin" so they survive a restart.
// The file payload is the exact byte image that lives in the RAM slot: the
// game's writable globals, its BSS, and the main memory pool. That image
// contains absolute pointers, which is only safe because a given binary always
// loads at the same addresses on a 3DS. The header records a format version,
// the exact section sizes, and a CRC32 of the payload, so a file written by a
// different build (or a torn/corrupt file) is rejected and the slot simply
// behaves as empty.
//
// A small append-only log ("sdmc:/sm64_savestate.log") records every save,
// load, and probe, so SD problems can be diagnosed on hardware.
// ---------------------------------------------------------------------------

#define SAVESTATE_FILE_MAGIC 0x44335353u // "SS3D" as little-endian bytes
#define SAVESTATE_FILE_VERSION 1u
#define SAVESTATE_IO_CHUNK (512u * 1024u)
#define SAVESTATE_LOG_PATH "sdmc:/sm64_savestate.log"
#define SAVESTATE_PROBE_RETRY_TICKS (2ull * SYSCLOCK_ARM11)

typedef struct
{
    u32 magic;
    u32 version;
    u32 dataSize;
    u32 bssSize;
    u32 poolSize;
    u32 crc;
} SavestateFileHeader;

// Like the slot buffers above, these must live outside the snapshot regions,
// hence the custom .bss.* sections.
static u32 sCrcTable[256]
    __attribute__((section(".bss.savestate_state")));
static u8 sSlotOnSd[SAVESTATE_SLOT_COUNT]
    __attribute__((section(".bss.savestate_state")));
static u8 sSlotProbed[SAVESTATE_SLOT_COUNT]
    __attribute__((section(".bss.savestate_state")));
static u64 sLastProbeTick
    __attribute__((section(".bss.savestate_state")));
static u8 sLogBooted
    __attribute__((section(".bss.savestate_state")));

static void savestate_log(const char *fmt, ...)
{
    va_list args;
    FILE *file = fopen(SAVESTATE_LOG_PATH, "a");

    if (file == NULL)
        return;

    if (!sLogBooted)
    {
        char cwd[512];
        sLogBooted = 1;
        if (getcwd(cwd, sizeof(cwd)) != NULL)
            fprintf(file, "--- boot: cwd=%s\n", cwd);
        else
            fprintf(file, "--- boot: cwd failed (errno=%d)\n", errno);
    }

    va_start(args, fmt);
    vfprintf(file, fmt, args);
    va_end(args);
    fputc('\n', file);
    fclose(file);
}

static u32 savestate_crc32(const u8 *data, u32 size)
{
    u32 crc = 0xFFFFFFFFu;
    u32 i;

    if (sCrcTable[1] == 0) // table not built yet; entry 1 is nonzero after
    {
        for (i = 0; i < 256; i++)
        {
            u32 c = i;
            int k;
            for (k = 0; k < 8; k++)
                c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            sCrcTable[i] = c;
        }
    }

    for (i = 0; i < size; i++)
        crc = sCrcTable[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8);

    return crc ^ 0xFFFFFFFFu;
}

static void savestate_file_path(int slot, char *out, size_t cap)
{
    snprintf(out, cap, "sdmc:/sm64state%d.bin", slot);
}

// Existence probe, cached per slot. Positive results are permanent; negative
// results are re-checked at most every couple of seconds, so a transient
// failure (card not ready yet) cannot leave the slot dark forever.
static bool savestate_file_exists(int slot)
{
    char path[64];
    FILE *file;
    u64 now;

    if (sSlotProbed[slot] && sSlotOnSd[slot])
        return true;

    if (sSlotProbed[slot])
    {
        now = svcGetSystemTick();
        if (sLastProbeTick != 0
            && now - sLastProbeTick < SAVESTATE_PROBE_RETRY_TICKS)
            return false;
        sLastProbeTick = now;
    }

    savestate_file_path(slot, path, sizeof(path));
    file = fopen(path, "rb");
    if (!sSlotProbed[slot])
        savestate_log("probe slot %d: %s (%s)", slot,
                      file != NULL ? "found" : "not found", path);
    sSlotProbed[slot] = 1;
    sSlotOnSd[slot] = file != NULL;
    if (file != NULL)
        fclose(file);
    return sSlotOnSd[slot] != 0;
}

static bool savestate_write_stream(FILE *file, const SavestateFileHeader *header,
                                   const u8 *payload, u32 size)
{
    u32 offset;
    u32 remaining;

    if (fwrite(header, sizeof(*header), 1, file) != 1)
        return false;

    for (offset = 0; offset < size; offset += SAVESTATE_IO_CHUNK)
    {
        remaining = size - offset;
        if (remaining > SAVESTATE_IO_CHUNK)
            remaining = SAVESTATE_IO_CHUNK;
        if (fwrite(payload + offset, 1, remaining, file) != remaining)
            return false;
    }

    return true;
}

static bool savestate_write_file(int slot, const u8 *payload, u32 size)
{
    char path[64];
    char tmpPath[80];
    SavestateFileHeader header;
    FILE *file;
    bool ok;

    savestate_file_path(slot, path, sizeof(path));
    snprintf(tmpPath, sizeof(tmpPath), "%s.tmp", path);

    header.magic = SAVESTATE_FILE_MAGIC;
    header.version = SAVESTATE_FILE_VERSION;
    header.dataSize = savestate_data_size();
    header.bssSize = savestate_bss_size();
    header.poolSize = sPoolSize;
    header.crc = savestate_crc32(payload, size);

    // Preferred: write a temp file and rename it over the real one, so a power
    // loss mid-write cannot destroy the previous good file for this slot.
    file = fopen(tmpPath, "wb");
    if (file != NULL)
    {
        ok = savestate_write_stream(file, &header, payload, size);
        if (ok)
            ok = fclose(file) == 0;
        else
            fclose(file);

        if (ok)
        {
            if (rename(tmpPath, path) == 0)
            {
                sSlotProbed[slot] = 1;
                sSlotOnSd[slot] = 1;
                savestate_log("save slot %d: ok (%lu bytes) via %s",
                              slot, (unsigned long) size, path);
                return true;
            }
            savestate_log("save slot %d: rename failed (errno=%d), writing directly",
                          slot, errno);
        }
        else
        {
            savestate_log("save slot %d: write to %s failed (errno=%d), writing directly",
                          slot, tmpPath, errno);
        }
        remove(tmpPath);
    }
    else
    {
        savestate_log("save slot %d: open %s failed (errno=%d), writing directly",
                      slot, tmpPath, errno);
    }

    // Fallback: write straight to the final path.
    file = fopen(path, "wb");
    if (file == NULL)
    {
        savestate_log("save slot %d: open %s failed (errno=%d)",
                      slot, path, errno);
        return false;
    }

    ok = savestate_write_stream(file, &header, payload, size);
    if (ok)
        ok = fclose(file) == 0;
    else
        fclose(file);

    if (!ok)
    {
        savestate_log("save slot %d: direct write to %s failed (errno=%d)",
                      slot, path, errno);
        remove(path);
        return false;
    }

    sSlotProbed[slot] = 1;
    sSlotOnSd[slot] = 1;
    savestate_log("save slot %d: ok (%lu bytes), direct %s",
                  slot, (unsigned long) size, path);
    return true;
}

// Pulls a slot back in from the SD card. Only accepts files that exactly
// match this build's memory layout.
static bool savestate_read_file(int slot)
{
    char path[64];
    SavestateFileHeader header;
    FILE *file;
    u8 *dst;
    u32 offset;
    u32 remaining;
    u32 dataSize = savestate_data_size();
    u32 bssSize = savestate_bss_size();
    u32 totalSize;

    savestate_file_path(slot, path, sizeof(path));

    file = fopen(path, "rb");
    if (file == NULL)
    {
        savestate_log("load slot %d: open %s failed (errno=%d)",
                      slot, path, errno);
        return false;
    }

    if (fread(&header, sizeof(header), 1, file) != 1)
    {
        fclose(file);
        sSlotProbed[slot] = 1;
        sSlotOnSd[slot] = 0;
        savestate_log("load slot %d: %s header read failed (errno=%d)",
                      slot, path, errno);
        return false;
    }

    if (header.magic != SAVESTATE_FILE_MAGIC
        || header.version != SAVESTATE_FILE_VERSION
        || header.dataSize != dataSize
        || header.bssSize != bssSize
        || header.poolSize != sPoolSize)
    {
        fclose(file);
        sSlotProbed[slot] = 1;
        sSlotOnSd[slot] = 0;
        savestate_log("load slot %d: %s layout mismatch (magic=%08lx version=%lu "
                      "sizes=%lu/%lu/%lu, expected %lu/%lu/%lu)",
                      slot, path,
                      (unsigned long) header.magic,
                      (unsigned long) header.version,
                      (unsigned long) header.dataSize,
                      (unsigned long) header.bssSize,
                      (unsigned long) header.poolSize,
                      (unsigned long) dataSize,
                      (unsigned long) bssSize,
                      (unsigned long) sPoolSize);
        return false;
    }

    totalSize = dataSize + bssSize + sPoolSize;
    dst = sSlotBuffers[slot];

    for (offset = 0; offset < totalSize; offset += SAVESTATE_IO_CHUNK)
    {
        remaining = totalSize - offset;
        if (remaining > SAVESTATE_IO_CHUNK)
            remaining = SAVESTATE_IO_CHUNK;
        if (fread(dst + offset, 1, remaining, file) != remaining)
        {
            fclose(file);
            sSlotProbed[slot] = 1;
            sSlotOnSd[slot] = 0;
            savestate_log("load slot %d: %s truncated at %lu/%lu (errno=%d)",
                          slot, path, (unsigned long) offset,
                          (unsigned long) totalSize, errno);
            return false;
        }
    }
    fclose(file);

    if (savestate_crc32(dst, totalSize) != header.crc)
    {
        sSlotProbed[slot] = 1;
        sSlotOnSd[slot] = 0;
        savestate_log("load slot %d: %s checksum mismatch", slot, path);
        return false;
    }

    sSlotSizes[slot] = totalSize;
    sSlotUsed[slot] = 1;
    sSlotProbed[slot] = 1;
    sSlotOnSd[slot] = 1;
    savestate_log("load slot %d: ok (%lu bytes) from %s",
                  slot, (unsigned long) totalSize, path);
    return true;
}

// The audio thread must not touch the audio heap or game state while it is
// being copied. No new audio frames are queued from inside the input handler,
// so once the outstanding frame counters and the in-flight flag are clear the
// audio thread cannot start new work until the next game tick.
static void savestate_wait_for_audio_idle(void)
{
    while (s_audio_frame_active
           || s_audio_frames_to_process > 0
           || s_audio_frames_to_tick > 0)
    {
        svcSleepThread(1000000ULL);
    }
}

void savestate_3ds_register_pool(void *start, size_t size)
{
    int i;

    sPoolStart = (u8 *) start;
    sPoolSize = (u32) size;
    sPoolRegistered = start != NULL && size != 0;

    // Probe the slots once at boot and record the memory layout, so the
    // diagnostic log always shows what the next session saw.
    savestate_log("boot: pool=%p poolSize=%lu dataSize=%lu bssSize=%lu",
                  start, (unsigned long) size,
                  (unsigned long) savestate_data_size(),
                  (unsigned long) savestate_bss_size());
    for (i = 0; i < SAVESTATE_SLOT_COUNT; i++)
        savestate_file_exists(i);
}

bool savestate_3ds_save(int slot)
{
    if (slot < 0 || slot >= SAVESTATE_SLOT_COUNT || !sPoolRegistered)
        return false;

    u32 dataSize = savestate_data_size();
    u32 bssSize = savestate_bss_size();
    u32 totalSize = dataSize + bssSize + sPoolSize;

    if (totalSize > SAVESTATE_SLOT_CAPACITY)
    {
        DBG_LOG("savestate: slot %d needs %lu bytes, capacity is %lu",
                slot, (unsigned long) totalSize,
                (unsigned long) SAVESTATE_SLOT_CAPACITY);
        return false;
    }

    savestate_wait_for_audio_idle();

    u8 *dst = sSlotBuffers[slot];

    memcpy(dst, gSavestateDataStart, dataSize);
    dst += dataSize;
    memcpy(dst, gSavestateBssStart, bssSize);
    dst += bssSize;
    memcpy(dst, sPoolStart, sPoolSize);

    sSlotSizes[slot] = totalSize;
    sSlotUsed[slot] = 1;

    // Mirror the slot to the SD card so it survives a restart. The RAM copy
    // stays valid even if this fails (e.g. SD card missing).
    savestate_write_file(slot, sSlotBuffers[slot], totalSize);

    DBG_LOG("savestate: saved slot %d (%lu bytes)", slot,
            (unsigned long) totalSize);
    return true;
}

bool savestate_3ds_load(int slot)
{
    if (slot < 0 || slot >= SAVESTATE_SLOT_COUNT || !sPoolRegistered)
        return false;

    // First use after a restart: pull the slot back in from the SD card.
    if (!sSlotUsed[slot] && !savestate_read_file(slot))
        return false;

    u32 dataSize = savestate_data_size();
    u32 bssSize = savestate_bss_size();
    u32 totalSize = dataSize + bssSize + sPoolSize;

    if (sSlotSizes[slot] != totalSize)
    {
        DBG_LOG("savestate: slot %d layout mismatch (%lu != %lu)", slot,
                (unsigned long) sSlotSizes[slot], (unsigned long) totalSize);
        return false;
    }

    savestate_wait_for_audio_idle();

    const u8 *src = sSlotBuffers[slot];

    memcpy(gSavestateDataStart, src, dataSize);
    src += dataSize;
    memcpy(gSavestateBssStart, src, bssSize);
    src += bssSize;
    memcpy(sPoolStart, src, sPoolSize);

    // The DSP is still playing samples synthesized from the pre-load state.
    audio_3ds_reset_dsp_queue();

    DBG_LOG("savestate: loaded slot %d (%lu bytes)", slot,
            (unsigned long) totalSize);
    return true;
}

bool savestate_3ds_slot_used(int slot)
{
    if (slot < 0 || slot >= SAVESTATE_SLOT_COUNT)
        return false;
    return sSlotUsed[slot] != 0 || savestate_file_exists(slot);
}

#endif
