# SPDX-FileCopyrightText: 2026 Pierre-Noel Bouteville <pierre-noel.bouteville@allcircuits.com>
#
# SPDX-License-Identifier: LicenseRef-ALLCircuits-ACT-1.1

# format_api: the header alone, for a library that calls format and leaves to the executable the
# choice of the configuration it links.
#
#   format_add_variant(<name> [CONFIG <header>])
#
# builds the library as <name> against <header>, an absolute path handed to the sources as
# FORMAT_CONFIG_FILE; without CONFIG, against src/format_config_default.h. The target format is
# that default build.

add_library(format_api INTERFACE)
target_include_directories(format_api INTERFACE ${CMAKE_CURRENT_LIST_DIR}/include)

function(format_add_variant name)
    cmake_parse_arguments(ARG "" "CONFIG" "" ${ARGN})

    add_library(${name} STATIC ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/src/format.c
                               ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/src/format_config_check.c)
    target_link_libraries(${name} PUBLIC format_api)

    if(ARG_CONFIG)
        target_compile_definitions(${name} PRIVATE FORMAT_CONFIG_FILE="${ARG_CONFIG}")
    endif()
endfunction()

format_add_variant(format)
