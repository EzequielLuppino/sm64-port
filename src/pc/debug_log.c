#include "debug_log.h"

#ifdef ENABLE_DEBUG_LOG

#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>

#include "game/game_init.h"

static FILE *sDebugLogFile = NULL;
static int sDebugLogFailed = 0;

void debug_log_init(void) {
    char cwd[512];

    if (sDebugLogFile != NULL || sDebugLogFailed) {
        return;
    }

#ifdef TARGET_N3DS
    sDebugLogFile = fopen("sdmc:/sm64_debug.log", "w");
    if (sDebugLogFile == NULL) {
        sDebugLogFile = fopen("sm64_debug.log", "w");
    }
#else
    sDebugLogFile = fopen("sm64_debug.log", "w");
#endif

    if (sDebugLogFile == NULL) {
        sDebugLogFailed = 1;
        return;
    }

    setvbuf(sDebugLogFile, NULL, _IONBF, 0);
    fprintf(sDebugLogFile, "=== SM64 3DS debug log ===\n");

    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        fprintf(sDebugLogFile, "cwd=%s\n", cwd);
    } else {
        fprintf(sDebugLogFile, "cwd=(failed errno=%d)\n", errno);
    }
    fflush(sDebugLogFile);
}

void debug_log_printf(const char *fmt, ...) {
    va_list args;

    if (sDebugLogFile == NULL) {
        if (sDebugLogFailed) {
            return;
        }
        debug_log_init();
        if (sDebugLogFile == NULL) {
            return;
        }
    }

    fprintf(sDebugLogFile, "[f=%u] ", (unsigned) gGlobalTimer);
    va_start(args, fmt);
    vfprintf(sDebugLogFile, fmt, args);
    va_end(args);
    fputc('\n', sDebugLogFile);
    fflush(sDebugLogFile);
}

#else

typedef int debug_log_disabled_t;

#endif
