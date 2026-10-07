/*
 * SPDX-FileCopyrightText: 2026 Pierre-Noel Bouteville <pierre-noel.bouteville@allcircuits.com>
 *
 * SPDX-License-Identifier: LicenseRef-ALLCircuits-ACT-1.1
 */

/*
 * The one place that picks the configuration <syslog.h> is read with: the
 * header the program names in SYSLOG_CONFIG_FILE, or the default shipped here
 * when it names none. Included from <syslog.h> only.
 */

#ifndef SYSLOG_SYSLOG_SETTINGS_H
#define SYSLOG_SYSLOG_SETTINGS_H

#ifdef SYSLOG_CONFIG_FILE
#    include SYSLOG_CONFIG_FILE
#else
#    include "syslog/syslog_config_default.h"
#endif

#endif /* SYSLOG_SYSLOG_SETTINGS_H */
