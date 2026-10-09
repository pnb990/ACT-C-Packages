<!--
SPDX-FileCopyrightText: 2026 Pierre-Noel Bouteville <pierre-noel.bouteville@allcircuits.com>
SPDX-FileCopyrightText: 2026 Pierre-Noel Bouteville <pnb990@gmail.com>

SPDX-License-Identifier: LicenseRef-ALLCircuits-ACT-1.1
-->

# ACT C packages

Libraries in C11. Each one lives in its own directory, `<lib>/`, and builds
itself with `<lib>/CMakeLists.txt`, which a project adds with
`add_subdirectory()`.

## Targets

`<lib>` is the library name, `<LIB>` the same in upper case.

| Target or function              | Content                                                                   |
| ------------------------------- | ------------------------------------------------------------------------- |
| `<lib>_api`                     | the headers alone, an `INTERFACE` target, when they read no configuration |
| `<lib>_add_variant(<name> ...)` | builds the library as `<name>`, against the arguments given               |
| `<lib>`                         | the library built alone, with its defaults                                |

A library that calls another links its `<lib>_api` and leaves the choice of a
variant to the executable, which links exactly one: two variants of the same
library each define the same symbols, and which one a link keeps would depend
on the order of the archives.

A library whose public headers read its configuration has no `<lib>_api`: a
translation unit built against another configuration than the variant it
links with would disagree with it silently. A library that calls it links the
variant the project declares instead, and the project declares one per
configuration its callers share.

## Configuration

A library ships the configuration it is built with by default, with every
option set, so that a build of the library alone compiles all of its code. A
program that wants another one copies the default, sets it, and names the copy
to `<lib>_add_variant()`, which hands it to the sources as a macro,
`<LIB>_CONFIG_FILE`; the library's README says which header that is. Two
configurations are two variants. The macro is private to the variant, or public
when the library's headers read the configuration.

## Ports

A function the library declares and the program provides, such as a lock over
the program's OS, is a port. The library names nothing of the program, and
declares its ports in a header of their own, never in its public ones: a
variant `<name>` comes with `<name>_port_api`, which only a port links, so a
caller of the library never sees them. The project writes the port in its own
component and builds it as a library of its own that links `<name>_port_api`
privately, so that an executable linking the port does not see the header
either; the executable links the port it wants next to the variant, as it picks
any other implementation.

## Building within a project

A library is compiled with the options of the project that builds it, warnings
included: a project that turns warnings into errors does so for the library
too. The component that builds a library adjusts that through the library's
own compile options, `target_compile_options()` on the variant, rather than
by patching the library.

The documentation a project generates covers its own tree only. A library here
documents itself in its README and in the comments of its sources.

## Building alone

The top-level `CMakeLists.txt` builds every library alone, with its default
configuration and warnings as errors; a project never goes through it. One
preset per compiler, the same commands as the CI:

```sh
cmake --preset gcc                  # or arm-none-eabi-gcc
cmake --build --preset gcc
```
