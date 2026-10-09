<!--
SPDX-FileCopyrightText: 2026 Pierre-Noel Bouteville <pierre-noel.bouteville@allcircuits.com>

SPDX-License-Identifier: LicenseRef-ALLCircuits-ACT-1.1
-->

# syslog

The POSIX `<syslog.h>` for a program with no system logger behind it, with a
level machinery on top: each translation unit registers as a module with a
name, a compile-time level mask and a running one, and the `SYSLOG_*` macros
drop a level either mask excludes. `include/syslog.h` states the contract.

The directory is self-contained: `include/`, `port/` and `src/` are the
library. It
needs a C11 compiler that can place an object in a named section, a linker
that bounds such a section, the [format](../format/) library, and what the
program provides below.

## Build

`CMakeLists.txt` declares two targets:

- `syslog`, the library. A source that logs links it.
- `syslog_port_api`, `port/syslog_port.h`: the functions the program provides
  to the library. Only a port links it, privately; a source that only logs
  never sees them.

The project names its configuration in the CMake variable
`SYSLOG_CONFIG_FILE`, an absolute path, before adding the directory:

```cmake
set(SYSLOG_CONFIG_FILE ${CMAKE_CURRENT_SOURCE_DIR}/include/syslog_config.h)
add_subdirectory(<path>/syslog act_syslog)
```

Left unset, the library builds against
`include/syslog/syslog_config_default.h`. `<syslog.h>` reads the
configuration, so the library hands it on to every target that links
`syslog`: every translation unit that includes it sees the same one, and a
build has one configuration of syslog, shared by every library that logs.

The library calls into format through `format_api`, so the executable links a
variant of format too; which configuration of format that is decides which
conversions a log line may use.

`include/` holds `syslog.h`, so it must come before the C library's headers on
the include path: `#include <syslog.h>` then reaches this one.

## Configuration

`include/syslog/syslog_settings.h`, included by `<syslog.h>`, picks the
configuration: the header named by `SYSLOG_CONFIG_FILE`, which the build sets
on `syslog` and on everything that links it, or else
`include/syslog/syslog_config_default.h`, where every option is set so that
the library built alone compiles all of its code. A program copies the
default, sets each option, and names the copy in `SYSLOG_CONFIG_FILE`. A
copy that leaves an option out is rejected at compile time.

| Option                       | Sets                                                                   |
| ---------------------------- | ---------------------------------------------------------------------- |
| `SYSLOG_CONFIG_COMPILE_MASK` | the levels any module may emit: a constant mask, or a program variable |
| `SYSLOG_CONFIG_LINE_BUFFER`  | 0 to stream each line as text; a size in bytes to hand it whole, its fields apart (see below) |
| `SYSLOG_CONFIG_SOURCE_LOCATION` | 1 to hand each `SYSLOG_*` call's source file and line to the output |
| `SYSLOG_CONFIG_SOURCE_FILE`  | the file name a call hands over, `__FILE_NAME__` where the compiler has it, else `__FILE__` |
| `SYSLOG_CONFIG_COLOR`        | 1 to colour each line by level with ANSI escapes, 0 for a plain stream |
| `SYSLOG_CONFIG_COLOR_RESET`  | the escape that ends a coloured line, read only when the colour is on  |
| `SYSLOG_CONFIG_SHOW_TIME`    | 1 to start the prefix with the millisecond stamp                       |
| `SYSLOG_CONFIG_SHOW_LEVEL`   | 1 to show the level tag in the prefix                                  |
| `SYSLOG_CONFIG_EOL`          | the string literal that ends every line                                |
| `SYSLOG_CONFIG_IMPLICIT_LEVEL_API` | 1 to provide `syslog_err()`, `syslog_info()` and the other levels |
| `SYSLOG_CONFIG_LOCK`         | 1 to bracket each line between `syslog_lock()` and `syslog_unlock()`, which the program provides |
| `SYSLOG_CONFIG_SECTION_ATTRIBUTE(name)` | how the compiler places an object in section `name` and keeps it |
| `SYSLOG_CONFIG_MASK_SECTION` | the section, a string literal, of the running masks                   |
| `SYSLOG_CONFIG_MODULE_SECTION` | the section, a string literal, of the descriptors                   |
| `SYSLOG_CONFIG_MODULE_START` | the symbol the linker defines at the start of the descriptors         |
| `SYSLOG_CONFIG_MODULE_STOP`  | the symbol the linker defines at their end                             |

