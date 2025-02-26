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

#include <iostream>
#include <vector>

#include <maxtest/testconnections.hh>
#include "fail_switch_rejoin_common.cpp"

using std::string;
using std::cout;

int main(int argc, char** argv)
{
    TestConnections test(argc, argv);
    test.repl->connect();
    // Set up test table
    basic_test(test);
    // Advance gtid:s a bit to so gtid variables are updated.
    MYSQL* maxconn = test.maxscale->open_rwsplit_connection();
    generate_traffic_and_check(test, maxconn, 10);
    test.repl->sync_slaves();

    test.tprintf(LINE);
    print_gtids(test);
    test.tprintf(LINE);
    char result_tmp[bufsize];
    string gtid_begin;
    if (find_field(maxconn, GTID_QUERY, GTID_FIELD, result_tmp) == 0)
    {
        gtid_begin = result_tmp;
    }
    mysql_close(maxconn);

    test.tprintf("Stopping MaxScale...");
    // Mess with the slaves to fix situation such that only one slave can be rejoined. Stop maxscale.
    if (!test.maxscale->stop_and_check_stopped())
    {
        test.expect(false, "Could not stop MaxScale.");
        return test.global_result;
    }

    // Leave first of three slaves connected so it's clear which one is the master server.
    const char STOP_SLAVE[] = "STOP SLAVE;";
    const char RESET_SLAVE[] = "RESET SLAVE ALL;";
    const char READ_ONLY_OFF[] = "SET GLOBAL read_only=0;";

    const int FIRST_MOD_NODE = 2;   // Modify nodes 2 & 3
    const int NODE_COUNT = 4;
    MYSQL** nodes = test.repl->nodes;
    for (int i = FIRST_MOD_NODE; i < NODE_COUNT; i++)
    {
        if ((mysql_query(nodes[i], STOP_SLAVE) != 0) || (mysql_query(nodes[i], RESET_SLAVE) != 0)
            || (mysql_query(nodes[i], READ_ONLY_OFF) != 0))
        {
            test.expect(false, "Could not stop slave connections and/or disable read_only for node %d.", i);
        }
    }

    if (test.ok())
    {
        // Add more events to node3.
        string gtid_node2, gtid_node3;
        test.tprintf("Sending more inserts to server 4.");
        generate_traffic_and_check(test, nodes[3], 10);
        // Save gtids
        if (find_field(nodes[2], GTID_QUERY, GTID_FIELD, result_tmp) == 0)
        {
            gtid_node2 = result_tmp;
        }
        if (find_field(nodes[3], GTID_QUERY, GTID_FIELD, result_tmp) == 0)
        {
            gtid_node3 = result_tmp;
        }
        print_gtids(test);
        bool gtids_ok = (gtid_begin == gtid_node2 && gtid_node2 < gtid_node3);
        test.expect(gtids_ok, "Gtid:s have not advanced correctly.");
    }


    test.tprintf("Restarting MaxScale. Server 4 should not rejoin the cluster.");
    test.tprintf(LINE);
    if (!test.maxscale->start_and_check_started())
    {
        test.expect(false, "Could not start MaxScale.");
        return test.global_result;
    }
    test.maxscale->wait_for_monitor(2);
    get_output(test);

    if (test.ok())
    {
        StringSet node2_states = test.get_server_status("server3");
        StringSet node3_states = test.get_server_status("server4");
        bool states_n2_ok = (node2_states.find("Slave") != node2_states.end());
        bool states_n3_ok = (node3_states.find("Slave") == node3_states.end());
        test.expect(states_n2_ok, "Node 2 has not rejoined when it should have.");
        test.expect(states_n3_ok, "Node 3 rejoined when it shouldn't have.");
    }

    if (test.ok())
    {
        // Finally, fix replication by telling the current master to replicate from server4
        test.tprintf(
            "Setting server 1 to replicate from server 4. Auto-rejoin should redirect servers 2 and 3.");
        const char CHANGE_CMD_FMT[] = "CHANGE MASTER TO MASTER_HOST = '%s', MASTER_PORT = %d, "
                                      "MASTER_USE_GTID = current_pos, MASTER_USER='repl', MASTER_PASSWORD = 'repl';";
        char change_cmd[256];
        snprintf(change_cmd, sizeof(change_cmd), CHANGE_CMD_FMT, test.repl->ip_private(3), test.repl->port[3]);
        test.try_query(nodes[0], "%s", change_cmd);
        test.try_query(nodes[0], "START SLAVE;");
        test.maxscale->wait_for_monitor(2);
        get_output(test);
        int master_id = get_master_server_id(test);
        test.expect(master_id == 4, "Server 4 should be the cluster master.");
        StringSet node0_states = test.get_server_status("server1");
        bool states_n0_ok = (node0_states.find("Slave") != node0_states.end()
                             && node0_states.find("Relay Master") == node0_states.end());
        test.expect(states_n0_ok, "Server 1 is not a slave when it should be.");
    }

    cout << "Reseting cluster...\n";
    string reset_cmd = "maxctrl call command mysqlmon reset-replication MySQL-Monitor server1";
    test.maxscale->ssh_output(reset_cmd);
    test.maxscale->wait_for_monitor(1);
    test.expect(get_master_server_id(test) == 1, "server1 is not the master when it should. "
                                                 "reset-replication must have failed.");
    return test.global_result;
}
