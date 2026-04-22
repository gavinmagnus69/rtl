#pragma once

#include <memory>

namespace db::core {
class Connection;
};

namespace db::pool {

struct PoolState;
// envelope under connection that can be received via ConnectionPool
struct PooledConnection {
  using Connection = db::core::Connection;
  explicit PooledConnection(const Connection &connection) = delete;
  explicit PooledConnection(const PooledConnection &) = delete;
  PooledConnection &operator=(const PooledConnection &) = delete;
  PooledConnection(PooledConnection &&) noexcept;
  PooledConnection &operator=(PooledConnection &&) noexcept;
  ~PooledConnection();

  Connection &get();
  Connection &operator*();
  Connection *operator->();
  // check for db::core::Connection nullptr
  [[nodiscard]] bool valid() const noexcept;
  // calls close on db::core::Connection
  void mark_broken() noexcept;

private:
  explicit PooledConnection(std::shared_ptr<PoolState> pool_state,
                            std::unique_ptr<db::core::Connection> connection);

private:
  std::shared_ptr<PoolState> m_poolState{nullptr};
  std::unique_ptr<db::core::Connection> m_connection{nullptr};
  bool m_broken{false};
  friend class ConnectionPool;
};
}; // namespace db::pool
