#pragma once
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace db::core {

enum class ErrorCode {
  unknown,
  connection_error,
  pool_exhausted,
  pool_timeout,
  query_error,
  constraint_violation,
  serialization_failure,
  deadlock_detected,
  transaction_aborted,
  mapping_error,
  configuration_error
};

class DbError : public std::runtime_error {
public:
  DbError(ErrorCode code, std::string_view message,
          std::optional<std::string> sql_state = std::nullopt)
      : std::runtime_error(std::string(message)), m_code(code),
        m_sqlState(std::move(sql_state)) {}
  [[nodiscard]] ErrorCode code() const noexcept { return m_code; }
  [[nodiscard]] const std::optional<std::string> &sqlState() const noexcept {
    return m_sqlState;
  }

private:
  ErrorCode m_code{ErrorCode::unknown};
  std::optional<std::string> m_sqlState{std::nullopt};
};

class ConnectionError : public DbError {
public:
  explicit ConnectionError(std::string message,
                           std::optional<std::string> sql_state = std::nullopt)
      : DbError(ErrorCode::connection_error, std::move(message),
                std::move(sql_state)) {}
};

class PoolExhausted : public DbError {
public:
  explicit PoolExhausted(std::string message)
      : DbError(ErrorCode::pool_exhausted, std::move(message)) {}
};

class PoolTimeout : public DbError {
public:
  explicit PoolTimeout(std::string message)
      : DbError(ErrorCode::pool_timeout, std::move(message)) {}
};

class QueryError : public DbError {
public:
  explicit QueryError(std::string message,
                      std::optional<std::string> sql_state = std::nullopt)
      : DbError(ErrorCode::query_error, std::move(message),
                std::move(sql_state)) {}
};

class ConstraintViolation : public DbError {
public:
  explicit ConstraintViolation(
      std::string message, std::optional<std::string> sql_state = std::nullopt)
      : DbError(ErrorCode::constraint_violation, std::move(message),
                std::move(sql_state)) {}
};

class SerializationFailure : public DbError {
public:
  explicit SerializationFailure(
      std::string message, std::optional<std::string> sql_state = std::nullopt)
      : DbError(ErrorCode::serialization_failure, std::move(message),
                std::move(sql_state)) {}
};

class DeadlockDetected : public DbError {
public:
  explicit DeadlockDetected(std::string message,
                            std::optional<std::string> sql_state = std::nullopt)
      : DbError(ErrorCode::deadlock_detected, std::move(message),
                std::move(sql_state)) {}
};

class TransactionAborted : public DbError {
public:
  explicit TransactionAborted(
      std::string message, std::optional<std::string> sql_state = std::nullopt)
      : DbError(ErrorCode::transaction_aborted, std::move(message),
                std::move(sql_state)) {}
};

class MappingError : public DbError {
public:
  explicit MappingError(std::string message)
      : DbError(ErrorCode::mapping_error, std::move(message)) {}
};

class ConfigurationError : public DbError {
public:
  explicit ConfigurationError(std::string message)
      : DbError(ErrorCode::configuration_error, std::move(message)) {}
};

} // namespace db::core
