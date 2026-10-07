/*
 * SPDX-FileCopyrightText: 2026 Pierre-Noel Bouteville <pierre-noel.bouteville@allcircuits.com>
 *
 * SPDX-License-Identifier: LicenseRef-ALLCircuits-ACT-1.1
 */

/*
 * The compile settings <syslog.h> is read with when the program names no
 * configuration of its own: every option set, so that a build of the library
 * alone compiles all of its code. A program that wants another set copies this
 * file, changes it, and names the copy in SYSLOG_CONFIG_FILE (see README.md).
 *
 * Included from <syslog.h> after the LOG_* level macros, whose values it uses.
 * It is not meant to be included on its own.
 */

#ifndef SYSLOG_SYSLOG_CONFIG_DEFAULT_H
#define SYSLOG_SYSLOG_CONFIG_DEFAULT_H

/**
 * The build-wide compile mask, the levels any module may emit, tested at every
 * log call. A single translation unit can strip further by defining its own
 * SYSLOG_COMPILE_MASK before including <syslog.h>. Two methods:
 *
 * - A constant mask, built with LOG_UPTO() or LOG_MASK(), as below. The calls
 *   it excludes are dead code on any build, format strings included. Changing
 *   it rebuilds every file that logs.
 *
 * - A const int the program defines in one of its sources:
 *
 *       extern const int app_syslog_compile_mask;
 *       #define SYSLOG_CONFIG_COMPILE_MASK app_syslog_compile_mask
 *
 *   Changing it rebuilds that one source. The calls it excludes are dropped
 *   only under link-time optimisation, otherwise filtered at run time.
 */
#define SYSLOG_CONFIG_COMPILE_MASK LOG_UPTO(LOG_DEBUG)

/**
 * The output form. 0 streams each line to a sink as text, prefix and all, run
 * by run, unbounded: the options from SYSLOG_CONFIG_COLOR to SYSLOG_CONFIG_EOL
 * shape it. A size in bytes, the terminator's included, formats each body into
 * a buffer of that size instead, and hands the line whole to a line output,
 * its level and module apart: for a destination that carries them as fields -
 * a frame, a record - and presents the line itself. The prefix options are
 * then neither read nor required. A body longer than the buffer is truncated.
 *
 * The two forms exclude each other, so this default, the stream, leaves the
 * line output's code out.
 */
#define SYSLOG_CONFIG_LINE_BUFFER 0

/**
 * Colour the line by level: a per-level ANSI sequence before the prefix and a
 * reset after the body. 1 to colour, 0 to leave the stream plain. A trace read
 * by a terminal that understands the escapes wants it on; one captured to a
 * file or a viewer that does not wants it off, or the escapes show up as text.
 */
#define SYSLOG_CONFIG_COLOR 1

/**
 * The sequence emitted after the body, once, to undo SYSLOG_CONFIG_COLOR before
 * the line ends. `\033[0m` is the full reset; only consulted when the colour is
 * on.
 */
#define SYSLOG_CONFIG_COLOR_RESET "\033[0m"

/**
 * Show the millisecond stamp (`[1234] `) at the head of the prefix. 1 to show,
 * 0 to drop it and the time base call that feeds it.
 */
#define SYSLOG_CONFIG_SHOW_TIME 1

/**
 * Show the level tag (`INFO `, `ERR `, ...) in the prefix. 1 to show, 0 to drop
 * it; the colour, when on, already tells the levels apart.
 */
#define SYSLOG_CONFIG_SHOW_LEVEL 1

/**
 * The line terminator, a string literal written once after the body (and after
 * the colour reset). "\n" is the plain choice. "\033[K\n" erases to the end
 * of the line first, which clears the trailing background when a colour paints
 * one to the margin.
 */
#define SYSLOG_CONFIG_EOL "\n"

/**
 * Provide the no-module convenience functions syslog_err(), syslog_info() and
 * the rest of the level set. 1 to compile them in, 0 to leave them out; unused
 * ones are dropped by --gc-sections either way, so this only matters when none
 * is used and the declarations are unwanted.
 */
#define SYSLOG_CONFIG_IMPLICIT_LEVEL_API 1

/**
 * Serialise each whole line against another writer. 0 for a program with a
 * single flow of control, which has nothing to serialise against. 1 to bracket
 * every line between syslog_lock() and syslog_unlock(), which the program then
 * defines along with syslog_lock_create(), typically over a mutex of its OS;
 * <syslog.h> declares the three.
 */
#define SYSLOG_CONFIG_LOCK 1

/**
 * How the compiler places an object in a named section and keeps it although
 * nothing references it by name: the running masks and the descriptors
 * SYSLOG_MODULE_REGISTER() defines. The GNU spelling below serves GCC and
 * Clang; another compiler maps it to its own.
 */
#define SYSLOG_CONFIG_SECTION_ATTRIBUTE(name)                                  \
    __attribute__((section(name), used))

/**
 * The section, a string literal, that gathers every module's running mask. It
 * belongs where the startup code zeroes it, and the linker script KEEP()s it.
 */
#define SYSLOG_CONFIG_MASK_SECTION "syslog_mask"

/**
 * The section, a string literal, that gathers every module's descriptor, and
 * the two symbols the linker defines at its start and at its end, between which
 * syslog_init() walks the descriptors. The names must agree with each other and
 * with the linker: GNU ld defines __start_<section> and __stop_<section> by
 * itself when the section name is a C identifier, as here; a linker script that
 * places the section defines them, and another linker names its own.
 */
#define SYSLOG_CONFIG_MODULE_SECTION "syslog_const"
#define SYSLOG_CONFIG_MODULE_START   __start_syslog_const
#define SYSLOG_CONFIG_MODULE_STOP    __stop_syslog_const

#endif /* SYSLOG_SYSLOG_CONFIG_DEFAULT_H */
