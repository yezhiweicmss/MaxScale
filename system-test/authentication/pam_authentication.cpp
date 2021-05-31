/*
 * Copyright (c) 2016 MariaDB Corporation Ab
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2025-05-25
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */

#include <maxtest/testconnections.hh>
#include <iostream>
#include <string>
#include <maxbase/format.hh>
#include <maxtest/mariadb_connector.hh>

using std::string;
using std::cout;
namespace
{
const string plugin_path = string(mxt::test_build_dir) + "/../connector-c/install/lib/mariadb/plugin";
}

MYSQL* pam_login(TestConnections& test, int port, const string& user, const string& pass,
                 const string& database);
bool test_pam_login(TestConnections& test, int port, const string& user, const string& pass,
                    const string& database);
bool test_mapped_pam_login(TestConnections& test, int port, const string& user, const string& pass,
                           const string& expected_user);
bool test_normal_login(TestConnections& test, int port, const string& user, const string& pass,
                       const string& db = "");
void test_main(TestConnections& test);

int main(int argc, char** argv)
{
    TestConnections test;
    return test.run_test(argc, argv, test_main);
}

void test_main(TestConnections& test)
{
    test.repl->connect();

    const int N = 2;    // Use just two backends so that setup is fast.
    test.expect(test.repl->N >= N, "Test requires at least two backends.");
    if (!test.ok())
    {
        return;
    }

    const char install_plugin[] = "INSTALL SONAME 'auth_pam';";
    const char uninstall_plugin[] = "UNINSTALL SONAME 'auth_pam';";

    const char pam_user[] = "dduck";
    const char pam_pw[] = "313";
    const char pam_config_name[] = "pam_config_msg";

    const string add_user_cmd = (string)"useradd " + pam_user;
    const string add_pw_cmd = mxb::string_printf("echo %s:%s | chpasswd", pam_user, pam_pw);
    const string read_shadow = "chmod o+r /etc/shadow";

    const string remove_user_cmd = (string)"userdel --remove " + pam_user;
    const string read_shadow_off = "chmod o-r /etc/shadow";

    const string pam_message_contents = "Lorem ipsum";

    // To make most out of this test, use a custom pam service configuration. It needs to be written to
    // all backends.

    string pam_config_path_src = mxb::string_printf("%s/authentication/%s", test_dir, pam_config_name);
    string pam_config_path_dst = mxb::string_printf("/etc/pam.d/%s", pam_config_name);

    const char pam_msgfile[] = "pam_test_msg.txt";
    string pam_msgfile_path_src = mxb::string_printf("%s/authentication/%s", test_dir, pam_msgfile);
    string pam_msgfile_path_dst = mxb::string_printf("/tmp/%s", pam_msgfile);

    const string delete_pam_conf_cmd = "rm -f " + pam_config_path_dst;
    const string delete_pam_message_cmd = "rm -f " + pam_msgfile_path_dst;

    test.repl->connect();
    auto mxs_ip = test.maxscales->ip4();

    // Prepare the backends for PAM authentication. Enable the plugin and create a user. Also,
    // make /etc/shadow readable for all so that the server process can access it.

    for (int i = 0; i < N; i++)
    {
        MYSQL* conn = test.repl->nodes[i];
        test.try_query(conn, install_plugin);

        auto& vm = test.repl->backend(i)->vm_node();
        vm.run_cmd_sudo(add_user_cmd);
        vm.run_cmd_sudo(add_pw_cmd);
        vm.run_cmd_sudo(read_shadow);

        // Also, copy the custom pam config and message file.
        vm.copy_to_node_sudo(pam_config_path_src, pam_config_path_dst);
        vm.copy_to_node_sudo(pam_msgfile_path_src, pam_msgfile_path_dst);
    }

    // Also create the user on the node running MaxScale, as the MaxScale PAM plugin compares against
    // local users.
    auto& mxs_vm = test.maxscales->vm_node();
    mxs_vm.run_cmd_sudo(add_user_cmd);
    mxs_vm.run_cmd_sudo(add_pw_cmd);
    mxs_vm.run_cmd_sudo(read_shadow);
    mxs_vm.copy_to_node_sudo(pam_config_path_src, pam_config_path_dst);
    mxs_vm.copy_to_node_sudo(pam_msgfile_path_src, pam_msgfile_path_dst);

    if (test.ok())
    {
        cout << "PAM-plugin installed and users created on all servers. Starting MaxScale.\n";
        test.maxscales->restart();
    }
    else
    {
        cout << "Test preparations failed.\n";
    }

    auto& mxs = test.maxscale();

    if (test.ok())
    {
        auto servers_status = mxs.get_servers();
        servers_status.check_servers_status({mxt::ServerInfo::master_st, mxt::ServerInfo::slave_st});
        servers_status.print();
    }

    // Helper function for checking PAM-login. If db is empty, log to null database.
    auto try_log_in = [&test](const string& user, const string& pass, const string& database) {
            int port = test.maxscales->rwsplit_port;
            if (!test_pam_login(test, port, user, pass, database))
            {
                test.expect(false, "PAM login failed.");
            }
        };

    auto update_users = [&mxs]() {
            mxs.stop();
            mxs.delete_log();
            mxs.start();
            mxs.wait_monitor_ticks();
        };

    if (test.ok())
    {
        // First, test that MaxCtrl login with the pam user works.
        string cmd = mxb::string_printf("-u %s -p %s show maxscale", pam_user, pam_pw);
        test.check_maxctrl(cmd);
        if (test.ok())
        {
            cout << "'maxctrl " << cmd << "' works.\n";
        }
    }

    const char create_pam_user_fmt[] = "CREATE OR REPLACE USER '%s'@'%%' IDENTIFIED VIA pam USING '%s';";
    if (test.ok())
    {
        MYSQL* conn = test.repl->nodes[0];
        // Create the PAM user on the master, it will replicate. Use the standard password service for
        // authenticating.
        test.try_query(conn, create_pam_user_fmt, pam_user, pam_config_name);
        test.try_query(conn, "GRANT SELECT ON *.* TO '%s'@'%%';", pam_user);
        test.try_query(conn, "FLUSH PRIVILEGES;");
        sleep(1);
        test.repl->sync_slaves();
        update_users();
        mxs.get_servers().print();

        // If ok so far, try logging in with PAM.
        if (test.ok())
        {
            cout << "Testing normal PAM user.\n";
            try_log_in(pam_user, pam_pw, "");
            test.log_includes(pam_message_contents.c_str());
        }

        // Remove the created user.
        test.try_query(conn, "DROP USER '%s'@'%%';", pam_user);
    }

    if (test.ok())
    {
        const char dummy_user[] = "proxy-target";
        const char dummy_pw[] = "unused_pw";
        // Basic PAM authentication seems to be working. Now try with an anonymous user proxying to
        // the real user. The following does not actually do proper user mapping, as that requires further
        // setup on the backends. It does however demonstrate that MaxScale detects the anonymous user and
        // accepts the login of a non-existent user with PAM.
        MYSQL* conn = test.repl->nodes[0];
        // Add a user which will be proxied.
        test.try_query(conn, "CREATE OR REPLACE USER '%s'@'%%' IDENTIFIED BY '%s';", dummy_user, dummy_pw);

        // Create the anonymous catch-all user and allow it to proxy as the "proxy-target", meaning it
        // gets the target's privileges. Granting the proxy privilege is a bit tricky since only the local
        // root user can give it.
        test.try_query(conn, create_pam_user_fmt, "", pam_config_name);
        test.repl->ssh_node_f(0, true, "echo \"GRANT PROXY ON '%s'@'%%' TO ''@'%%'; FLUSH PRIVILEGES;\" | "
                                       "mysql --user=root",
                              dummy_user);
        test.repl->sync_slaves();
        update_users();
        mxs.get_servers().print();

        if (test.ok())
        {
            // Again, try logging in with the same user.
            cout << "Testing anonymous proxy user.\n";
            try_log_in(pam_user, pam_pw, "");
            test.log_includes(pam_message_contents.c_str());
        }

        // Remove the created users.
        test.try_query(conn, "DROP USER '%s'@'%%';", dummy_user);
        test.try_query(conn, "DROP USER ''@'%%';");
    }

    if (test.ok())
    {
        // Test roles. Create a user without privileges but with a default role. The role has another role
        // which finally has the privileges to the db.
        MYSQL* conn = test.repl->nodes[0];
        test.try_query(conn, create_pam_user_fmt, pam_user, pam_config_name);
        const char create_role_fmt[] = "CREATE ROLE %s;";
        const char grant_role_fmt[] = "GRANT %s TO %s;";
        const char r1[] = "role1";
        const char r2[] = "role2";
        const char r3[] = "role3";
        const char dbname[] = "empty_db";

        // pam_user->role1->role2->role3->privilege
        test.try_query(conn, "CREATE OR REPLACE DATABASE %s;", dbname);
        test.try_query(conn, create_role_fmt, r1);
        test.try_query(conn, create_role_fmt, r2);
        test.try_query(conn, create_role_fmt, r3);
        test.try_query(conn, "GRANT %s TO '%s'@'%%';", r1, pam_user);
        test.try_query(conn, "SET DEFAULT ROLE %s for '%s'@'%%';", r1, pam_user);
        test.try_query(conn, grant_role_fmt, r2, r1);
        test.try_query(conn, grant_role_fmt, r3, r2);
        test.try_query(conn, "GRANT SELECT ON *.* TO '%s';", r3);
        test.try_query(conn, "FLUSH PRIVILEGES;");
        test.repl->sync_slaves();
        update_users();

        // If ok so far, try logging in with PAM.
        if (test.ok())
        {
            cout << "Testing normal PAM user with role-based privileges.\n";
            try_log_in(pam_user, pam_pw, dbname);
            test.log_includes(pam_message_contents.c_str());
        }

        // Remove the created items.
        test.try_query(conn, "DROP USER '%s'@'%%';", pam_user);
        test.try_query(conn, "DROP DATABASE %s;", dbname);
        const char drop_role_fmt[] = "DROP ROLE %s;";
        test.try_query(conn, drop_role_fmt, r1);
        test.try_query(conn, drop_role_fmt, r2);
        test.try_query(conn, drop_role_fmt, r3);
    }

    if (test.ok())
    {
        // Test that normal authentication on the same port works. This tests MXS-2497.
        auto maxconn = test.maxscales->open_rwsplit_connection();
        int port = test.maxscales->rwsplit_port;
        test.try_query(maxconn, "SELECT rand();");
        cout << "Normal mariadb-authentication on port " << port << (test.ok() ? " works.\n" : " failed.\n");
        mysql_close(maxconn);
    }

    // Remove the linux user from the MaxScale node. Required for next test cases.
    test.maxscales->ssh_node_f(0, true, "%s", remove_user_cmd.c_str());

    int normal_port = test.maxscales->rwsplit_port;
    int skip_auth_port = 4007;
    int nomatch_port = 4008;
    int caseless_port = 4009;
    int cleartext_port = 4010;
    int user_map_port = 4011;

    const char login_failed_msg[] = "Login to port %i failed.";
    if (test.ok())
    {
        cout << "\n";
        // Recreate the pam user.
        MYSQL* conn = test.repl->nodes[0];
        test.try_query(conn, create_pam_user_fmt, pam_user, pam_config_name);
        // Normal listener should not work anymore, but the one with skip_authentication should work
        // even with the Linux user removed.

        bool login_success = test_pam_login(test, normal_port, pam_user, pam_pw, "");
        test.expect(!login_success, "Normal login succeeded when it should not have.");

        cout << "Testing listener with skip_authentication.\n";
        login_success = test_pam_login(test, skip_auth_port, pam_user, pam_pw, "");
        test.expect(login_success, login_failed_msg, skip_auth_port);
        if (test.ok())
        {
            cout << "skip_authentication works.\n";
        }
        test.try_query(conn, "DROP USER '%s'@'%%';", pam_user);
    }

    const char create_fmt[] = "CREATE OR REPLACE USER '%s'@'%s' IDENTIFIED BY '%s';";
    if (test.ok())
    {
        cout << "\n";
        // Create a user which can only connect from MaxScale IP. This should work with the listener with
        // authenticator_options=match_host=false.
        string user = "maxhost_user";
        auto userz = user.c_str();
        auto hostz = mxs_ip;
        string pass = "maxhost_pass";
        MYSQL* conn = test.repl->nodes[0];
        test.try_query(conn, create_fmt, userz, hostz, pass.c_str());

        if (test.ok())
        {
            const char unexpected[] = "Login to port %i succeeded when it should have failed.";
            bool login_success = test_normal_login(test, normal_port, user, pass);
            test.expect(!login_success, unexpected, normal_port);
            login_success = test_normal_login(test, skip_auth_port, user, pass);
            test.expect(!login_success, unexpected, normal_port);

            cout << "Testing listener with match_host=false.\n";
            login_success = test_normal_login(test, nomatch_port, user, pass);
            test.expect(login_success, login_failed_msg, normal_port);
            if (test.ok())
            {
                cout << "match_host=false works.\n";
            }
        }
        test.try_query(conn, "DROP USER '%s'@'%s';", userz, hostz);
    }

    if (test.ok())
    {
        // Test lower_case_table_names. Only test the MaxScale-side of authentication, as testing
        // the server is not really the purpose here.
        MYSQL* conn = test.repl->nodes[0];
        string user = "low_case_user";
        string pass = "low_case_pass";
        auto userz = user.c_str();
        const char host[] = "%";
        test.try_query(conn, create_fmt, userz, host, pass.c_str());

        const char create_db_fmt[] = "CREATE OR REPLACE DATABASE %s;";
        const char grant_sel_fmt[] = "GRANT select on %s.* TO '%s'@'%s';";

        const char test_db1[] = "test_db1";
        test.try_query(conn, create_db_fmt, test_db1);
        test.try_query(conn, grant_sel_fmt, test_db1, userz, host);

        const char test_db2[] = "tEsT_db2";
        test.try_query(conn, create_db_fmt, test_db2);
        test.try_query(conn, grant_sel_fmt, test_db2, userz, host);

        auto test_normal_login_short = [&test, &user, pass, mxs_ip](int port, const string& db) {
                auto host = mxs_ip;
                MYSQL* maxconn = nullptr;
                maxconn = open_conn_db(port, host, db, user, pass);
                auto err = mysql_error(maxconn);
                bool rval = (*err == '\0');
                if (*err)
                {
                    test.tprintf("Could not log in: '%s'", err);
                }
                mysql_close(maxconn);
                return rval;
            };

        const string login_db1 = "TeSt_dB1";
        const string login_db2 = "tESt_Db2";
        const char unexpected_login[] = "Login to db %s worked when it should not have.";
        const char failed_login[] = "Login to db %s failed.";

        if (test.ok())
        {
            cout << "\n";
            // Should not work, as requested db is not equal to real db.
            bool login_success = test_normal_login_short(normal_port, login_db1);
            test.expect(!login_success, unexpected_login, login_db1.c_str());

            cout << "Testing listener with lower_case_table_names=1\n";
            // Should work, as the login db is converted to lower case.
            login_success = test_normal_login_short(nomatch_port, login_db1);
            test.expect(login_success, failed_login, login_db1.c_str());
            if (test.ok())
            {
                cout << "lower_case_table_names=1 works.\n";
            }
            cout << "\n";

            // Should not work, as target db is not lower case.
            login_success = test_normal_login_short(nomatch_port, login_db2);
            test.expect(!login_success, unexpected_login, login_db2.c_str());

            cout << "Testing listener with lower_case_table_names=2\n";
            // Should work, as listener compares db names case-insensitive.
            login_success = test_normal_login_short(caseless_port, login_db2);
            test.expect(login_success, failed_login, login_db1.c_str());
            if (test.ok())
            {
                cout << "lower_case_table_names=2 works.\n";
            }
            cout << "\n";

            // Check that log_password_mismatch works.
            login_success = test_normal_login(test, caseless_port, user, "wrong_pw");
            test.expect(!login_success, "Login using wrong password worked when it should not have.");
            test.log_includes("Client gave wrong password. Got hash");
            if (test.ok())
            {
                cout << "log_password_mismatch works.\n";
            }
            cout << "\n";
        }

        test.try_query(conn, "DROP USER '%s'@'%s';", user.c_str(), host);
        test.try_query(conn, "DROP DATABASE %s;", test_db1);
        test.try_query(conn, "DROP DATABASE %s;", test_db2);
    }

    if (test.ok())
    {
        const string setting_name = "pam_use_cleartext_plugin";
        const string setting_val = setting_name + "=1";

        // Helper function for enabling/disabling the setting and checking its value.
        auto alter_setting = [&](int node, bool enable) {
                // disabling end enabling the plugin causes server to reload config file.
                MYSQL* conn = test.repl->nodes[node];
                test.try_query(conn, "%s", uninstall_plugin);
                if (enable)
                {
                    test.repl->stash_server_settings(node);
                    test.repl->add_server_setting(node, setting_val.c_str());
                }
                else
                {
                    test.repl->reset_server_settings(node);
                }
                test.try_query(conn, "%s", install_plugin);

                // Check that the setting is in effect.
                string field_name = "@@" + setting_name;
                string query = "select " + field_name + ";";
                char value[10];
                string expected_value = enable ? "1" : "0";
                if (find_field(conn, query.c_str(), field_name.c_str(), value) == 0)
                {
                    test.expect(value == expected_value, "%s on node %i has value %s when %s expected",
                                field_name.c_str(), node, value, expected_value.c_str());
                }
                else
                {
                    test.expect(false, "Could not read value of %s", field_name.c_str());
                }
            };

        // Test pam_use_cleartext_plugin. Enable the setting on all backends.
        cout << "Enabling " << setting_val << " on all backends.\n";
        for (int i = 0; i < N; i++)
        {
            alter_setting(i, true);
        }

        if (test.ok())
        {
            // The user needs to be recreated on the MaxScale node.
            test.maxscales->ssh_node_f(0, true, "%s", add_user_cmd.c_str());
            test.maxscales->ssh_node_f(0, true, "%s", add_pw_cmd.c_str());

            cout << "Testing listener with " << setting_val << "\n";
            MYSQL* conn = test.repl->nodes[0];
            test.try_query(conn, create_pam_user_fmt, pam_user, "passwd");
            // Try to login with wrong pw to ensure user data is updated.
            sleep(1);
            bool login_success = test_pam_login(test, cleartext_port, "wrong", "wrong", "");
            test.expect(!login_success, "Login succeeded when it should not have.");
            sleep(1);
            login_success = test_pam_login(test, cleartext_port, pam_user, pam_pw, "");
            test.expect(login_success, "Login with %s failed", setting_name.c_str());
            if (test.ok())
            {
                cout << setting_name << " works.\n";
            }
            test.try_query(conn, "DROP USER '%s'@'%%';", pam_user);
        }

        cout << "Disabling " << setting_val << " on all backends.\n";
        for (int i = 0; i < N; i++)
        {
            alter_setting(i, false);
        }
    }

    if (test.ok())
    {
        // Test user account mapping (MXS-3475). For this, the pam_user_map.so-file is required.
        // This file is installed with the server, but not with MaxScale. Depending on distro, the file
        // may be in different places. Check both.
        auto& vm = test.repl->backend(0)->vm_node();
        string lib_source1 = "/usr/lib64/security/pam_user_map.so";
        string lib_source2 = "/usr/lib/security/pam_user_map.so";
        string lib_temp = "/tmp/pam_user_map.so";
        const char pam_map_config_name[] = "pam_config_user_map";

        string pam_map_config_path_src = mxb::string_printf("%s/authentication/%s",
                                                            test_dir, pam_map_config_name);
        string pam_map_config_path_dst = mxb::string_printf("/etc/pam.d/%s", pam_map_config_name);
        string pam_user_map_conf_src = mxb::string_printf("%s/authentication/user_map.conf", test_dir);
        string pam_user_map_conf_dst = "/etc/security/user_map.conf";

        if (vm.copy_from_node(lib_source1, lib_temp) || vm.copy_from_node(lib_source2, lib_temp))
        {
            if (mxs_vm.copy_to_node(lib_temp, lib_temp))
            {
                test.logger().log_msg("pam_user_map.so copied to MaxScale VM.");
            }
            else
            {
                test.add_failure("Failed to copy library '%s' to %s.",
                                 lib_temp.c_str(), mxs_vm.m_name.c_str());
            }
        }
        else
        {
            test.add_failure("Failed to copy library '%s' or '%s' to host machine.",
                             lib_source1.c_str(), lib_source2.c_str());
        }

        mxs_vm.copy_to_node_sudo(pam_map_config_path_src, pam_map_config_path_dst);
        mxs_vm.copy_to_node_sudo(pam_user_map_conf_src, pam_user_map_conf_dst);

        if (test.ok())
        {
            // For this case, it's enough to create the Linux user on the MaxScale VM.
            const char orig_user[] = "orig_pam_user";
            const char orig_pass[] = "orig_pam_pw";
            const string add_orig_user_cmd = (string)"useradd " + orig_user;
            const string add_orig_pw_cmd = mxb::string_printf("echo %s:%s | chpasswd", orig_user, orig_pass);
            mxs_vm.run_cmd_output_sudo(add_orig_user_cmd);
            mxs_vm.run_cmd_sudo(add_orig_pw_cmd);

            auto srv = test.repl->backend(0);
            auto conn = srv->try_open_connection();
            string create_orig_user_query = mxb::string_printf(create_pam_user_fmt,
                                                               orig_user, pam_map_config_name);
            conn->cmd(create_orig_user_query);
            const char mapped_user[] = "mapped_mariadb";
            string create_mapped_user_query = mxb::string_printf("create or replace user '%s'@'%%';",
                                                                 mapped_user);
            conn->cmd(create_mapped_user_query);
            // Try to login with wrong username so MaxScale updates accounts.
            sleep(1);
            bool login_success = test_pam_login(test, user_map_port, "wrong", "wrong", "");
            test.expect(!login_success, "Login succeeded when it should not have.");
            sleep(1);
            bool mapped_login_ok = test_mapped_pam_login(test, user_map_port, orig_user, orig_pass,
                                                         mapped_user);
            test.expect(mapped_login_ok, "Mapped login failed.");

            // Cleanup
            const char drop_user_fmt[] = "DROP USER '%s'@'%%';";
            string drop_orig_user_query = mxb::string_printf(drop_user_fmt, orig_user);
            conn->cmd(drop_orig_user_query);
            string drop_mapped_user_query = mxb::string_printf(drop_user_fmt, mapped_user);
            conn->cmd(drop_mapped_user_query);
            const string remove_orig_user_cmd = (string)"userdel --remove " + orig_user;
            mxs_vm.run_cmd_output_sudo(remove_orig_user_cmd);
        }

        // Delete the library file from both the tester VM and MaxScale VM.
        string del_pam_lib_cmd = "rm -f " + lib_temp;
        system(del_pam_lib_cmd.c_str());
        mxs_vm.run_cmd_output_sudo(del_pam_lib_cmd);

        // Delete config files from MaxScale VM.
        const string delete_pam_map_conf_cmd = "rm -f " + pam_map_config_path_dst;
        const string delete_pam_user_map_conf_cmd = "rm -f " + pam_user_map_conf_dst;
        mxs_vm.run_cmd_output_sudo(delete_pam_map_conf_cmd);
        mxs_vm.run_cmd_output_sudo(delete_pam_user_map_conf_cmd);
    }

    test.tprintf("Test complete. Cleaning up.");
    // Cleanup: remove linux user and files from the MaxScale node.
    mxs_vm.run_cmd_sudo(remove_user_cmd);
    mxs_vm.run_cmd_sudo(read_shadow_off);
    mxs_vm.run_cmd_sudo(delete_pam_conf_cmd);
    mxs_vm.run_cmd_sudo(delete_pam_message_cmd);

    // Cleanup: remove the linux users on the backends, unload pam plugin.
    for (int i = 0; i < N; i++)
    {
        MYSQL* conn = test.repl->nodes[i];
        test.try_query(conn, "UNINSTALL SONAME 'auth_pam';");
        auto& vm = test.repl->backend(i)->vm_node();
        vm.run_cmd_sudo(remove_user_cmd);
        vm.run_cmd_sudo(read_shadow_off);
        vm.run_cmd_sudo(delete_pam_conf_cmd);
        vm.run_cmd_sudo(delete_pam_message_cmd);
    }

    test.repl->disconnect();
}


