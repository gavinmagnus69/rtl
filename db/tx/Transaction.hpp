#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "IsolationLevel.hpp"

namespace db::pool {
class ConnectionPool;
struct PooledConnection;
}; // namespace db::pool

namespace db::core {
class Result;
struct DbParam;
struct ConnectionTransaction;
}; // namespace db::core

namespace db::tx {

struct Savepoint;

struct Transaction {
  Transaction &operator=(const Transaction &) = delete;
  Transaction(const Transaction &) = delete;
  explicit Transaction(db::pool::ConnectionPool &);
  Transaction(db::pool::ConnectionPool &, IsolationLevel);
  Transaction(Transaction &&) noexcept;
  Transaction &operator=(Transaction &&) noexcept;
  ~Transaction();

  core::Result exec(std::string_view sql);
  core::Result exec_params(std::string_view sql,
                           const std::vector<core::DbParam> &params);
  core::Result exec_prepared(std::string_view name,
                             const std::vector<core::DbParam> &params);
  void commit();
  void rollback();
  [[nodiscard]] bool active() const noexcept;

private:
  void setIsolationLevel(IsolationLevel);
  std::string make_savepoint_name();
  struct Impl;
  void ensure_valid();
  std::unique_ptr<Impl> m_impl;
  IsolationLevel m_isoLevel{IsolationLevel::read_committed};
  friend class Savepoint;
};

}; // namespace db::tx
