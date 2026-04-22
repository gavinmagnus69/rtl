#include "Transaction.hpp"

#include "Connection.hpp"

#include "ConnectionTransaction.hpp"

#include "Errors.hpp"
#include "IsolationLevel.hpp"
#include "PooledConnection.hpp"

#include "ConnectionPool.hpp"

#include <format>
#include <stdexcept>

namespace db::tx {

struct Transaction::Impl {
  explicit Impl(db::pool::ConnectionPool &pool)
      : connection(pool.acquire()), tx(connection.get().begin_transaction()) {}

  db::pool::PooledConnection connection;
  db::core::ConnectionTransaction tx;
  uint64_t next_savepoint_id{0};
};

Transaction::Transaction(Transaction &&tx) noexcept {
  m_isoLevel = tx.m_isoLevel;
  m_impl = std::move(tx.m_impl);
  tx.m_impl.reset();
};

Transaction &Transaction::operator=(Transaction &&tx) noexcept {
  if (this != &tx) {
    m_isoLevel = tx.m_isoLevel;
    m_impl.reset();
    m_impl = std::move(tx.m_impl);
    tx.m_impl.reset();
  }
  return *this;
};

Transaction::Transaction(db::pool::ConnectionPool &pool)
    : m_impl(std::make_unique<Impl>(pool)) {};

Transaction::Transaction(db::pool::ConnectionPool &pool, IsolationLevel level)
    : m_impl(std::make_unique<Impl>(pool)), m_isoLevel(level) {
  setIsolationLevel(level);
};

Transaction::~Transaction() {
  try {
    if (!m_impl) {
      return;
    }
    if (!m_impl->connection.valid()) {
      m_impl->connection.mark_broken();
    }
    if (m_impl->tx.active()) {
      m_impl->tx.rollback();
    }
  } catch (...) {
  }
};

core::Result Transaction::exec(std::string_view sql) {
  try {
    ensure_valid();
    return m_impl->tx.exec(sql);

  } catch (db::core::ConnectionError &exp) {
    m_impl->connection.mark_broken();
    throw;
  }
};

core::Result
Transaction::exec_params(std::string_view sql,
                         const std::vector<core::DbParam> &params) {
  try {
    ensure_valid();
    return m_impl->tx.exec_params(sql, params);
  } catch (db::core::ConnectionError &exp) {
    m_impl->connection.mark_broken();
    throw;
  }
};

core::Result
Transaction::exec_prepared(std::string_view name,
                           const std::vector<core::DbParam> &params) {
  try {
    ensure_valid();
    return m_impl->tx.exec_prepared(name, params);
  } catch (db::core::ConnectionError &exp) {
    m_impl->connection.mark_broken();
    throw;
  }
};

void Transaction::commit() {
  try {
    ensure_valid();
    m_impl->tx.commit();
    m_impl
        .reset(); // releasing connection to go back to pool after successful op

  } catch (db::core::ConnectionError &exp) {
    m_impl->connection.mark_broken();
    throw;
  }
};

void Transaction::rollback() {
  try {
    ensure_valid();
    m_impl->tx.rollback();
    m_impl
        .reset(); // releasing connection to go back to pool after successful op

  } catch (db::core::ConnectionError &exp) {
    m_impl->connection.mark_broken();
    throw;
  }
};

[[nodiscard]] bool Transaction::active() const noexcept {
  return m_impl && m_impl->tx.active();
};

void Transaction::ensure_valid() {
  if (!m_impl) {
    throw std::logic_error("Invalid transaction handle");
  }
};

std::string Transaction::make_savepoint_name() {
  if (!m_impl) {
    return "";
  }
  return std::format("tx_sp_{}", m_impl->next_savepoint_id++);
};

void Transaction::setIsolationLevel(IsolationLevel level) {
  try {
    auto isolationLevel_to_string =
        [this](IsolationLevel level) -> std::string {
      if (level == IsolationLevel::read_committed) {
        return "READ COMMITTED";
      }
      if (level == IsolationLevel::repeatable_read) {
        return "REPEATABLE READ";
      }
      if (level == IsolationLevel::serializable) {
        return "SERIALIZABLE";
      }
      throw std::runtime_error{"Unknown isolation level"};
    };
    ensure_valid();
    m_impl->tx.exec(std::format("SET TRANSACTION ISOLATION LEVEL {}",
                                isolationLevel_to_string(level)));
  } catch (db::core::ConnectionError &exp) {
    m_impl->connection.mark_broken();
    throw;
  }
};

}; // namespace db::tx
