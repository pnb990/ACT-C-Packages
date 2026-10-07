/*
 * SPDX-FileCopyrightText: 2026 Pierre-Noel Bouteville <pierre-noel.bouteville@allcircuits.com>
 *
 * SPDX-License-Identifier: LicenseRef-ALLCircuits-ACT-1.1
 */

#include "syslog.h"

#include <inttypes.h>

#include "format/format.h"
#include "syslog_port.h"

/*
 * The descriptors, gathered by the linker into SYSLOG_CONFIG_MODULE_SECTION,
 * between the two bounds the configuration names. They bracket the array with
 * no central list, so a module joins it by registering, touching no shared
 * file.
 */
extern const syslog_module_t SYSLOG_CONFIG_MODULE_START[];
extern const syslog_module_t SYSLOG_CONFIG_MODULE_STOP[];

/* What syslog_init() was handed; null until then, and nothing is emitted. */
static syslog_time_ms_t syslog_time_ms = NULL;
#if SYSLOG_CONFIG_LINE_BUFFER
static syslog_line_write_t syslog_output = NULL;

/* The body of the line being made, shared by every writer under the lock: a
 * buffer on the stack would cost its size to every task that logs. */
static char syslog_line_buffer[SYSLOG_CONFIG_LINE_BUFFER];
#else
static syslog_sink_write_t syslog_output = NULL;
#endif

#if SYSLOG_CONFIG_LOCK
static syslog_lock_handle_t syslog_lock_handle = NULL;
#endif

#if !SYSLOG_CONFIG_LINE_BUFFER
#    if SYSLOG_CONFIG_SHOW_LEVEL
/* The tag each level wears in the prefix, indexed by the level value 0..7. */
static const char *const syslog_level_tag[] = { "EMERG", "ALERT",   "CRIT",
                                                "ERR",   "WARNING", "NOTICE",
                                                "INFO",  "DEBUG" };
#    endif

#    if SYSLOG_CONFIG_COLOR
/*
 * The colour each level wears, indexed the same way. An ANSI select-graphic
 * sequence written before the prefix; SYSLOG_CONFIG_COLOR_RESET after the body
 * undoes it. Text and background are `ESC[3<n>m` / `ESC[4<n>m`, n a colour
 * 0..7. The three most severe levels take a red background so they cannot be
 * missed.
 */
#        define ANSI_ESC     "\033["
#        define ANSI_TEXT(c) ANSI_ESC "3" c "m"
#        define ANSI_BACK(c) ANSI_ESC "4" c "m"
#        define ANSI_BLACK   "0"
#        define ANSI_RED     "1"
#        define ANSI_GREEN   "2"
#        define ANSI_YELLOW  "3"
#        define ANSI_CYAN    "6"
#        define ANSI_WHITE   "7"

static const char *const syslog_level_color[] = {
    ANSI_BACK(ANSI_RED) ANSI_TEXT(ANSI_WHITE),  /* EMERG   */
    ANSI_BACK(ANSI_RED) ANSI_TEXT(ANSI_YELLOW), /* ALERT   */
    ANSI_BACK(ANSI_RED) ANSI_TEXT(ANSI_BLACK),  /* CRIT    */
    ANSI_TEXT(ANSI_RED),                        /* ERR     */
    ANSI_TEXT(ANSI_YELLOW),                     /* WARNING */
    ANSI_TEXT(ANSI_GREEN),                      /* NOTICE  */
    ANSI_TEXT(ANSI_CYAN),                       /* INFO    */
    ANSI_TEXT(ANSI_WHITE),                      /* DEBUG   */
};
#    endif
#endif /* !SYSLOG_CONFIG_LINE_BUFFER */

#if SYSLOG_CONFIG_LINE_BUFFER
/*
 * The line output: the body formatted into the shared buffer, truncated to
 * it, and handed over whole with its level and module apart. The level is
 * masked as in the stream output below.
 */
static void syslog_emit_line(const char *name,
                             syslog_level_t level,
                             const char *format,
                             va_list args)
{
    syslog_line_t line;
    const int length = format_vsnprintf(syslog_line_buffer,
                                        sizeof(syslog_line_buffer),
                                        format,
                                        args);

    line.time_ms = syslog_time_ms();
    line.level   = (syslog_level_t)((unsigned int)level & 0x07u);
    line.module  = name;
    line.text    = syslog_line_buffer;
    line.length  = ((size_t)length < sizeof(syslog_line_buffer))
                       ? (size_t)length
                       : (sizeof(syslog_line_buffer) - 1u);
    syslog_output(&line);
}
#else
/*
 * The stream output: the prefix and the body stream straight to the sink with
 * no line buffer, so nothing bounds a line. The level indexes the tag and
 * colour tables masked to its low three bits, so a POSIX priority carrying a
 * facility in its high bits still lands on a tag rather than off the end.
 */
