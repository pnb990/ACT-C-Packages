/*
 * SPDX-FileCopyrightText: 2026 Pierre-Noel Bouteville <pierre-noel.bouteville@allcircuits.com>
 *
 * SPDX-License-Identifier: LicenseRef-ALLCircuits-ACT-1.1
 */

/*
 * Rejects a configuration the library cannot be built with. Compiled with the
 * library's sources, so against the same configuration; it produces no code.
 * The configuration is reached through <syslog.h>, the one header that
 * includes it, after the level macros its values are spelled with.
 *
 * The value of the mask is not checked: every int is one, the bits above
 * LOG_DEBUG's naming no level. Nor is the type of the two strings: a value
 * that is not a string literal fails where the library uses it.
 */

#include <syslog.h>

#if !defined(SYSLOG_CONFIG_COMPILE_MASK)
#    error "the syslog configuration must define SYSLOG_CONFIG_COMPILE_MASK"
#endif

#if !defined(SYSLOG_CONFIG_COLOR)
#    error "the syslog configuration must define SYSLOG_CONFIG_COLOR"
#endif

#if SYSLOG_CONFIG_COLOR && !defined(SYSLOG_CONFIG_COLOR_RESET)
#    error "the syslog configuration must define SYSLOG_CONFIG_COLOR_RESET"
#endif

#if !defined(SYSLOG_CONFIG_SHOW_TIME)
#    error "the syslog configuration must define SYSLOG_CONFIG_SHOW_TIME"
#endif

#if !defined(SYSLOG_CONFIG_SHOW_LEVEL)
#    error "the syslog configuration must define SYSLOG_CONFIG_SHOW_LEVEL"
#endif

#if !defined(SYSLOG_CONFIG_EOL)
#    error "the syslog configuration must define SYSLOG_CONFIG_EOL"
#endif

#if !defined(SYSLOG_CONFIG_IMPLICIT_LEVEL_API)
#    error                                                                     \
        "the syslog configuration must define SYSLOG_CONFIG_IMPLICIT_LEVEL_API"
#endif

#if !defined(SYSLOG_CONFIG_LOCK)
#    error "the syslog configuration must define SYSLOG_CONFIG_LOCK"
#endif

#if !defined(SYSLOG_CONFIG_SECTION_ATTRIBUTE)
#    error                                                                     \
        "the syslog configuration must define SYSLOG_CONFIG_SECTION_ATTRIBUTE"
#endif

#if !defined(SYSLOG_CONFIG_MASK_SECTION)
#    error "the syslog configuration must define SYSLOG_CONFIG_MASK_SECTION"
#endif

#if !defined(SYSLOG_CONFIG_MODULE_SECTION)
#    error "the syslog configuration must define SYSLOG_CONFIG_MODULE_SECTION"
#endif

#if !defined(SYSLOG_CONFIG_MODULE_START)
#    error "the syslog configuration must define SYSLOG_CONFIG_MODULE_START"
#endif

#if !defined(SYSLOG_CONFIG_MODULE_STOP)
#    error "the syslog configuration must define SYSLOG_CONFIG_MODULE_STOP"
#endif
