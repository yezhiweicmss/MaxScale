/*
 * Copyright (c) 2016 MariaDB Corporation Ab
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

/**
 * @file auroramon.c - Amazon RDS Aurora monitor
 */

#define MXS_MODULE_NAME "auroramon"

#include "auroramon.hh"
#include <mysql.h>
#include <maxbase/alloc.h>
#include <maxbase/assert.h>
#include <maxscale/modinfo.hh>
#include <maxscale/mysql_utils.hh>

using maxscale::MonitorServer;

AuroraMonitor::AuroraMonitor(const std::string& name, const std::string& module)
    : MonitorWorkerSimple(name, module)
{
}

AuroraMonitor::~AuroraMonitor()
{
}

// static
AuroraMonitor* AuroraMonitor::create(const std::string& name, const std::string& module)
{
    return new AuroraMonitor(name, module);
}

bool AuroraMonitor::has_sufficient_permissions()
{
    return test_permissions("SELECT @@aurora_server_id, server_id FROM "
                            "information_schema.replica_host_status WHERE session_id = 'MASTER_SESSION_ID'");
}

/**
 * @brief Update the status of a server
 *
 * This function connects to the database and queries it for its status. The
 * status of the server is adjusted accordingly based on the results of the
 * query.
 *
 * @param monitored_server  Server whose status should be updated
 */
void AuroraMonitor::update_server_status(MonitorServer* monitored_server)
{
    monitored_server->clear_pending_status(SERVER_MASTER | SERVER_SLAVE);

    MYSQL_RES* result;

    /** Connection is OK, query for replica status */
    if (mxs_mysql_query(monitored_server->con,
                        "SELECT @@aurora_server_id, server_id FROM "
                        "information_schema.replica_host_status "
                        "WHERE session_id = 'MASTER_SESSION_ID'") == 0
        && (result = mysql_store_result(monitored_server->con)))
    {
        mxb_assert(mysql_field_count(monitored_server->con) == 2);
        MYSQL_ROW row = mysql_fetch_row(result);
        int status = SERVER_SLAVE;

        /** The master will return a row with two identical non-NULL fields */
        if (row && row[0] && row[1] && strcmp(row[0], row[1]) == 0)
        {
            status = SERVER_MASTER;
        }

        monitored_server->set_pending_status(status);
        mysql_free_result(result);
    }
    else
    {
        monitored_server->mon_report_query_error();
    }
}

/**
 * The module entry point routine. It is this routine that must populate the
 * structure that is referred to as the "module object", this is a structure
 * with the set of external entry points for this module.
 *
 * @return The module object
 */
extern "C" MXS_MODULE* MXS_CREATE_MODULE()
{
    static MXS_MODULE info =
    {
        mxs::MODULE_INFO_VERSION,
        MXS_MODULE_NAME,
        mxs::ModuleType::MONITOR,
        mxs::ModuleStatus::BETA,
        MXS_MONITOR_VERSION,
        "Aurora monitor",
        "V1.0.0",
        MXS_NO_MODULE_CAPABILITIES,
        &maxscale::MonitorApi<AuroraMonitor>::s_api,
        NULL,   /* Process init. */
        NULL,   /* Process finish. */
        NULL,   /* Thread init. */
        NULL,   /* Thread finish. */
        {
            {MXS_END_MODULE_PARAMS}
        }
    };

    return &info;
}
