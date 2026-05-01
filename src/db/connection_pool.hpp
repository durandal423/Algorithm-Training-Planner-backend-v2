#pragma once

#include "config.hpp"

#include <pqxx/pqxx>

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

class ConnectionPool;

class ConnectionLease final {
public:
    ConnectionLease() noexcept = default;

    ConnectionLease(const ConnectionLease&) = delete;
    ConnectionLease& operator=(const ConnectionLease&) = delete;

    ConnectionLease(ConnectionLease&& other) noexcept;
    ConnectionLease& operator=(ConnectionLease&& other) noexcept;

    ~ConnectionLease();

    pqxx::connection& connection() const;

    explicit operator bool() const noexcept {
        return connection_ != nullptr;
    }

private:
    friend class ConnectionPool;

    ConnectionLease(ConnectionPool& pool,
                    std::unique_ptr<pqxx::connection> connection) noexcept;

    void reset() noexcept;

private:
    ConnectionPool* pool_{nullptr};
    std::unique_ptr<pqxx::connection> connection_;
};

class ConnectionPool final {
public:
    using Clock = std::chrono::steady_clock;
    using Duration = Clock::duration;

    explicit ConnectionPool(
        std::string conninfo,
        std::size_t max_size = config::CONNECTION_POOL_MAX_SIZE,
        Duration idle_timeout = std::chrono::minutes{
            config::CONNECTION_POOL_IDLE_TIMEOUT_MINUTES
        }
    );

    ConnectionPool(const ConnectionPool&) = delete;
    ConnectionPool& operator=(const ConnectionPool&) = delete;

    ConnectionLease acquire();

private:
    friend class ConnectionLease;

    class IdleConnection final {
    public:
        explicit IdleConnection(
            std::unique_ptr<pqxx::connection> connection
        ) noexcept;

        IdleConnection(const IdleConnection&) = delete;
        IdleConnection& operator=(const IdleConnection&) = delete;

        IdleConnection(IdleConnection&&) noexcept = default;
        IdleConnection& operator=(IdleConnection&&) noexcept = default;

        bool expired(Duration timeout) const noexcept;

        std::unique_ptr<pqxx::connection> take() noexcept;

    private:
        std::unique_ptr<pqxx::connection> connection_;
        Clock::time_point returned_at_;
    };

private:
    std::unique_ptr<pqxx::connection> createConnection() const;

    void release(std::unique_ptr<pqxx::connection> connection) noexcept;

    bool isConnectionAlive(
        const std::unique_ptr<pqxx::connection>& connection
    ) const noexcept;

    // 调用前必须已经持有 mutex_
    std::unique_ptr<pqxx::connection> takeIdleConnectionLocked();

    // 调用前必须已经持有 mutex_
    void discardOneConnectionLocked() noexcept;

private:
    std::string conninfo_;
    std::size_t max_size_;
    Duration idle_timeout_;

    // 当前池管理的连接总数：
    // = 已借出的连接数 + idle_ 里的空闲连接数
    std::size_t total_connections_{0};

    std::vector<IdleConnection> idle_;

    std::mutex mutex_;
    std::condition_variable available_cv_;
};