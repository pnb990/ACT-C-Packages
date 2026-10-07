# SPDX-FileCopyrightText: 2026 Pierre-Noel Bouteville <pierre-noel.bouteville@allcircuits.com>
#
# SPDX-License-Identifier: LicenseRef-ALLCircuits-ACT-1.1

# syslog is the library, built against the header SYSLOG_CONFIG_FILE names, an absolute path the
# project sets before adding this directory; left unset, against
# include/syslog/syslog_config_default.h.
#
# syslog_port_api is port/syslog_port.h, the functions the program provides to syslog: a port
# links it privately, and a source that only logs links syslog and never sees them.
#
# <syslog.h> includes the configuration, so the definition is public: every translation unit that
# includes it must see the same one, there is one configuration per build, and no header-only
# target. The library calls format through format_api, and leaves the choice of a format variant
# to the executable.

add_library(syslog STATIC ${CMAKE_CURRENT_SOURCE_DIR}/src/syslog.c
                           ${CMAKE_CURRENT_SOURCE_DIR}/src/syslog_config_check.c)
target_include_directories(syslog PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(syslog PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/port)
target_link_libraries(syslog PUBLIC format_api)

if(SYSLOG_CONFIG_FILE)
    target_compile_definitions(syslog PUBLIC SYSLOG_CONFIG_FILE="${SYSLOG_CONFIG_FILE}")
endif()

add_library(syslog_port_api INTERFACE)
target_include_directories(syslog_port_api INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/port)
target_link_libraries(syslog_port_api INTERFACE syslog)
