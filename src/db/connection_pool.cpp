#include "db/connection_pool.hpp"

#include <spdlog/spdlog.h>

#include <stdexcept>
#include <utility>

// =====================
// ConnectionLease
// =====================

ConnectionLease::ConnectionLease(ConnectionLease&& other) noexcept
    : pool_(other.pool_),
      connection_(std::move(other.connection_)) {
    other.pool_ = nullptr;
}

ConnectionLease& ConnectionLease::operator=(ConnectionLease&& other) noexcept {
    if (this != &other) {
        reset();

        pool_ = other.pool_;
        connection_ = std::move(other.connection_);

        other.pool_ = nullptr;
    }

    return *this;
}

ConnectionLease::~ConnectionLease() {
    reset();
}

pqxx::connection& ConnectionLease::connection() const {
    if (connection_ == nullptr) {
        throw std::runtime_error{"invalid connection lease"};
    }

    return *connection_;
}

ConnectionLease::ConnectionLease(
    ConnectionPool& pool,
    std::unique_ptr<pqxx::connection> connection
) noexcept
    : pool_(&pool),
      connection_(std::move(connection)) {}

void ConnectionLease::reset() noexcept {
    if (pool_ != nullptr && connection_ != nullptr) {
        pool_->release(std::move(connection_));
    }

    connection_ = nullptr;
    pool_ = nullptr;
}

// =====================
// ConnectionPool::IdleConnection
// =====================

ConnectionPool::IdleConnection::IdleConnection(
    std::unique_ptr<pqxx::connection> connection
) noexcept
    : connection_(std::move(connection)),
      returned_at_(Clock::now()) {}

bool ConnectionPool::IdleConnection::expired(Duration timeout) const noexcept {
    return Clock::now() - returned_at_ > timeout;
}

std::unique_ptr<pqxx::connection>
ConnectionPool::IdleConnection::take() noexcept {
    return std::move(connection_);
}

// =====================
// ConnectionPool
// =====================

ConnectionPool::ConnectionPool(
    std::string conninfo,
    std::size_t max_size,
    Duration idle_timeout
)
    : conninfo_(std::move(conninfo)),
      max_size_(max_size),
      idle_timeout_(idle_timeout) {
    if (max_size_ == 0) {
        throw std::invalid_argument{"connection pool max_size must be greater than 0"};
    }
}

ConnectionLease ConnectionPool::acquire() {
    while (true) {
        std::unique_ptr<pqxx::connection> connection;
        bool should_create_connection = false;

        {
            std::unique_lock lock{mutex_};

            available_cv_.wait(lock, [&] {
                return !idle_.empty() || total_connections_ < max_size_;
            });

            connection = takeIdleConnectionLocked();

            if (connection == nullptr && total_connections_ < max_size_) {
                ++total_connections_;
                should_create_connection = true;
            }
        }

        if (connection != nullptr) {
            if (isConnectionAlive(connection)) {
                return ConnectionLease{*this, std::move(connection)};
            }

            spdlog::warn("discarding closed database connection");

            {
                std::lock_guard lock{mutex_};
                discardOneConnectionLocked();
            }

            available_cv_.notify_one();
            continue;
        }

        if (should_create_connection) {
            try {
                connection = createConnection();

                if (isConnectionAlive(connection)) {
                    return ConnectionLease{*this, std::move(connection)};
                }

                spdlog::warn("created database connection is not open");

                {
                    std::lock_guard lock{mutex_};
                    discardOneConnectionLocked();
                }

                available_cv_.notify_one();
                continue;
                
            } catch (const std::exception& e) {
                spdlog::error(
                    "failed to create a new database connection: {}",
                    e.what()
                );

                {
                    std::lock_guard lock{mutex_};
                    discardOneConnectionLocked();
                }

                available_cv_.notify_one();
                throw;
            } catch (...) {
                spdlog::error(
                    "failed to create a new database connection: unknown exception"
                );

                {
                    std::lock_guard lock{mutex_};
                    discardOneConnectionLocked();
                }

                available_cv_.notify_one();
                throw;
            }
        }
    }
}

std::unique_ptr<pqxx::connection>
ConnectionPool::createConnection() const {
    return std::make_unique<pqxx::connection>(conninfo_);
}

void ConnectionPool::release(
    std::unique_ptr<pqxx::connection> connection
) noexcept {
    if (connection == nullptr) {
        return;
    }

    {
        std::lock_guard lock{mutex_};
        idle_.emplace_back(std::move(connection));
    }

    available_cv_.notify_one();
}

bool ConnectionPool::isConnectionAlive(
    const std::unique_ptr<pqxx::connection>& connection
) const noexcept {
    if (connection == nullptr) {
        return false;
    }

    try {
        return connection->is_open();
    } catch (const std::exception& e) {
        spdlog::warn(
            "failed to check database connection status: {}",
            e.what()
        );
        return false;
    } catch (...) {
        spdlog::warn(
            "failed to check database connection status: unknown exception"
        );
        return false;
    }
}

std::unique_ptr<pqxx::connection>
ConnectionPool::takeIdleConnectionLocked() {
    while (!idle_.empty()) {
        auto idle_connection = std::move(idle_.back());
        idle_.pop_back();

        if (idle_connection.expired(idle_timeout_)) {
            discardOneConnectionLocked();
            continue;
        }

        return idle_connection.take();
    }

    return nullptr;
}

void ConnectionPool::discardOneConnectionLocked() noexcept {
    if (total_connections_ > 0) {
        --total_connections_;
    }
}