// Helper function for checking PAM-login. If db is empty, log to null database.
MYSQL* pam_login(TestConnections& test, int port, const string& user, const string& pass,
                 const string& database)
{
    const char* host = test.maxscales->ip4();
    const char* db = nullptr;
    if (!database.empty())
    {
        db = database.c_str();
    }

    if (db)
    {
        printf("Trying to log in to [%s]:%i as %s with database %s.\n", host, port, user.c_str(), db);
    }
    else
    {
        printf("Trying to log in to [%s]:%i as %s.\n", host, port, user.c_str());
    }

    MYSQL* rval = nullptr;
    MYSQL* maxconn = mysql_init(NULL);
    // Need to set plugin directory so that dialog.so is found.
    mysql_optionsv(maxconn, MYSQL_PLUGIN_DIR, plugin_path.c_str());
    mysql_real_connect(maxconn, host, user.c_str(), pass.c_str(), db, port, NULL, 0);
    auto err = mysql_error(maxconn);
    if (*err)
    {
        test.tprintf("Could not log in: '%s'", err);
        mysql_close(maxconn);
    }
    else
    {
        rval = maxconn;
    }
    return rval;
}

bool test_pam_login(TestConnections& test, int port, const string& user, const string& pass,
                    const string& database)
{
    bool rval = false;
    auto maxconn = pam_login(test, port, user, pass, database);
    if (maxconn)
    {
        if (execute_query_silent(maxconn, "SELECT rand();") == 0)
        {
            rval = true;
            cout << "Logged in and queried successfully.\n";
        }
        else
        {
            cout << "Query rejected: '" << mysql_error(maxconn) << "'\n";
        }
    }
    return rval;
}

