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

#include <maxscale/ccdefs.hh>

#include <string>
#include <unordered_map>
#include <microhttpd.h>

#include "httprequest.hh"
#include "httpresponse.hh"
#include "websocket.hh"

class Client
{
public:
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    using Headers = std::unordered_map<std::string, std::string>;

    /**
     * @brief Create a new client
     *
     * @param connection The connection handle for this client
     */
    Client(MHD_Connection* connection, const char* url, const char* method);

    // Handle HTTP request
    int handle(const std::string& url, const std::string& method,
               const char* upload_data, size_t* upload_data_size);

private:
    enum state
    {
        OK,
        FAILED,
        INIT,
        CLOSED
    };

    /**
     * @brief Process a client request
     *
     * This function can be called multiple times if a PUT/POST/PATCH
     * uploads a large amount of data.
     *
     * @param url    Requested URL
     * @param method Request method
     * @param data   Pointer to request data
     * @param size   Size of request data
     *
     * @return MHD_YES on success, MHD_NO on error
     */
    int process(std::string url, std::string method, const char* data, size_t* size);

    /**
     * @brief Authenticate the client
     *
     * @param connection The MHD connection object
     * @param url        Requested URL
     * @param method     The request method
     *
     * @return True if authentication was successful
     */
    bool auth(MHD_Connection* connection, const char* url, const char* method);

    /**
     * Get client state
     *
     * @return The client state
     */
    state get_state() const
    {
        return m_state;
    }

    /**
     * Close the client connection
     *
     * All further requests will be rejected immediately
     */
    void close()
    {
        m_state = CLOSED;
    }

    /**
     * Serve a file that the client is requesting
     *
     * @param url   The URL the client is requesting
     *
     * @return True if a file was served and the processing should stop
     */
    bool serve_file(const std::string& url) const;

    MHD_Connection* m_connection;   /**< Connection handle */
    std::string     m_data;         /**< Uploaded data */
    state           m_state;        /**< Client state */
    std::string     m_user;         /**< The user account */
    Headers         m_headers;
    HttpRequest     m_request;

    WebSocket::Handler m_ws_handler;

    HttpResponse generate_token(const HttpRequest& request);
    bool         auth_with_token(const std::string& token);
    bool         authorize_user(const char* user, const char* method, const char* url) const;
    bool         is_basic_endpoint() const;
    bool         send_cors_preflight_request(const std::string& verb);
    std::string  get_header(const std::string& key) const;
    size_t       request_data_length() const;
    void         send_shutting_down_error() const;
    void         send_basic_auth_error() const;
    void         send_token_auth_error() const;
    void         send_write_access_error() const;
    void         send_no_https_error() const;
    void         add_cors_headers(MHD_Response*) const;
    void         upgrade_to_ws();
    int          queue_response(const HttpResponse& response);
    int          queue_delayed_response(const HttpResponse::Callback& cb);

    static void handle_ws_upgrade(void* cls, MHD_Connection* connection, void* con_cls,
                                  const char* extra_in, size_t extra_in_size,
                                  int socket, MHD_UpgradeResponseHandle* urh);
};

/**
 * @brief Start the administrative interface
 *
 * @return True if the interface was successfully started
 */
bool mxs_admin_init();

/**
 * @brief Shutdown the administrative interface
 *
 * This stops the REST API from accepting new requests but it will still allow existing requests to complete.
 * All connection attempts will be rejected with a HTTP 503 error stating that MaxScale is shutting down.
 */
void mxs_admin_shutdown();

/**
 * @brief Stop the administrative interface
 */
void mxs_admin_finish();

/**
 * @brief Check if admin interface uses HTTPS protocol
 *
 * @return True if HTTPS is enabled
 */
bool mxs_admin_https_enabled();

/**
 * @brief Enable CORS support
 *
 * CORS support allows browsers to access the REST API without MaxScale being the origin. There is no
 * validation of the headers which means this is meant only for testing purposes.
 */
bool mxs_admin_enable_cors();
