#include "ConnectionPool.hpp"

#include "ConnectionConfig.hpp"
#include "PoolState.hpp"

#include "Errors.hpp"
#include "PooledConnection.hpp"

namespace db::pool {

ConnectionPool::ConnectionPool(const PoolConfig &config,
                               const db::core::ConnectionConfig &conn_cfg)
    : m_state(std::make_shared<PoolState>(config, conn_cfg)) {};

PooledConnection ConnectionPool::acquire() {
  if (!m_state) {
    throw db::core::PoolExhausted{"Failed to create pool"};
  }
  auto connection_ptr = m_state->acquire();
  if (!connection_ptr) {
    throw db::core::PoolExhausted{"Connection ptr is nullptr"};
  }
  return PooledConnection{m_state, std::move(connection_ptr)};
};

PooledConnection ConnectionPool::try_acquire_for(size_t timeout_ms) {
  if (!m_state) {
    throw db::core::PoolExhausted{"Failed to create pool"};
  }
  auto connection_ptr =
      m_state->try_acquire_for(std::chrono::milliseconds{timeout_ms});
  if (!connection_ptr) {
    throw db::core::PoolExhausted{"Connection ptr is nullptr"};
  }
  return PooledConnection{m_state, std::move(connection_ptr)};
};

[[nodiscard]] size_t ConnectionPool::idle_count() const {
  if (!m_state) {
    throw db::core::PoolExhausted{"Failed to create pool"};
  }
  return m_state->idle_count();
};

[[nodiscard]] size_t ConnectionPool::active_count() const {
  if (!m_state) {
    throw db::core::PoolExhausted{"Failed to create pool"};
  }
  return m_state->active_count();
};

}; // namespace db::pool