static void syslog_emit_stream(const char *name,
                               syslog_level_t level,
                               const char *format,
                               va_list args)
{
#    if SYSLOG_CONFIG_COLOR || SYSLOG_CONFIG_SHOW_LEVEL
    const unsigned int level_index = (unsigned int)level & 0x07u;
#    else
    (void)level;
#    endif

#    if SYSLOG_CONFIG_COLOR
    format_emit(syslog_output, NULL, "%s", syslog_level_color[level_index]);
#    endif
#    if SYSLOG_CONFIG_SHOW_TIME
    format_emit(syslog_output, NULL, "[%" PRIu32 "] ", syslog_time_ms());
#    endif
#    if SYSLOG_CONFIG_SHOW_LEVEL
    format_emit(syslog_output, NULL, "%s ", syslog_level_tag[level_index]);
#    endif
    format_emit(syslog_output, NULL, "%s: ", name);
    format_vemit(syslog_output, NULL, format, args);
#    if SYSLOG_CONFIG_COLOR
    format_emit(syslog_output, NULL, "%s", SYSLOG_CONFIG_COLOR_RESET);
#    endif
    syslog_output(SYSLOG_CONFIG_EOL, sizeof(SYSLOG_CONFIG_EOL) - 1u, NULL);
}
#endif /* SYSLOG_CONFIG_LINE_BUFFER */

/*
 * The one path every entry point funnels into. It takes only what the line
 * needs - a name and a level - not a module descriptor: the filtering is done
 * by the caller (the macro tests the module's mask; the POSIX path is governed
 * by setlogmask), so nothing here has a module to consult. With
 * SYSLOG_CONFIG_LOCK the program's lock brackets the whole line, so a second
 * writer does not interleave characters into it, nor into the line buffer.
 */
static void syslog_emit_v(const char *name,
                          syslog_level_t level,
                          const char *format,
                          va_list args)
{
    if ((syslog_time_ms != NULL) && (syslog_output != NULL))
    {
#if SYSLOG_CONFIG_LOCK
        syslog_lock(syslog_lock_handle);
#endif
#if SYSLOG_CONFIG_LINE_BUFFER
        syslog_emit_line(name, level, format, args);
#else
        syslog_emit_stream(name, level, format, args);
#endif
#if SYSLOG_CONFIG_LOCK
        syslog_unlock(syslog_lock_handle);
#endif
    }
}

void syslog_emit(const syslog_module_t *module,
                 syslog_level_t level,
                 const char *format,
                 ...)
{
    va_list args;

    va_start(args, format);
    syslog_emit_v(module->name, level, format, args);
    va_end(args);
}

void vsyslog(int priority, const char *format, va_list args)
{
    syslog_emit_v("", (syslog_level_t)(priority & 0x07), format, args);
}

void syslog(int priority, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    vsyslog(priority, format, args);
    va_end(args);
}

#if SYSLOG_CONFIG_IMPLICIT_LEVEL_API
/*
 * The no-module level functions, generated from one body: each opens its
 * va_list and hands it to the shared path with an empty name and its own level.
 * They match syslog(), not the macros - no compile-time strip, no module.
 */
#    define SYSLOG_DEFINE_LEVEL(suffix, level)                                 \
        void syslog_##suffix(const char *format, ...)                          \
        {                                                                      \
            va_list args;                                                      \
            va_start(args, format);                                            \
            syslog_emit_v("", (level), format, args);                          \
            va_end(args);                                                      \
        }

SYSLOG_DEFINE_LEVEL(emerg, SYSLOG_LEVEL_EMERG)
SYSLOG_DEFINE_LEVEL(alert, SYSLOG_LEVEL_ALERT)
SYSLOG_DEFINE_LEVEL(crit, SYSLOG_LEVEL_CRIT)
SYSLOG_DEFINE_LEVEL(err, SYSLOG_LEVEL_ERR)
SYSLOG_DEFINE_LEVEL(warning, SYSLOG_LEVEL_WARNING)
SYSLOG_DEFINE_LEVEL(notice, SYSLOG_LEVEL_NOTICE)
SYSLOG_DEFINE_LEVEL(info, SYSLOG_LEVEL_INFO)
SYSLOG_DEFINE_LEVEL(debug, SYSLOG_LEVEL_DEBUG)

#    undef SYSLOG_DEFINE_LEVEL
#endif /* SYSLOG_CONFIG_IMPLICIT_LEVEL_API */

int setlogmask(int mask)
{
    /* The global floor is added with the level API; here it is inert and keeps
     * the POSIX signature so callers need not change when it gains teeth. */
    (void)mask;
    return 0;
}

void syslog_drain(int timeout_s)
{
    /* The sink writes synchronously - a character is on the wire before the
     * write returns - so nothing is queued to wait for. The parameter is here
     * for a future buffered sink and for source that must flush one. */
    (void)timeout_s;
}

#if SYSLOG_CONFIG_LINE_BUFFER
void syslog_init(syslog_time_ms_t time_ms, syslog_line_write_t output)
#else
void syslog_init(syslog_time_ms_t time_ms, syslog_sink_write_t output)
#endif
{
    const syslog_module_t *module = SYSLOG_CONFIG_MODULE_START;
    int status                    = 0;

#if SYSLOG_CONFIG_LOCK
    syslog_lock_handle = syslog_lock_create();
    if (syslog_lock_handle == NULL)
    {
        status = -1;
    }
#endif

    /* Without the lock the output stays null, so every line is dropped rather
     * than written unserialised. */
    if (status >= 0)
    {
        syslog_time_ms = time_ms;
        syslog_output  = output;
    }

    for (; module < SYSLOG_CONFIG_MODULE_STOP; module++)
    {
        *module->mask = module->default_mask;
    }
}
