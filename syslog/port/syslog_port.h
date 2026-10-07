/*
 * SPDX-FileCopyrightText: 2026 Pierre-Noel Bouteville <pierre-noel.bouteville@allcircuits.com>
 *
 * SPDX-License-Identifier: LicenseRef-ALLCircuits-ACT-1.1
 */

#ifndef SYSLOG_PORT_H
#define SYSLOG_PORT_H

#include <syslog.h>

#if SYSLOG_CONFIG_LOCK
/*
 * The lock that serialises one whole line against another writer, provided by
 * a port of the program when SYSLOG_CONFIG_LOCK is 1, so that the library
 * names no OS. A program that turns the lock on without a port gets a link
 * error.
 *
 * The contract between the library and its ports, not part of <syslog.h>: a
 * source that only logs never sees it. The build hands this directory to the
 * library's sources and to the ports alone (see README.md).
 */

/**
 * The lock as the program represents it. The library only stores it and hands
 * it back.
 */
typedef void *syslog_lock_handle_t;

/**
 * @brief Create the lock. Called once, by syslog_init().
 *
 * @return The lock, or NULL when none can be had: syslog_init() then leaves
 *         the output off.
 */
syslog_lock_handle_t syslog_lock_create(void);

/**
 * @brief Take the lock, before the first run of a line.
 *
 * Called from every context that logs, so the lock must be one those contexts
 * may take.
 *
 * @param lock What syslog_lock_create() returned.
 */
void syslog_lock(syslog_lock_handle_t lock);

/**
 * @brief Release the lock, after the last run of a line.
 *
 * @param lock What syslog_lock_create() returned.
 */
void syslog_unlock(syslog_lock_handle_t lock);
#endif /* SYSLOG_CONFIG_LOCK */

#endif /* SYSLOG_PORT_H */
