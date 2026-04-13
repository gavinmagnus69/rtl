#include "Connection.hpp"

#include "ConnectionTransaction.hpp"
#include "detail/ConnectionState.hpp"
#include "detail/PqxxHelpers.hpp"

#include <pqxx/pqxx>

#include <stdexcept>
#include <string>

namespace db::core {

Connection::Connection(const ConnectionConfig &cfg) {
  try_to_establish_connection(cfg);
}

Connection::Connection(Connection &&connection) noexcept = default;

Connection &Connection::operator=(Connection &&connection) noexcept = default;

Connection::~Connection() = default;

const ConnectionConfig &Connection::config() const noexcept {
  return m_connectionState->cfg;
}

bool Connection::is_open() const noexcept {
  return m_connectionState && m_connectionState->connection != nullptr &&
         m_connectionState->connection->is_open();
}

bool Connection::close() noexcept {
  if (!m_connectionState || !m_connectionState->connection) {
    return true;
  }
  m_connectionState->connection.reset();
  return true;
}

void Connection::try_to_establish_connection(const ConnectionConfig &cfg) {
  if (!cfg.valid()) {
    throw ConfigurationError("Invalid connection configuration");
  }
  m_connectionState = std::make_shared<detail::ConnectionState>();
  m_connectionState->cfg = cfg;
  try {
    m_connectionState->connection =
        std::make_unique<pqxx::connection>(detail::make_connection_string(cfg));
  } catch (const pqxx::broken_connection &ex) {
    throw ConnectionError(ex.what());
  } catch (const pqxx::failure &ex) {
    throw ConnectionError(ex.what());
  }
}

ConnectionTransaction Connection::begin_transaction() {
  ensure_open();
  return ConnectionTransaction{m_connectionState};
}

[[nodiscard]] Result Connection::exec(std::string_view sql) {
  auto tx = begin_transaction();
  auto result = tx.exec(sql);
  tx.commit();
  return result;
}

[[nodiscard]] Result
Connection::exec_params(std::string_view sql,
                        const std::vector<DbParam> &params) {
  ensure_open();
  if (params.empty()) {
    return exec(sql);
  }
  auto tx = begin_transaction();
  auto result = tx.exec_params(sql, params);
  tx.commit();
  return result;
}

void Connection::prepare(std::string_view name, std::string_view sql) {

  detail::translate_pqxx_exceptions([&]() {
    ensure_open();
    m_connectionState->connection->prepare(std::string{name}, std::string{sql});
  });
};

Result Connection::exec_prepared(std::string_view name,
                                 const std::vector<DbParam> &params) {
  auto txn = begin_transaction();
  auto result = txn.exec_prepared(std::string{name}, params);
  txn.commit();
  return result;
}

void Connection::ensure_open() {
  if (!m_connectionState) {
    throw std::logic_error{"Invalid connection handle"};
  }
  if (!m_connectionState->connection ||
      !m_connectionState->connection->is_open()) {
    throw ConnectionError("Connection is not open");
  }
}

} // namespace db::core
