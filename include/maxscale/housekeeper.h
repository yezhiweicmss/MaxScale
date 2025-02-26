/*
 * Copyright (c) 2018 MariaDB Corporation Ab
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2026-07-11
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */
#pragma once

/**
 * @file housekeeper.h A mechanism to have task run periodically
 */

#include <maxscale/cdefs.h>
#include <maxscale/dcb.hh>

MXS_BEGIN_DECLS

/**
 * The task callback function
 *
 * The parameter is the user data given to the `hktask_add` function.
 *
 * If the function returns true, the same task is added back to the queue and
 * executed again at a later point in time. If the function returns false,
 * the task is removed.
 */
typedef bool (* TASKFN)(void* data);

/**
 * @brief Add a new task
 *
 * The task will be first run @c frequency seconds after this call is
 * made and will the be executed repeatedly every frequency seconds
 * until the task is removed.
 *
 * Task names must be unique.
 *
 * @param name      Task name
 * @param task      Function to execute
 * @param data      Data passed to function as the parameter
 * @param frequency Frequency of execution
 */
void hktask_add(const char* name, TASKFN func, void* data, int frequency);

/**
 * @brief Remove all tasks with this name
 *
 * @param name Task name
 */
void hktask_remove(const char* name);

/**
 * @brief Show tasks as JSON resource
 *
 * @param host Hostname of this server
 *
 * @return Collection of JSON formatted task resources
 */
json_t* hk_tasks_json(const char* host);

MXS_END_DECLS
