/*
 * SPDX-FileCopyrightText: 2026 Pierre-Noel Bouteville <pierre-noel.bouteville@allcircuits.com>
 *
 * SPDX-License-Identifier: LicenseRef-ALLCircuits-ACT-1.1
 */

/*
 * The one place that picks the configuration the library's sources are built
 * against: the header the program names in FORMAT_CONFIG_FILE, or the default
 * shipped here when it names none.
 */

#ifndef FORMAT_SETTINGS_H
#define FORMAT_SETTINGS_H

#ifdef FORMAT_CONFIG_FILE
#    include FORMAT_CONFIG_FILE
#else
#    include "format_config_default.h"
#endif

#endif /* FORMAT_SETTINGS_H */
