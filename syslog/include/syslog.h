/*
 * SPDX-FileCopyrightText: 2026 Pierre-Noel Bouteville <pierre-noel.bouteville@allcircuits.com>
 *
 * SPDX-License-Identifier: LicenseRef-ALLCircuits-ACT-1.1
 */

#ifndef SYSLOG_H
#define SYSLOG_H

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

/*
 * A syslog on the trace path, in the POSIX shape so that code written against
 * <syslog.h> compiles here and against a C library's syslog unchanged.
 *
 * This header is reached as <syslog.h> and, with -I on the include path, wins
 * over the C library's own: the two are never seen in one translation unit, and
 * a program that includes this one links this library, never the C library's
 * syslog.
 *
 * Three storeys:
 *
 * - the POSIX floor: syslog(), vsyslog(), setlogmask() and the eight level
 *   macros, all portable;
 * - a typed level and a per-module registration on top, so a component gets a
 *   name and a level mask the running program can change;
 * - the SYSLOG_* macros, which carry the compile-time masks and the per-module
 *   test the POSIX function cannot: a level a compile-time mask excludes is
 *   dead code, format string and all.
 *
 * A running filter is a level mask, as POSIX setlogmask() takes one: a bit per
 * level, so a build can enable an arbitrary set - not only every level down to
 * a threshold. LOG_MASK() and LOG_UPTO() build one.
 *
 * openlog() and closelog() are not provided: there is no connection to open, so
 * a translation unit calling them gets a link error rather than a silent no-op.
 */

/*
 * The POSIX floor. The eight levels as integer macros, spelled token for token
 * as the C library spells them, so the two headers could not disagree even if
 * both were somehow visible. Lower is more severe, so a threshold test reads
 * `level <= threshold` - the opposite of the intuition.
 */
#define LOG_EMERG   0
#define LOG_ALERT   1
#define LOG_CRIT    2
#define LOG_ERR     3
#define LOG_WARNING 4
#define LOG_NOTICE  5
#define LOG_INFO    6
#define LOG_DEBUG   7

/*
 * The mask helpers, spelled as the C library spells them. LOG_MASK(pri) is the
 * one-level bit; LOG_UPTO(pri) is every level at least as severe. A module's
 * running filter and setlogmask() both take such a mask.
 */
#define LOG_MASK(priority) (1 << (priority))
#define LOG_UPTO(priority) ((1 << ((priority) + 1)) - 1)

/* The program's settings, see README.md. Included here, after the level
 * macros its values are spelled with. */
#include "syslog/syslog_settings.h"

/*
 * Argument checking against the standard printf grammar. GCC only; a compiler
 * without the attribute loses the check, not the build. The format engine is
 * narrower than that grammar, see format/format.h.
 */
#if defined(__GNUC__)
#    define SYSLOG_CHECKED(format_index, first_argument_index)                 \
        __attribute__((format(printf, format_index, first_argument_index)))
#else
#    define SYSLOG_CHECKED(format_index, first_argument_index)
#endif

void syslog(int priority, const char *format, ...)
    SYSLOG_CHECKED(/*format=*/2, /*first_arg=*/3);
void vsyslog(int priority, const char *format, va_list args)
    SYSLOG_CHECKED(/*format=*/2, /*no_varargs=*/0);
int setlogmask(int mask);

/**
 * @brief Wait for the trace output to have left the chip.
 *
 * A courtesy before a reset or a halt so a last line is not cut off. The sink
 * writes synchronously - a call returns once the characters are on the wire -
 * so nothing is queued and this returns at once; it exists so code that must
 * flush a buffered sink compiles and reads the same here.
 *
 * @param timeout_s How long to wait at most, in seconds. Ignored while no sink
 *                  buffers.
 */
void syslog_drain(int timeout_s);

/**
 * The levels, typed. An enumeration aliasing the macros so a signature says
 * it takes a level, without the names colliding: the macros cannot
 * themselves be an enumeration under the POSIX names, since the C library
 * spells some of them as expressions an enumerator cannot hold.
 */
