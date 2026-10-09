/*
 * SPDX-FileCopyrightText: 2026 Pierre-Noel Bouteville <pierre-noel.bouteville@allcircuits.com>
 *
 * SPDX-License-Identifier: LicenseRef-ALLCircuits-ACT-1.1
 */

/*
 * Rejects a configuration the library cannot be built with. An option left
 * out would otherwise read as 0 in an #if, silently on a build without
 * -Wundef. Compiled with the library's sources, so against the same
 * configuration; it produces no code.
 */

#include "format_settings.h"

#if !defined(FORMAT_CONFIG_LONG_LONG)
#    error "the format configuration must define FORMAT_CONFIG_LONG_LONG"
#endif

#if !defined(FORMAT_CONFIG_FLOAT)
#    error "the format configuration must define FORMAT_CONFIG_FLOAT"
#endif

#if !defined(FORMAT_CONFIG_FLOAT_BIG)
#    error "the format configuration must define FORMAT_CONFIG_FLOAT_BIG"
#endif

_Static_assert(!FORMAT_CONFIG_FLOAT || FORMAT_CONFIG_LONG_LONG,
               "FORMAT_CONFIG_FLOAT needs FORMAT_CONFIG_LONG_LONG");

_Static_assert(!FORMAT_CONFIG_FLOAT_BIG || FORMAT_CONFIG_FLOAT,
               "FORMAT_CONFIG_FLOAT_BIG needs FORMAT_CONFIG_FLOAT");
