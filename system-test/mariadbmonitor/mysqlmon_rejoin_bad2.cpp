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

#include <vector>
#include <iostream>
#include <iterator>

#include <maxtest/testconnections.hh>
#include "fail_switch_rejoin_common.cpp"

using std::string;
using std::cout;
using std::endl;

static void expect(TestConnections& test, const char* zServer, const StringSet& expected)
{
    StringSet found = test.get_server_status(zServer);

    std::ostream_iterator<string> oi(cout, ", ");

    cout << zServer << ", expected states: ";
    std::copy(expected.begin(), expected.end(), oi);
    cout << endl;

    cout << zServer << ", found states   : ";
    std::copy(found.begin(), found.end(), oi);
    cout << endl;

    if (found != expected)
    {
        cout << "ERROR, found states are not the same as the expected ones." << endl;
        ++test.global_result;
    }

    cout << endl;
}

static void expect(TestConnections& test, const char* zServer, const char* zState)
{
    StringSet s;
    s.insert(zState);
    expect(test, zServer, s);
}

static void expect(TestConnections& test, const char* zServer, const char* zState1, const char* zState2)
{
    StringSet s;
    s.insert(zState1);
    s.insert(zState2);
    expect(test, zServer, s);
}

int main(int argc, char** argv)
{
    TestConnections test(argc, argv);
    test.repl->connect();
    // Set up test table
    basic_test(test);

    MYSQL* maxconn = test.maxscale->open_rwsplit_connection();
    // Advance gtid:s a bit to so gtid variables are updated.
    generate_traffic_and_check(test, maxconn, 5);
    test.repl->sync_slaves();
    get_output(test);

    print_gtids(test);
    mysql_close(maxconn);

    // Stop master, wait for failover
    cout << "Stopping master, should auto-failover." << endl;
    int master_id_old = get_master_server_id(test);
    test.repl->stop_node(0);
    test.maxscale->wait_for_monitor(2);
    get_output(test);
    int master_id_new = get_master_server_id(test);
    cout << "Master server id is " << master_id_new << endl;
    test.expect(master_id_new > 0 && master_id_new != master_id_old,
                "Failover did not promote a new master.");
    if (test.global_result != 0)
    {
        return test.global_result;
    }

    cout << "Stopping MaxScale for a moment.\n";
    // Stop maxscale to prevent an unintended rejoin.
    if (!test.maxscale->stop_and_check_stopped())
    {
        test.expect(false, "Could not stop MaxScale.");
        return test.global_result;
    }

    // Restart old master. Then add some events to it.
    test.repl->start_node(0, (char*)"");
    test.repl->connect();
    cout << "Adding more events to node 0.\n";
    generate_traffic_and_check(test, test.repl->nodes[0], 5);
    print_gtids(test);

    cout << "Starting MaxScale, node 0 should not be able to join because it has extra events.\n";
    // Restart maxscale. Should not rejoin old master.
    if (!test.maxscale->start_and_check_started())
    {
        test.expect(false, "Could not start MaxScale.");
        return test.global_result;
    }
    test.maxscale->wait_for_monitor(2);
    get_output(test);

    expect(test, "server1", "Running");
    if (test.global_result != 0)
    {
        cout << "Old master is a member of the cluster when it should not be. \n";
        return test.global_result;
    }

    // Set current master to replicate from the old master. The new master should lose its Master status
    // and auto_rejoin will redirect servers 3 and 4 so that all replicate from server 1.
    cout << "Setting server " << master_id_new << " to replicate from server 1. Server " << master_id_new
         << " should lose its master status and other servers should be redirected to server 1.\n";
    const char CHANGE_CMD_FMT[] = "CHANGE MASTER TO MASTER_HOST = '%s', MASTER_PORT = %d, "
                                  "MASTER_USE_GTID = current_pos, "
                                  "MASTER_USER='repl', MASTER_PASSWORD = 'repl';";
    char cmd[256];
    int ind = master_id_new - 1;
    snprintf(cmd, sizeof(cmd), CHANGE_CMD_FMT, test.repl->ip_private(0), test.repl->port[0]);
    MYSQL** nodes = test.repl->nodes;
    mysql_query(nodes[ind], cmd);
    mysql_query(nodes[ind], "START SLAVE;");
    test.maxscale->wait_for_monitor(2);
    get_output(test);

    expect(test, "server1", "Master", "Running");
    expect(test, "server2", "Slave", "Running");
    expect(test, "server3", "Slave", "Running");
    expect(test, "server4", "Slave", "Running");
    return test.global_result;
}