typedef enum
{
    SYSLOG_LEVEL_EMERG   = LOG_EMERG,
    SYSLOG_LEVEL_ALERT   = LOG_ALERT,
    SYSLOG_LEVEL_CRIT    = LOG_CRIT,
    SYSLOG_LEVEL_ERR     = LOG_ERR,
    SYSLOG_LEVEL_WARNING = LOG_WARNING,
    SYSLOG_LEVEL_NOTICE  = LOG_NOTICE,
    SYSLOG_LEVEL_INFO    = LOG_INFO,
    SYSLOG_LEVEL_DEBUG   = LOG_DEBUG
} syslog_level_t;

/*
 * The build-wide compile mask, from the configuration. A translation unit that
 * wants a narrower one defines SYSLOG_COMPILE_MASK before including this
 * header.
 */
#ifndef SYSLOG_COMPILE_MASK
#    define SYSLOG_COMPILE_MASK SYSLOG_CONFIG_COMPILE_MASK
#endif

/*
 * The compile-time mask that admits every level: what a module passes to
 * SYSLOG_MODULE_REGISTER when it wants no compile-time restriction of its own,
 * only the build-wide compile mask above.
 */
#define SYSLOG_MASK_ALL LOG_UPTO(SYSLOG_LEVEL_DEBUG)

/**
 * What one component is, for the level machinery: its name for the prefix, the
 * mask it starts at, and the byte its running mask lives in. The byte is
 * pointed at rather than held inline so the descriptor sits in flash while the
 * mask it governs sits in RAM.
 */
typedef struct
{
    const char *name;     /**< The component's name, for the prefix. */
    uint8_t default_mask; /**< The level mask syslog_init() writes at boot. */
    uint8_t *mask;        /**< The RAM byte holding the running level mask. */
} syslog_module_t;

/**
 * @brief Declare the calling translation unit as a log module.
 *
 * Used once per file, before its first log. It defines, under fixed file-local
 * names the macros below find without being told: the compile-time mask (an
 * enumeration constant, so a level it excludes folds to dead code at the call
 * site), the running-mask byte (in RAM, section SYSLOG_CONFIG_MASK_SECTION)
 * and a descriptor (in flash, section SYSLOG_CONFIG_MODULE_SECTION) that
 * points at it. SYSLOG_CONFIG_SECTION_ATTRIBUTE() places the latter two and
 * keeps the compiler from dropping them; the program's linker script KEEP()s
 * their sections (see README.md), so --gc-sections does not. The descriptor is
 * aligned to its own alignment, no more: the compiler would otherwise
 * over-align it and the linker would pad between the gathered descriptors,
 * whereas syslog_init() walks them as a plain array and needs the stride to be
 * exactly one descriptor.
 *
 * @param module_name         The component's name, an identifier, stringised
 *                            for the prefix.
 * @param module_compile_mask The levels compiled in at all, a constant mask
 *                            (SYSLOG_MASK_ALL for no restriction beyond the
 *                            build-wide compile mask). A level outside it costs
 *                            nothing: no code and no format string.
 * @param module_default_mask The level mask it starts at at run time, usually
 *                            LOG_UPTO(some_level); only levels also in
 *                            module_compile_mask can ever come out.
 */
#define SYSLOG_MODULE_REGISTER(module_name,                                    \
                               module_compile_mask,                            \
                               module_default_mask)                            \
    enum                                                                       \
    {                                                                          \
        syslog_current_compile_mask = (module_compile_mask)                    \
    };                                                                         \
    static uint8_t syslog_current_mask SYSLOG_CONFIG_SECTION_ATTRIBUTE(        \
        SYSLOG_CONFIG_MASK_SECTION);                                           \
    static _Alignas(syslog_module_t)                                           \
        const syslog_module_t syslog_current_module                            \
        SYSLOG_CONFIG_SECTION_ATTRIBUTE(SYSLOG_CONFIG_MODULE_SECTION) = {      \
            .name         = #module_name,                                      \
            .default_mask = (module_default_mask),                             \
            .mask         = &syslog_current_mask,                              \
        }

/**
 * @brief Format one log line and stream it to the sink, prefix and all.
 *
 * The macros reach it; call it directly only to log against a descriptor that
 * is not the calling file's own. It applies no level test - the caller has
 * already decided - and it never buffers the body, so nothing bounds a line.
 *
 * @param module The descriptor whose name prefixes the line.
 * @param level  The line's level, for the prefix tag.
 * @param format A printf format, followed by its arguments.
 */
