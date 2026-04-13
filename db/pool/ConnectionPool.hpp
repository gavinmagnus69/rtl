#pragma once

#include <memory>

#include "PoolConfig.hpp"
#include "PooledConnection.hpp"

namespace db::core {
struct ConnectionConfig;
};

namespace db::pool {

struct PoolState;
class ConnectionPool {
public:
  ConnectionPool(const PoolConfig &config,
                 const db::core::ConnectionConfig &conn_cfg);
  ~ConnectionPool() = default;
  ConnectionPool(const ConnectionPool &) = delete;
  ConnectionPool &operator=(const ConnectionPool &pool) = delete;
  ConnectionPool(ConnectionPool &&) = default;
  ConnectionPool &operator=(ConnectionPool &&) = default;

public:
  PooledConnection acquire();
  PooledConnection try_acquire_for(size_t timeout_ms);
  [[nodiscard]] size_t idle_count() const;
  [[nodiscard]] size_t active_count() const;

private:
  std::shared_ptr<PoolState> m_state{nullptr};
};
}; // namespace db::pool
