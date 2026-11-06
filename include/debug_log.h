#ifndef DEBUG_LOG_H
#define DEBUG_LOG_H

/*
 * Optional SD-card debug logging.
 *
 * Enable with: make DEBUG_LOG=1
 * Disable (default): all logging compiles out.
 */

#ifdef ENABLE_DEBUG_LOG

void debug_log_init(void);
void debug_log_printf(const char *fmt, ...);

#define DBG_LOG(...) debug_log_printf(__VA_ARGS__)
#define DBG_LOG_INIT() debug_log_init()

#else

#define DBG_LOG(...) ((void)0)
#define DBG_LOG_INIT() ((void)0)

#endif

#endif
