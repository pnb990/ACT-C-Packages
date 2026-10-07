/*
 * SPDX-FileCopyrightText: 2026 Pierre-Noel Bouteville <pierre-noel.bouteville@allcircuits.com>
 *
 * SPDX-License-Identifier: LicenseRef-ALLCircuits-ACT-1.1
 */

/*
 * The compile settings the library is built with when the program names no
 * configuration of its own: every option set, so that a build of the library
 * alone compiles all of its code. A program that wants another set copies this
 * file, changes it, and names the copy in FORMAT_CONFIG_FILE (see README.md).
 */

#ifndef FORMAT_CONFIG_DEFAULT_H
#define FORMAT_CONFIG_DEFAULT_H

/**
 * Whether the `ll` length modifier is compiled in. It widens the accumulator to
 * 64 bits, so on a 32 bit core every division, whatever the number's width,
 * goes through the runtime helper rather than an instruction.
 *
 * Cleared, `%lld` prints itself and leaves its argument unread, like any
 * unimplemented conversion.
 */
#define FORMAT_CONFIG_LONG_LONG 1

/**
 * Whether `%f` is compiled in. The cost is not the conversion but the double
 * arithmetic under it: on a core without a double precision FPU, a Cortex-M4F
 * among them, every double operation calls a runtime helper, linked in as soon
 * as this is set.
 *
 * Needs FORMAT_CONFIG_LONG_LONG: a double's integer part only comes out exact
 * through a 64 bit accumulator.
 *
 * Cleared, `%f` prints itself; a fixed point value can still be written
 * `%d.%03d`.
 */
#define FORMAT_CONFIG_FLOAT 1

/**
 * Whether the exponent notation is compiled in: `%e`, and `%g` which picks
 * between the two notations. It also lets `%f` write a magnitude above two to
 * the sixty fourth, which fixed notation cannot hold - without it such a value
 * prints `ovf` rather than a wrong number.
 *
 * Needs FORMAT_CONFIG_FLOAT, and adds the table of powers of ten and the
 * normalisation on top of its cost.
 */
#define FORMAT_CONFIG_FLOAT_BIG 1

#endif /* FORMAT_CONFIG_DEFAULT_H */