The options from `SYSLOG_CONFIG_COLOR` to `SYSLOG_CONFIG_EOL` shape the
stream output, and are neither read nor required with the line output.

The source location is the `SYSLOG_*` macros' alone: `syslog()`, `vsyslog()`
and the no-module level functions carry none. With the stream output it shows
in the prefix after the level tag, `foo.c:42 `; with the line output it fills
the line's `file` and `line`. At 0 the macros pass neither, `file` is `NULL`
and nothing reaches the call sites.

The compile mask is a level mask, as a module's own and `setlogmask()` take,
or the name of a `const int` the program defines. A constant strips the calls
it excludes from every build, and changing it rebuilds every file that logs. A
variable rebuilds one source, and strips the same calls only when the build
optimises at link time; without that, they stay and are filtered at run time.
The default configuration shows both methods.

The lock is for a program with more than one writer. The library names no OS:
with `SYSLOG_CONFIG_LOCK` at 1 the program provides the three functions
`port/syslog_port.h` declares, as a port (see below), and at 0 nothing is
declared or called.

## What the program provides

- with `SYSLOG_CONFIG_LOCK` at 1, `syslog_lock_create()`, `syslog_lock()` and
  `syslog_unlock()`: `syslog_init()` creates the lock, whose handle the
  library hands back around every line, and leaves the output off if that
  fails;
- a call to `syslog_init()` before the first log, handing it the millisecond
  time base of the prefix and the output: until then every module's running
  mask is zero, there is no output, and nothing comes out.

## Two outputs

With `SYSLOG_CONFIG_LINE_BUFFER` at 0, the output is a sink, which takes
each line as runs of text, the prefix the options shape and the end of line
included, with no buffer: nothing bounds a line. It suits a terminal or a
file.

With a size, the output is a line output, `syslog_line_write_t`: the body is
formatted into a buffer of that size, truncated to it, and the line is handed
over whole, as a `syslog_line_t` holding its stamp, level, module, source
location and text apart, without a prefix or an end of line. It suits a
destination that carries them as fields - a frame on a link, a record in a
store - and leaves the presentation to whoever reads them, rather than parsing
them back out of the text. The buffer is static, one per program, and the lock serialises the
writers over it.

The modules are gathered by the linker, with no central list. Each registration
puts its descriptor in `SYSLOG_CONFIG_MODULE_SECTION` and its running mask in
`SYSLOG_CONFIG_MASK_SECTION`, and `syslog_init()` walks the descriptors between
`SYSLOG_CONFIG_MODULE_START` and `SYSLOG_CONFIG_MODULE_STOP`. The default
configuration uses the GNU spelling, sections `syslog_const` and `syslog_mask`,
and the bounds `__start_syslog_const` and `__stop_syslog_const`, which GNU ld
defines by itself; a linker script that places the section has to define them,
and both sections must be `KEEP()` in it, since nothing references them by
name and `--gc-sections` would drop them. The mask section belongs where the
startup code zeroes it, in `.bss`. Another compiler or linker names its own
spelling, sections and bounds in the configuration.

## Writing a port

With `SYSLOG_CONFIG_LOCK` at 1 the program provides the three functions of
`port/syslog_port.h`, in a library of its own that the executable links next
to `syslog`. A port over a POSIX mutex:

```c
#include "syslog_port.h"

#include <pthread.h>

static pthread_mutex_t syslog_mutex = PTHREAD_MUTEX_INITIALIZER;

syslog_lock_handle_t syslog_lock_create(void)
{
    return &syslog_mutex;
}

void syslog_lock(syslog_lock_handle_t lock)
{
    pthread_mutex_lock(lock);
}

void syslog_unlock(syslog_lock_handle_t lock)
{
    pthread_mutex_unlock(lock);
}
```

```cmake
find_package(Threads REQUIRED)

add_library(app_syslog_port STATIC syslog_port_pthread.c)
target_link_libraries(app_syslog_port PUBLIC syslog
                                      PRIVATE syslog_port_api Threads::Threads)

add_executable(app main.c)
target_link_libraries(app PRIVATE syslog app_syslog_port format_full)
```

`syslog_port_api` is private, so `app` does not see `syslog_port.h`. A
`syslog_lock_create()` that returns NULL leaves the output off. The lock is
taken from every context that logs, so a program that logs from an interrupt
needs a lock an interrupt may take, such as masking interrupts, not a mutex.
