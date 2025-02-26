/*
 * Copyright (c) 2020 MariaDB Corporation Ab
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

#include "nosqlprotocol.hh"
#include "nosqlbase.hh"
#include <memory>
#include <unordered_map>
#include <maxbase/json.hh>
#include <maxscale/sqlite3.h>
#include "nosqlscram.hh"

namespace nosql
{

namespace role
{

enum Id
{
#define NOSQL_ROLE(id, value, name) id = value,
#include "nosqlrole.hh"
#undef NOSQL_ROLE
};

struct Role
{
    std::string db;
    Id          id;
};

std::unordered_map<std::string, uint32_t> to_bitmasks(const std::vector<Role>& roles);

std::string to_string(Id id);

bool from_string(const std::string& key, Id* pId);

inline bool from_string(const char* zKey, Id* pValue)
{
    return from_string(std::string(zKey), pValue);
}

inline bool from_string(const string_view& key, Id* pValue)
{
    return from_string(std::string(key.data(), key.length()), pValue);
}

std::string to_json(const Role& role);

bool from_json(const mxb::Json& json, Role* pRole);
bool from_json(const std::string& json, Role* pRole);

std::string to_json(const std::vector<Role>& roles);

bool from_json(const std::string& json, std::vector<Role>* pRoles);

// throws if invalid.
void from_bson(const bsoncxx::array::view& bson,
               const std::string& default_db,
               std::vector<Role>* pRoles);

}

class UserManager
{
public:
    ~UserManager();

    class UserInfo
    {
    public:
        std::string                   mariadb_user;
        std::string                   db;
        std::string                   user;
        std::string                   pwd_sha1_b64;
        std::string                   host;
        std::string                   uuid;
        std::string                   custom_data; // JSON document
        std::string                   salt_sha1_b64;
        std::string                   salt_sha256_b64;
        std::string                   salted_pwd_sha1_b64;
        std::string                   salted_pwd_sha256_b64;
        std::vector<scram::Mechanism> mechanisms;
        std::vector<role::Role>       roles;

        std::vector<uint8_t> pwd_sha1() const;
        std::vector<uint8_t> salt_sha1() const;
        std::vector<uint8_t> salt_sha256() const;
        std::vector<uint8_t> salted_pwd_sha1() const;
        std::vector<uint8_t> salted_pwd_sha256() const;

        std::vector<uint8_t> salt(scram::Mechanism mechanism) const
        {
            return mechanism == scram::Mechanism::SHA_1 ? this->salt_sha1() : this->salt_sha256();
        }

        std::string salt_b64(scram::Mechanism mechanism) const
        {
            return mechanism == scram::Mechanism::SHA_1 ? this->salt_sha1_b64 : this->salt_sha256_b64;
        }

        std::vector<uint8_t> salted_pwd(scram::Mechanism mechanism) const
        {
            return mechanism == scram::Mechanism::SHA_1 ? this->salted_pwd_sha1() : this->salted_pwd_sha256();
        }

        std::string salted_pwd_b64(scram::Mechanism mechanism) const
        {
            return mechanism == scram::Mechanism::SHA_1
                ? this->salted_pwd_sha1_b64
                : this->salted_pwd_sha256_b64;
        }
    };

    static std::unique_ptr<UserManager> create(const std::string& name);

    const std::string& path() const
    {
        return m_path;
    }

    bool add_user(const std::string& db,
                  std::string user,
                  std::string password, // Cleartext
                  const std::string& host,
                  const std::string& custom_data, // Assumed to be JSON document.
                  const std::vector<scram::Mechanism>& mechanisms,
                  const std::vector<role::Role>& roles);

    bool remove_user(const std::string& db, const std::string& user);

    bool get_info(const std::string& db, const std::string& user, UserInfo* pInfo) const;

    bool get_info(const std::string& mariadb_user, UserInfo* pInfo) const;

    bool get_info(const string_view& mariadb_user, UserInfo* pInfo) const
    {
        return get_info(std::string(mariadb_user.data(), mariadb_user.length()), pInfo);
    }

    bool user_exists(const std::string& db, const std::string& user) const
    {
        return get_info(db, user, nullptr);
    }

    bool user_exists(const std::string& db, const string_view& user) const
    {
        return get_info(db, std::string(user.data(), user.length()), nullptr);
    }

    bool user_exists(const std::string& mariadb_user) const
    {
        return get_info(mariadb_user, nullptr);
    }

    bool user_exists(const string_view& mariadb_user) const
    {
        return get_info(std::string(mariadb_user.data(), mariadb_user.length()), nullptr);
    }

    std::vector<UserInfo> get_infos() const;

    std::vector<UserInfo> get_infos(const std::string& db) const;

    std::vector<UserInfo> get_infos(const std::vector<std::string>& mariadb_users) const;

    struct Account
    {
        std::string mariadb_user;
        std::string user;
        std::string db;
        std::string host;
    };

    bool get_account(const std::string& db, const std::string& user, Account* pAccount)
    {
        UserInfo info;
        bool rv = get_info(db, user, &info);

        if (rv)
        {
            pAccount->mariadb_user = info.mariadb_user;
            pAccount->user = info.user;
            pAccount->db = info.db;
            pAccount->host = info.host;
        }

        return rv;
    }

    std::vector<Account> get_accounts(const std::string& db) const;

    bool remove_accounts(const std::vector<Account>& accounts) const;

    struct Update
    {
        enum What
        {
            CUSTOM_DATA = 1 << 0,
            MECHANISMS  = 1 << 1,
            PWD         = 1 << 2,
            ROLES       = 1 << 3,

            MASK = (PWD | MECHANISMS | ROLES | CUSTOM_DATA)
        };

        std::string                   pwd;
        std::string                   custom_data;
        std::vector<scram::Mechanism> mechanisms;
        std::vector<role::Role>       roles;
    };

    bool update(const std::string& db, const std::string& user, uint32_t what, const Update& data) const;

    bool set_mechanisms(const std::string& db,
                        const std::string& user,
                        const std::vector<scram::Mechanism>& mechanisms) const
    {
        Update data;
        data.mechanisms = mechanisms;

        return update(db, user, Update::MECHANISMS, data);
    }

    bool set_roles(const std::string& db, const std::string& user, const std::vector<role::Role>& roles) const
    {
        Update data;
        data.roles = roles;

        return update(db, user, Update::ROLES, data);
    }

    static std::string get_mariadb_user(const std::string& db, const std::string& user)
    {
        return db + "." + user;
    }

    static std::string get_mariadb_user(const std::string& db, const string_view& user)
    {
        return db + "." + std::string(user.data(), user.length());
    }

private:
    UserManager(std::string path, sqlite3* pDb);

    std::string m_path;
    sqlite3&    m_db;
};

}