void syslog_emit(const syslog_module_t *module,
                 syslog_level_t level,
                 const char *format,
                 ...) SYSLOG_CHECKED(/*format=*/3, /*first_arg=*/4);

/*
 * The log call. The first two tests are compile-time constants on a constant
 * severity - the build-wide compile mask and the module's own - so a
 * level either one excludes is dead code the compiler drops: the call, its
 * arguments and the format string all go. The third tests the level's bit in
 * the per-module mask, changeable at run time. None is reachable through the
 * POSIX syslog() function, which is why these exist.
 */
#define SYSLOG(severity, ...)                                                  \
    do                                                                         \
    {                                                                          \
        if ((LOG_MASK(severity) & SYSLOG_COMPILE_MASK) &&                      \
            (LOG_MASK(severity) & syslog_current_compile_mask) &&              \
            (LOG_MASK(severity) & *syslog_current_module.mask))                \
        {                                                                      \
            syslog_emit(&syslog_current_module, (severity), __VA_ARGS__);      \
        }                                                                      \
    } while (0)

#define SYSLOG_ERR(...)     SYSLOG(SYSLOG_LEVEL_ERR, __VA_ARGS__)
#define SYSLOG_WARNING(...) SYSLOG(SYSLOG_LEVEL_WARNING, __VA_ARGS__)
#define SYSLOG_INFO(...)    SYSLOG(SYSLOG_LEVEL_INFO, __VA_ARGS__)
#define SYSLOG_DEBUG(...)   SYSLOG(SYSLOG_LEVEL_DEBUG, __VA_ARGS__)

#if SYSLOG_CONFIG_IMPLICIT_LEVEL_API
/*
 * The no-module convenience set, one function per level. Like syslog() and
 * unlike the SYSLOG_* macros, they carry no module and no compile-time test:
 * the line always reaches the emitter, with an empty module name. For a line
 * that belongs to a module - filtered, prefixed with its name - use the macros.
 * SYSLOG_CONFIG_IMPLICIT_LEVEL_API drops the whole set.
 */
void syslog_emerg(const char *format, ...)
    SYSLOG_CHECKED(/*format=*/1, /*first_arg=*/2);
void syslog_alert(const char *format, ...)
    SYSLOG_CHECKED(/*format=*/1, /*first_arg=*/2);
void syslog_crit(const char *format, ...)
    SYSLOG_CHECKED(/*format=*/1, /*first_arg=*/2);
void syslog_err(const char *format, ...)
    SYSLOG_CHECKED(/*format=*/1, /*first_arg=*/2);
void syslog_warning(const char *format, ...)
    SYSLOG_CHECKED(/*format=*/1, /*first_arg=*/2);
void syslog_notice(const char *format, ...)
    SYSLOG_CHECKED(/*format=*/1, /*first_arg=*/2);
void syslog_info(const char *format, ...)
    SYSLOG_CHECKED(/*format=*/1, /*first_arg=*/2);
void syslog_debug(const char *format, ...)
    SYSLOG_CHECKED(/*format=*/1, /*first_arg=*/2);
#endif /* SYSLOG_CONFIG_IMPLICIT_LEVEL_API */

/**
 * The millisecond stamp the prefix carries: milliseconds since start, free to
 * wrap. The program is the one that knows its time base.
 */
typedef uint32_t (*syslog_time_ms_t)(void);

/**
 * Where each run of a log line goes, prefix, body and the closing newline
 * alike. It has the shape of a format write callback, so the emitter streams
 * each run straight through it. A run is not null terminated, and context is
 * always null. A call must return whatever becomes of the run, since a log call
 * must never wedge its caller.
 */
typedef void (*syslog_sink_write_t)(const char *data,
                                    size_t length,
                                    void *context);

/**
 * @brief Take the time base and the sink, and set every registered module to
 * its default mask.
 *
 * Walks the descriptors the linker gathered and writes each one's default into
 * its RAM byte. Called once at start-up, before the first log: until it runs a
 * module's mask is zero and there is no sink, so nothing comes out. With
 * SYSLOG_CONFIG_LOCK it creates the lock first, and leaves the output off if
 * that fails.
 *
 * @param time_ms    The time base of the prefix.
 * @param sink_write The destination of every line.
 */
void syslog_init(syslog_time_ms_t time_ms, syslog_sink_write_t sink_write);

#endif /* SYSLOG_H */
