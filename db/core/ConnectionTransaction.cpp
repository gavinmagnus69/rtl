#include "ConnectionTransaction.hpp"

#include "Result.hpp"
#include "detail/ConnectionState.hpp"
#include "detail/PqxxHelpers.hpp"
#include "detail/ResultFactory.hpp"

#include <pqxx/pqxx>

#include <memory>
#include <stdexcept>
#include <utility>

namespace db::core {

struct ConnectionTransaction::Impl {
  explicit Impl(pqxx::connection &connection) : work(connection) {}

  pqxx::work work;
};

ConnectionTransaction::ConnectionTransaction(
    ConnectionTransaction &&txn) noexcept {
  m_state = std::move(txn.m_state);
  txn.m_state = nullptr;
  m_impl = std::move(txn.m_impl);
  txn.m_impl = nullptr;
  m_active = txn.m_active;
  txn.m_active = false;
};

ConnectionTransaction &
ConnectionTransaction::operator=(ConnectionTransaction &&txn) noexcept {
  if (this != &txn) {
    finish();
    m_state = std::move(txn.m_state);
    txn.m_state = nullptr;
    m_impl = std::move(txn.m_impl);
    txn.m_impl = nullptr;
    m_active = txn.m_active;
    txn.m_active = false;
  }
  return *this;
};

ConnectionTransaction::ConnectionTransaction(
    std::shared_ptr<detail::ConnectionState> state)
    : m_state(std::move(state)) {
  begin();
};

void ConnectionTransaction::begin() {
  if (!m_state) {
    throw std::logic_error{"Invalid transaction handle"};
  }
  if (!m_state->connection) {
    throw ConnectionError{"Connection state does not own a connection"};
  }
  if (!m_state->connection->is_open()) {
    throw ConnectionError{"Connection is not open"};
  }
  if (m_state->transaction_active) {
    throw std::logic_error{"Connection already has an active transaction"};
  }
  if (m_active || m_impl) {
    throw std::logic_error{"Transaction has already been started"};
  }

  detail::translate_pqxx_exceptions([&]() {
    m_impl = std::make_unique<Impl>(*m_state->connection);
    m_state->transaction_active = true;
    m_active = true;
  });
};

Result ConnectionTransaction::exec(std::string_view sql) {

  return detail::translate_pqxx_exceptions([&]() {
    ensure_active();
    pqxx::result result = m_impl->work.exec(sql);
    return detail::ResultFactory::make_from_pqxx_result(std::move(result));
  });
};

Result ConnectionTransaction::exec_params(std::string_view sql,
                                          const std::vector<DbParam> &params) {

  return detail::translate_pqxx_exceptions([&]() {
    ensure_active();

    if (params.empty()) {
      pqxx::result result = m_impl->work.exec(sql);
      return detail::ResultFactory::make_from_pqxx_result(std::move(result));
    }
    auto pq_params = detail::make_pqxx_params(params);
    pqxx::result result = m_impl->work.exec(sql, std::move(pq_params));
    return detail::ResultFactory::make_from_pqxx_result(std::move(result));
  });
};

Result
ConnectionTransaction::exec_prepared(std::string_view name,
                                     const std::vector<DbParam> &params) {
  return detail::translate_pqxx_exceptions([&]() {
    ensure_active();
    auto pq_params = detail::make_pqxx_params(params);
    pqxx::result result =
        m_impl->work.exec(pqxx::prepped{std::string{name}}, std::move(pq_params));
    return detail::ResultFactory::make_from_pqxx_result(std::move(result));
  });
};

void ConnectionTransaction::commit() {

  detail::translate_pqxx_exceptions([&]() {
    ensure_active();
    m_impl->work.commit();
    finish();
  });
};

void ConnectionTransaction::rollback() {
  ensure_active();
  finish();
};

[[nodiscard]] bool ConnectionTransaction::active() const noexcept {
  return m_active;
};

void ConnectionTransaction::ensure_active() const {
  if (!m_state) {
    throw std::logic_error("Invalid transaction handle");
  }
  if (!m_active || !m_impl) {
    throw std::logic_error("Transaction is not active");
  }
};

void ConnectionTransaction::finish() noexcept {
  m_impl.reset();
  m_active = false;
  if (m_state) {
    m_state->transaction_active = false;
  }
};

ConnectionTransaction::~ConnectionTransaction() {
  try {
    finish();
  } catch (...) {
  }
};

}; // namespace db::core