bool test_mapped_pam_login(TestConnections& test, int port, const string& user, const string& pass,
                           const string& expected_user)
{
    bool rval = false;
    auto maxconn = pam_login(test, port, user, pass, "");
    if (maxconn)
    {
        auto res = get_result(maxconn, "select user();");
        if (!res.empty())
        {
            string effective_user = res[0][0];
            effective_user = mxt::cutoff_string(effective_user, '@');
            if (effective_user == expected_user)
            {
                test.tprintf("Logged in. Mapped user is '%s', as expected.", effective_user.c_str());
                rval = true;
            }
            else
            {
                test.tprintf("User '%s' mapped to '%s' when '%s' was expected.",
                             user.c_str(), effective_user.c_str(), expected_user.c_str());
            }
        }
        else
        {
            cout << "Query rejected: '" << mysql_error(maxconn) << "'\n";
        }
    }
    return rval;
}

bool test_normal_login(TestConnections& test, int port, const string& user, const string& pass,
                       const string& db)
{
    bool rval = false;
    auto host = test.maxscales->ip4();
    MYSQL* maxconn = nullptr;
    if (db.empty())
    {
        maxconn = open_conn_no_db(port, host, user, pass);
    }
    else
    {
        maxconn = open_conn_db(port, host, db, user, pass);
    }

    auto err = mysql_error(maxconn);
    if (*err)
    {
        test.tprintf("Could not log in: '%s'", err);
    }
    else
    {
        if (execute_query_silent(maxconn, "SELECT rand();") == 0)
        {
            rval = true;
            cout << "Logged in and queried successfully.\n";
        }
        else
        {
            cout << "Query rejected: '" << mysql_error(maxconn) << "'\n";
        }
    }
    mysql_close(maxconn);
    return rval;
}
