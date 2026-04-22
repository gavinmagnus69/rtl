#include "PooledConnection.hpp"

#include <stdexcept>

#include "Connection.hpp"
#include "ConnectionPool.hpp"
#include "Errors.hpp"
#include "PoolState.hpp"

namespace db::pool {
using Connection = db::core::Connection;

PooledConnection::PooledConnection(PooledConnection &&connection) noexcept {
  m_poolState = std::move(connection.m_poolState);
  connection.m_poolState = nullptr;
  m_connection = std::move(connection.m_connection);
  m_broken = connection.m_broken;
  connection.m_connection = nullptr;
};

PooledConnection &
PooledConnection::operator=(PooledConnection &&connection) noexcept {
  if (this != &connection) {
    if (m_poolState) {
      m_poolState->return_connection(std::move(m_connection));
    }
    m_poolState = std::move(connection.m_poolState);
    connection.m_poolState = nullptr;
    m_connection = std::move(connection.m_connection);
    m_broken = connection.m_broken;
    connection.m_connection = nullptr;
  }
  return *this;
};

PooledConnection::PooledConnection(
    std::shared_ptr<PoolState> pool_state,
    std::unique_ptr<db::core::Connection> connection)
    : m_poolState(std::move(pool_state)), m_connection(std::move(connection)) {

      };

PooledConnection::~PooledConnection() {
  if (m_poolState) {
    m_poolState->return_connection(std::move(m_connection));
  }
};

Connection &PooledConnection::get() {
  if (!m_connection) {
    throw std::logic_error("Invalid pooled connection handle");
  }
  return *m_connection;
};

Connection &PooledConnection::operator*() {
  if (!m_connection) {
    throw std::logic_error("Invalid pooled connection handle");
  }
  return *m_connection;
};

Connection *PooledConnection::operator->() {
  if (!m_connection) {
    throw std::logic_error("Invalid pooled connection handle");
  }
  return m_connection.get();
};

[[nodiscard]] bool PooledConnection::valid() const noexcept {
  return m_connection != nullptr;
}

void PooledConnection::mark_broken() noexcept {
  m_broken = true;
  if (m_connection) {
    (void)m_connection->close();
  }
};

}; // namespace db::pool
