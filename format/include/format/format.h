/*
 * SPDX-FileCopyrightText: 2001-2002 Georges Menie
 * SPDX-FileCopyrightText: 2026 Pierre-Noel Bouteville <pierre-noel.bouteville@allcircuits.com>
 *
 * SPDX-License-Identifier: LicenseRef-ALLCircuits-ACT-1.1
 */

#ifndef FORMAT_FORMAT_H
#define FORMAT_FORMAT_H

#include <stdarg.h>
#include <stddef.h>

/*
 * A formatter with no C library behind it, in two shapes: one that streams the
 * conversion into a caller supplied sink, and one that fills a buffer.
 *
 * Conversions: %d %i %u %x %X %c %s %p and %%, with the '-' and '0' flags, a
 * width, and the hh / h / l / ll / z length modifiers. Precision applies to %s,
 * capping the characters read.
 *
 * Three more are compile settings rather than conversions, since each is paid
 * for by every program that links this one (see README.md):
 *
 *   %f        FORMAT_CONFIG_FLOAT
 *   %e %g     FORMAT_CONFIG_FLOAT_BIG
 *   %lld ...  FORMAT_CONFIG_LONG_LONG
 *
 * Everything else is unimplemented, notably %o, the '+', ' ' and '#' flags, the
 * '*' width, and a precision on a number. The compiler does not catch them: the
 * variadic entry points carry GCC's printf format attribute, whose grammar is
 * wider than this engine. An unimplemented conversion prints itself - '%' then
 * the offending character - and leaves its argument unread, shifting the ones
 * after it, so a wrong format shows up in the output rather than vanishing.
 *
 * What the float conversions promise, when compiled in:
 *
 * - Rounding is to nearest at the precision asked, ties to even: the rule a C
 *   library follows under the default rounding mode, so the two match.
 * - %f is exact at every precision: its digits are read from the value's bits
 *   with integer arithmetic, never by scaling, so nothing falls off a mantissa.
 * - %e and %g reach their notation by scaling through powers of ten, which does
 *   round. Exact to the twelfth significant digit; past it the last digits may
 *   differ by one from an exact conversion. The default precision of six is
 *   well inside that.
 * - A precision above seventeen digits is met with seventeen: past that a
 *   double carries nothing, and further digits would misread as the value's.
 * - inf, -inf and nan print as those words. A zero keeps its sign.
 * - %f holds a magnitude below two to the sixty fourth. Above it, %e's notation
 *   is used, or `ovf` without FORMAT_CONFIG_FLOAT_BIG to provide it. Never a
 *   number that is not the value.
 *
 * One deliberate extension: %s with a null pointer prints "(null)", as GNU and
 * musl do. C11 7.21.6.1p8 makes it undefined behaviour, so this is our choice,
 * not something portable code may rely on - a formatter on a log path must not
 * fault on a pointer the caller failed to check.
 */

/*
 * Argument checking against the standard printf grammar. GCC only; the port
 * that has no such attribute loses the check, not the build.
 */
#if defined(__GNUC__)
#    define FORMAT_CHECKED(format_index, first_argument_index)                 \
        __attribute__((format(printf, format_index, first_argument_index)))
#else
#    define FORMAT_CHECKED(format_index, first_argument_index)
#endif

/**
 * @brief Takes a run of characters produced by a conversion.
 *
 * Called once per run - a literal stretch of format, the digits of a number,
 * the body of a %s - never per character, so a sink with a queue behind it
 * pushes a block not a byte. The run is never empty, not null terminated, and
 * does not outlive the call.
 *
 * Reports nothing: a trace path drops what it cannot take, and the formatter
 * could do nothing with a refusal halfway through a line.
 *
 * @param data    Characters to take, not null terminated.
 * @param length  How many of them, never zero.
 * @param context What was handed to the emitting call.
 */
typedef void (*format_write_callback_t)(const char *data,
                                        size_t length,
                                        void *context);

int format_emit(format_write_callback_t write,
                void *context,
                const char *format,
                ...) FORMAT_CHECKED(3, 4);

int format_vemit(format_write_callback_t write,
                 void *context,
                 const char *format,
                 va_list args) FORMAT_CHECKED(3, 0);

int format_snprintf(char *out, size_t size, const char *format, ...)
    FORMAT_CHECKED(3, 4);

int format_vsnprintf(char *out, size_t size, const char *format, va_list args)
    FORMAT_CHECKED(3, 0);

#endif /* FORMAT_FORMAT_H */
