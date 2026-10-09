<!--
SPDX-FileCopyrightText: 2026 Pierre-Noel Bouteville <pierre-noel.bouteville@allcircuits.com>

SPDX-License-Identifier: LicenseRef-ALLCircuits-ACT-1.1
-->

# format

The printf conversions with no C library behind them, in two shapes:
`format_emit()` streams the output into a sink the caller provides, run by
run, and `format_snprintf()` fills a buffer. `include/format/format.h` states
the contract: which conversions exist and what happens to the others.

The directory is self-contained: `include/` and `src/` are the library, and it
needs nothing but a C11 compiler.

## Build

`CMakeLists.txt` declares `format_api`, the header alone, and
`format_add_variant(<name> [CONFIG <header>])`, which builds the library as
`<name>` against a configuration. A library that calls this one links
`format_api` and leaves the choice of a configuration to the executable, which
links exactly one variant: two on the same link line would each define the
same symbols, and which one wins would depend on the order of the archives.

## Configuration

`src/format_settings.h` picks the configuration every source is built with:
the header named by `FORMAT_CONFIG_FILE`, which `CONFIG` sets, or else
`src/format_config_default.h`, where every option is set so that the library
built alone, the `format` target, compiles all of its code. A program copies
the default, sets each option, and passes the copy to `format_add_variant()`;
`format/format.h` never includes it, so no consumer needs it. A copy that
leaves an option out is rejected at compile time. Each option is a cost every
caller pays, so a program that needs two sets builds two variants.

| Option                    | Adds                         | Costs                                                                          |
| ------------------------- | ---------------------------- | ------------------------------------------------------------------------------ |
| `FORMAT_CONFIG_LONG_LONG` | `%lld` and friends           | a 64 bit accumulator: the 64 bit division helper, unless the optimiser folds it |
| `FORMAT_CONFIG_FLOAT`     | `%f`; needs `LONG_LONG`      | the double arithmetic: runtime helpers on a core without a double precision FPU |
| `FORMAT_CONFIG_FLOAT_BIG` | `%e`, `%g`; needs `FLOAT`    | a table of powers of ten, and `%f` above two to the sixty fourth               |

A conversion that is compiled out prints itself, like any unimplemented one.

## Without a C library

`example/libc.c` defines `snprintf()` and `vsnprintf()` over the library. Copy
it into a program that links no C library, never into one that does.
