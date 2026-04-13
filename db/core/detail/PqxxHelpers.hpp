#pragma once

#include "ConnectionConfig.hpp"
#include "Errors.hpp"
#include "Params.hpp"

#include <pqxx/pqxx>

namespace db::core::detail {

inline static pqxx::params
make_pqxx_params(const std::vector<db::core::DbParam> &params) {
  pqxx::params pq_params;
  pq_params.reserve(params.size());
  for (const auto &param : params) {
    std::visit(
        [&pq_params](const auto &value) {
          using T = std::decay_t<decltype(value)>;

          if constexpr (std::is_same_v<T, std::nullptr_t>) {
            pq_params.append();
            return;
          }
          pq_params.append(value);
        },
        param.value);
  }
  return pq_params;
}

inline std::string
make_connection_string(const db::core::ConnectionConfig &config) {
  std::string conninfo =
      std::format("host={} port={} dbname={} user={} password={}", config.host,
                  config.port, config.dbname, config.user, config.password);

  if (config.application_name.has_value()) {
    conninfo += std::format(" application_name={}", *config.application_name);
  }

  if (config.connect_timeout.has_value()) {
    conninfo +=
        std::format(" connect_timeout={}", config.connect_timeout->count());
  }

  return conninfo;
}

inline std::optional<std::string> to_sql_state(std::string const &sql_state) {
  if (sql_state.empty()) {
    return std::nullopt;
  }
  return sql_state;
}

template <typename Fn>
inline auto translate_pqxx_exceptions(Fn &&fn) -> std::invoke_result_t<Fn &&> {
  using invoke_result_t = std::invoke_result_t<Fn &&>;

  try {
    if constexpr (std::is_void_v<invoke_result_t>) {
      std::forward<Fn>(fn)();
      return;
    }
    return std::forward<Fn>(fn)();
  } catch (const pqxx::deadlock_detected &ex) {
    throw db::core::DeadlockDetected(ex.what(), to_sql_state(ex.sqlstate()));
  } catch (const pqxx::serialization_failure &ex) {
    throw db::core::SerializationFailure(ex.what(),
                                         to_sql_state(ex.sqlstate()));
  } catch (const pqxx::integrity_constraint_violation &ex) {
    throw db::core::ConstraintViolation(ex.what(), to_sql_state(ex.sqlstate()));
  } catch (const pqxx::transaction_rollback &ex) {
    throw db::core::TransactionAborted(ex.what(), to_sql_state(ex.sqlstate()));
  } catch (const pqxx::sql_error &ex) {
    throw db::core::QueryError(ex.what(), to_sql_state(ex.sqlstate()));
  } catch (const pqxx::conversion_error &ex) {
    throw db::core::MappingError(ex.what());
  } catch (const pqxx::argument_error &ex) {
    throw db::core::QueryError(ex.what());
  } catch (const pqxx::usage_error &ex) {
    throw db::core::QueryError(ex.what());
  } catch (const pqxx::internal_error &ex) {
    throw db::core::QueryError(ex.what());
  } catch (const pqxx::in_doubt_error &ex) {
    throw db::core::TransactionAborted(ex.what());
  } catch (const pqxx::broken_connection &ex) {
    throw db::core::ConnectionError(ex.what());
  } catch (const pqxx::failure &ex) {
    throw db::core::ConnectionError(ex.what());
  }
}

}; // namespace db::core::detail