# SPDX-FileCopyrightText: 2026 Pierre-Noel Bouteville <pnb990@gmail.com>
#
# SPDX-License-Identifier: BSD-3-Clause

# Every library of this repository built alone, with its default configuration: what the CI checks
# and what a developer runs to do the same (CMakePresets.json). A project never comes through here:
# it adds the <lib>/ it wants with add_subdirectory(), and compiles it with its own options.
#
# Warnings are errors here, because a project that turns them into errors does so for the
# libraries too.

cmake_minimum_required(VERSION 3.22)
project(act_c_packages C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra -Werror)

# format first: syslog links format_api.
add_subdirectory(format)
add_subdirectory(syslog)
