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

#include <atomic>
#include <deque>
#include <list>
#include <mutex>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <functional>

#include <maxbase/atomic.hh>
#include <maxbase/semaphore.hh>
#include <maxbase/stopwatch.hh>
#include <maxbase/watchedworker.hh>
#include <maxscale/dcb.hh>
#include <maxscale/indexedstorage.hh>
#include <maxscale/poll.hh>
#include <maxscale/query_classifier.hh>
#include <maxscale/session.hh>

MXS_BEGIN_DECLS

// The worker ID of the "main" thread
#define MXS_RWORKER_MAIN -1

/**
 * Return the routing worker associated with the provided worker id.
 *
 * @param worker_id  A worker id. If MXS_RWORKER_MAIN is used, the
 *                   routing worker running in the main thread will
 *                   be returned.
 *
 * @return The corresponding routing worker instance, or NULL if the
 *         id does not correspond to a routing worker.
 */
MXB_WORKER* mxs_rworker_get(int worker_id);

/**
 * Return the id of the current routing worker.
 *
 * @return The id of the routing worker, or -1 if there is no current
 *         routing worker.
 */
int mxs_rworker_get_current_id();


MXS_END_DECLS

class ServerEndpoint;

namespace maxscale
{

class RoutingWorker : public mxb::WatchedWorker
                    , public BackendDCB::Manager
                    , private MXB_POLL_DATA
{
    RoutingWorker(const RoutingWorker&) = delete;
    RoutingWorker& operator=(const RoutingWorker&) = delete;

public:
    enum
    {
        MAIN = -1
    };

    typedef mxs::Registry<MXS_SESSION> SessionsById;
    typedef std::vector<DCB*>          Zombies;

    typedef std::vector<void*>           LocalData;
    typedef std::vector<void (*)(void*)> DataDeleters;

    /**
     * Initialize the routing worker mechanism.
     *
     * To be called once at process startup. This will cause as many workers
     * to be created as the number of threads defined.
     *
     * @param pNotifier  The watchdog notifier. Must remain alive for the
     *                   lifetime of the routing worker.
     *
     * @return True if the initialization succeeded, false otherwise.
     */
    static bool init(mxb::WatchdogNotifier* pNotifier);

    /**
     * Finalize the worker mechanism.
     *
     * To be called once at process shutdown. This will cause all workers
     * to be destroyed. When the function is called, no worker should be
     * running anymore.
     */
    static void finish();

    /**
     * Add a file descriptor to the epoll instance shared between all workers.
     * Events occuring on the provided file descriptor will be handled by all
     * workers. This is primarily intended for listening sockets where the
     * only event is EPOLLIN, signaling that accept() can be used on the listening
     * socket for creating a connected socket to a client.
     *
     * @param fd      The file descriptor to be added.
     * @param events  Mask of epoll event types.
     * @param pData   The poll data associated with the descriptor:
     *
     *                  data->handler  : Handler that knows how to deal with events
     *                                   for this particular type of 'struct mxs_poll_data'.
     *                  data->thread.id: 0
     *
     * @return True, if the descriptor could be added, false otherwise.
     */
    static bool add_shared_fd(int fd, uint32_t events, MXB_POLL_DATA* pData);

    /**
     * Remove a file descriptor from the epoll instance shared between all workers.
     *
     * @param fd  The file descriptor to be removed.
     *
     * @return True on success, false on failure.
     */
    static bool remove_shared_fd(int fd);

    /**
     * Returns the id of the routing worker
     *
     * @return The id of the routing worker.
     */
    int id() const override
    {
        return m_id;
    }

    /**
     * Return a reference to the session registry of this worker.
     *
     * @return Session registry.
     */
    SessionsById& session_registry();

    /**
     * Add a session to the current routing worker's session container.
     *
     * @param ses Session to add.
     */
    void register_session(MXS_SESSION* ses);

    /**
     * Remove a session from the current routing worker's session container.
     *
     * @param id Which id to remove
     */
    void deregister_session(uint64_t session_id);

    /**
     * Return the worker associated with the provided worker id.
     *
     * @param worker_id  A worker id. By specifying MAIN, the routing worker
     *                   running in the main thread will be returned.
     *
     * @return The corresponding worker instance, or NULL if the id does
     *         not correspond to a worker.
     */
    static RoutingWorker* get(int worker_id);

    /**
     * Return the worker associated with the current thread.
     *
     * @return The worker instance, or NULL if the current thread does not have a worker.
     */
    static RoutingWorker* get_current();

    /**
     * Return the worker id associated with the current thread.
     *
     * @return A worker instance, or -1 if the current thread does not have a worker.
     */
    static int get_current_id();

    /**
     * Starts all routing workers.
     *
     * @return True, if all workers could be started.
     */
    static bool start_workers();

    /**
     * Returns whether worker threads are running
     *
     * @return True if worker threads are running
     */
    static bool is_running();

    /**
     * Waits for all routing workers.
     */
    static void join_workers();

    /**
     * Check if all workers have finished shutting down
     */
    static bool shutdown_complete();

    /**
     * Posts a task to all workers for execution.
     *
     * @param pTask  The task to be executed.
     * @param pSem   If non-NULL, will be posted once per worker when the task's
     *               `execute` return.
     *
     * @return How many workers the task was posted to.
     *
     * @attention The very same task will be posted to all workers. The task
     *            should either not have any sharable data or then it should
     *            have data specific to each worker that can be accessed
     *            without locks.
     *
     * @attention The task will be posted to each routing worker using the
     *            EXECUTE_AUTO execution mode. That is, if the calling thread
     *            is that of a routing worker, then the task will be executed
     *            directly without going through the message loop of the worker,
     *            otherwise the task is delivered via the message loop.
     */
    static size_t broadcast(Task* pTask, mxb::Semaphore* pSem = NULL);

    /**
     * Posts a task to all workers for execution.
     *
     * @param pTask  The task to be executed.
     *
     * @return How many workers the task was posted to.
     *
     * @attention The very same task will be posted to all workers. The task
     *            should either not have any sharable data or then it should
     *            have data specific to each worker that can be accessed
     *            without locks.
     *
     * @attention Once the task has been executed by all workers, it will
     *            be deleted.
     *
     * @attention The task will be posted to each routing worker using the
     *            EXECUTE_AUTO execution mode. That is, if the calling thread
     *            is that of a routing worker, then the task will be executed
     *            directly without going through the message loop of the worker,
     *            otherwise the task is delivered via the message loop.
     */
    static size_t broadcast(std::unique_ptr<DisposableTask> sTask);

    /**
     * Posts a function to all workers for execution.
     *
     * @param pSem If non-NULL, will be posted once the task's `execute` return.
     * @param mode Execution mode
     *
     * @return How many workers the task was posted to.
     */
    static size_t broadcast(const std::function<void ()>& func, mxb::Semaphore* pSem, execute_mode_t mode);

    static size_t broadcast(const std::function<void ()>& func, enum execute_mode_t mode)
    {
        return broadcast(func, NULL, mode);
    }

    /**
     * Executes a task on all workers in serial mode (the task is executed
     * on at most one worker thread at a time). When the function returns
     * the task has been executed on all workers.
     *
     * @param task  The task to be executed.
     *
     * @return How many workers the task was posted to.
     *
     * @warning This function is extremely inefficient and will be slow compared
     * to the other functions. Only use this function when printing thread-specific
     * data to stdout.
     *
     * @attention The task will be posted to each routing worker using the
     *            EXECUTE_AUTO execution mode. That is, if the calling thread
     *            is that of a routing worker, then the task will be executed
     *            directly without going through the message loop of the worker,
     *            otherwise the task is delivered via the message loop.
     */
    static size_t execute_serially(Task& task);
    static size_t execute_serially(const std::function<void()>& func);

    /**
     * Executes a task on all workers concurrently and waits until all workers
     * are done. That is, when the function returns the task has been executed
     * by all workers.
     *
     * @param task  The task to be executed.
     *
     * @return How many workers the task was posted to.
     *
     * @attention The task will be posted to each routing worker using the
     *            EXECUTE_AUTO execution mode. That is, if the calling thread
     *            is that of a routing worker, then the task will be executed
     *            directly without going through the message loop of the worker,
     *            otherwise the task is delivered via the message loop.
     */
    static size_t execute_concurrently(Task& task);
    static size_t execute_concurrently(const std::function<void()>& func);

    /**
     * Broadcast a message to all worker.
     *
     * @param msg_id  The message id.
     * @param arg1    Message specific first argument.
     * @param arg2    Message specific second argument.
     *
     * @return The number of messages posted; if less that ne number of workers
     *         then some postings failed.
     *
     * @attention The return value tells *only* whether message could be posted,
     *            *not* that it has reached the worker.
     *
     * @attentsion Exactly the same arguments are passed to all workers. Take that
     *             into account if the passed data must be freed.
     *
     * @attention This function is signal safe.
     */
    static size_t broadcast_message(uint32_t msg_id, intptr_t arg1, intptr_t arg2);

    /**
     * Initate shutdown of all workers.
     *
     * @attention A call to this function will only initiate the shutdowm,
     *            the workers will not have shut down when the function returns.
     *
     * @attention This function is signal safe.
     */
    static void shutdown_all();

    /**
     * Returns statistics for all workers.
     *
     * @return Combined statistics.
     *
     * @attentions The statistics may no longer be accurate by the time it has
     *             been returned. The returned values may also not represent a
     *             100% consistent set.
     */
    static STATISTICS get_statistics();

    /**
     * Return a specific combined statistic value.
     *
     * @param what  What to return.
     *
     * @return The corresponding value.
     */
    static int64_t get_one_statistic(POLL_STAT what);

    /**
     * Get next worker
     *
     * @return The worker where work should be assigned
     */
    static RoutingWorker* pick_worker();

    /**
     * Provides QC statistics of one workers
     *
     * @param id[in]       Id of worker.
     * @param pStats[out]  The QC statistics of that worker.
     *
     * return True, if @c id referred to a worker, false otherwise.
     */
    static bool get_qc_stats(int id, QC_CACHE_STATS* pStats);

    /**
     * Provides QC statistics of all workers
     *
     * @param all_stats  Vector that on return will contain the statistics of all workers.
     */
    static void get_qc_stats(std::vector<QC_CACHE_STATS>& all_stats);

    /**
     * Provides QC statistics of all workers as a Json object for use in the REST-API.
     */
    static std::unique_ptr<json_t> get_qc_stats_as_json(const char* zHost);

    /**
     * Provides QC statistics of one worker as a Json object for use in the REST-API.
     *
     * @param zHost  The name of the MaxScale host.
     * @param id     An id of a worker.
     *
     * @return A json object if @c id refers to a worker, NULL otherwise.
     */
    static std::unique_ptr<json_t> get_qc_stats_as_json(const char* zHost, int id);

    using DCBs = std::unordered_set<DCB*>;
    /**
     * Access all DCBs of the routing worker.
     *
     * @attn Must only be called from worker thread.
     *
     * @return Unordered set of DCBs.
     */
    const DCBs& dcbs() const
    {
        mxb_assert(this == RoutingWorker::get_current());
        return m_dcbs;
    }

    struct ConnectionResult
    {
        bool                    conn_limit_reached {false};
        mxs::BackendConnection* conn {nullptr};
    };

    ConnectionResult
    get_backend_connection(SERVER* pSrv, MXS_SESSION* pSes, mxs::Component* pUpstream);

    mxs::BackendConnection* pool_get_connection(SERVER* pSrv, MXS_SESSION* pSes, mxs::Component* pUpstream);

    void pool_close_all_conns();
    void pool_close_all_conns_by_server(SERVER* pSrv);

    void add_conn_wait_entry(ServerEndpoint* ep);
    void erase_conn_wait_entry(ServerEndpoint* ep);
    void notify_connection_available(SERVER* server);
    bool conn_to_server_needed(const SERVER* srv) const;

    static void pool_set_size(const std::string& srvname, int64_t size);

    struct ConnectionPoolStats
    {
        size_t curr_size {0};   /**< Current pool size */
        size_t max_size {0};    /**< Maximum pool size achieved since startup */
        size_t times_empty {0}; /**< Times the current pool was empty */
        size_t times_found {0}; /**< Times when a connection was available from the pool */

        void add(const ConnectionPoolStats& rhs);
    };
    static ConnectionPoolStats pool_get_stats(const SERVER* pSrv);

    ConnectionPoolStats pool_stats(const SERVER* pSrv);

    /**
     * Register a function to be called every epoll_tick.
     */
    void register_epoll_tick_func(std::function<void(void)> func);

    /**
     * @return The indexed storage of this worker.
     */
    IndexedStorage& storage()
    {
        return m_storage;
    }

    const IndexedStorage& storage() const
    {
        return m_storage;
    }

    static void collect_worker_load(size_t count);

    static bool balance_workers();

    static bool balance_workers(int threshold);

    void rebalance(RoutingWorker* pTo, int nSessions = 1);

    /**
     * Start the routingworker shutdown process
     */
    static void start_shutdown();

private:
    // DCB::Manager
    void add(DCB* pDcb) override;
    void remove(DCB* pDcb) override;
    void destroy(DCB* pDcb) override;
    // BackendDCB::Manager
    bool move_to_conn_pool(BackendDCB* dcb) override;

    void evict_dcb(BackendDCB* pDcb);
    void close_pooled_dcb(BackendDCB* pDcb);

    bool try_shutdown(Call::action_t action);

private:
    const int      m_id;        /*< The id of the worker. */
    SessionsById   m_sessions;  /*< A mapping of session_id->MXS_SESSION */
    Zombies        m_zombies;   /*< DCBs to be deleted. */
    IndexedStorage m_storage;   /*< The storage of this worker. */
    DCBs           m_dcbs;      /*< DCBs managed by this worker. */

    struct
    {
        RoutingWorker* pTo {nullptr};   /*< Worker to offload work to. */
        bool           perform = false;
        int            nSessions = 0;

        void set(RoutingWorker* pTo, int nSessions)
        {
            this->pTo = pTo;
            this->nSessions = nSessions;
            this->perform = true;
        }

        void reset()
        {
            pTo = nullptr;
            perform = false;
            nSessions = 0;
        }
    } m_rebalance;

    RoutingWorker(mxb::WatchdogNotifier* pNotifier);
    virtual ~RoutingWorker();

    static RoutingWorker* create(mxb::WatchdogNotifier* pNotifier, int epoll_listener_fd);

    bool pre_run() override;
    void post_run() override;
    void epoll_tick() override;

    void process_timeouts();
    void delete_zombies();
    void rebalance();
    void pool_close_expired();

    void activate_waiting_endpoints();
    void fail_timed_out_endpoints();

    static uint32_t epoll_instance_handler(MXB_POLL_DATA* data, MXB_WORKER* worker, uint32_t events);
    uint32_t        handle_epoll_events(uint32_t events);

    class ConnPoolEntry
    {
    public:
        explicit ConnPoolEntry(mxs::BackendConnection* pConn);
        ~ConnPoolEntry();

        ConnPoolEntry(ConnPoolEntry&& rhs)
            : m_created(rhs.m_created)
            , m_pConn(rhs.release_conn())
        {
        }

        bool hanged_up() const
        {
            return m_pConn->dcb()->hanged_up();
        }

        time_t created() const
        {
            return m_created;
        }

        mxs::BackendConnection* conn() const
        {
            return m_pConn;
        }

        mxs::BackendConnection* release_conn()
        {
            mxs::BackendConnection* pConn = m_pConn;
            m_pConn = nullptr;
            return pConn;
        }

    private:
        time_t                  m_created;  /*< Time when entry was created. */
        mxs::BackendConnection* m_pConn {nullptr};
    };

    class DCBHandler : public DCB::Handler
    {
    public:
        DCBHandler(const DCBHandler&) = delete;
        DCBHandler& operator=(const DCBHandler&) = delete;

        DCBHandler(RoutingWorker* pOwner);

        void ready_for_reading(DCB* pDcb) override;
        void write_ready(DCB* pDcb) override;
        void error(DCB* dcb) override;
        void hangup(DCB* dcb) override;

    private:
        RoutingWorker& m_owner;
    };

    class ConnectionPool
    {
    public:
        ConnectionPool(mxs::RoutingWorker* owner, SERVER* target_server, int global_capacity);
        ConnectionPool(ConnectionPool&& rhs);

        void remove_and_close(mxs::BackendConnection* conn);
        void close_expired();
        void close_all();
        bool empty() const;
        bool has_space() const;
        void set_capacity(int global_capacity);

        ConnectionPoolStats stats() const;

        mxs::BackendConnection* get_connection(MXS_SESSION* session);
        void                    add_connection(mxs::BackendConnection* conn);

    private:
        std::map<mxs::BackendConnection*, ConnPoolEntry> m_contents;

        mxs::RoutingWorker*         m_owner {nullptr};
        SERVER*                     m_target_server {nullptr};
        int                         m_capacity {0}; // Capacity for this pool.
        mutable ConnectionPoolStats m_stats;
    };
    using ConnPoolGroup = std::map<const SERVER*, ConnectionPool>;

    // Protects the connection pool. This is only contended when the REST API asks for statistics on the
    // connection pool and accessing it directly is significantly faster than waiting for the worker to finish
    // their current work and post the results.
    mutable std::mutex m_pool_lock;

    ConnPoolGroup m_pool_group;     /**< Pooled connections for each server */

    using EndpointsBySrv = std::map<const SERVER*, std::deque<ServerEndpoint*>>;

    /** Has a ServerEndpoint activation round been scheduled already? Used to avoid adding multiple identical
     * delayed calls. */
    bool           m_ep_activation_scheduled {false};
    EndpointsBySrv m_eps_waiting_for_conn;      /**< ServerEndpoints waiting for a connection */

    DCBHandler m_pool_handler;
    long       m_next_timeout_check {0};

    std::vector<std::function<void()>> m_epoll_tick_funcs;
};
}

/**
 * @brief Convert a routing worker to JSON format
 *
 * @param host Hostname of this server
 * @param id   ID of the worker
 *
 * @return JSON resource representing the worker
 */
json_t* mxs_rworker_to_json(const char* host, int id);

/**
 * Convert routing workers into JSON format
 *
 * @param host Hostname of this server
 *
 * @return A JSON resource collection of workers
 *
 * @see mxs_json_resource()
 */
json_t* mxs_rworker_list_to_json(const char* host);

/**
 * @brief MaxScale worker watchdog
 *
 * If this function returns, then MaxScale is alive. If not,
 * then some thread is dead.
 */
void mxs_rworker_watchdog();
