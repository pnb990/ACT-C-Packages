/*
 * SPDX-FileCopyrightText: 2026 Pierre-Noel Bouteville <pierre-noel.bouteville@allcircuits.com>
 *
 * SPDX-License-Identifier: LicenseRef-ALLCircuits-ACT-1.1
 */

/*
 * The C library's snprintf() and vsnprintf(), for a program that links none:
 * copied into the program, it lets third-party sources calling them link
 * against the format library. Real functions rather than macros, so the
 * prototypes stay those of <stdio.h>. Never linked next to a C library, whose
 * own definitions would collide.
 */

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

#include "format/format.h"

int vsnprintf(char *out, size_t size, const char *format, va_list args)
{
    return format_vsnprintf(out, size, format, args);
}

int snprintf(char *out, size_t size, const char *format, ...)
{
    va_list args;
    int length = 0;

    va_start(args, format);
    length = format_vsnprintf(out, size, format, args);
    va_end(args);

    return length;
}
