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

#include <maxtest/mariadb_nodes.hh>
#include "failover_common.cpp"

int main(int argc, char** argv)
{
    TestConnections test(argc, argv);
    test.repl->connect();

    test.maxscale->wait_for_monitor(2);
    basic_test(test);
    print_gtids(test);

    // Part 1
    int node0_id = prepare_test_1(test);
    test.maxscale->wait_for_monitor(2);
    check_test_1(test, node0_id);

    if (test.global_result != 0)
    {
        return test.global_result;
    }

    // Part 2
    prepare_test_2(test);
    test.maxscale->wait_for_monitor(2);
    check_test_2(test);

    if (test.global_result != 0)
    {
        return test.global_result;
    }

    // Part 3
    prepare_test_3(test);
    test.maxscale->wait_for_monitor(2);
    check_test_3(test);

    return test.global_result;
}
