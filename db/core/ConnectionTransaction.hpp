#pragma once

#include <memory>
#include <string_view>
#include <vector>

#include "Params.hpp"
#include "Result.hpp"

namespace db::core {

class Connection;

namespace detail {
struct ConnectionState;
};

struct ConnectionTransaction {
  ConnectionTransaction(const ConnectionTransaction &) = delete;
  ConnectionTransaction &operator=(const ConnectionTransaction &) = delete;
  ConnectionTransaction(ConnectionTransaction &&) noexcept;
  ConnectionTransaction &operator=(ConnectionTransaction &&) noexcept;
  ~ConnectionTransaction();
  Result exec(std::string_view sql);
  Result exec_params(std::string_view sql, const std::vector<DbParam> &params);
  Result exec_prepared(std::string_view name,
                       const std::vector<DbParam> &params);
  void commit();
  void rollback();
  [[nodiscard]] bool active() const noexcept;

private:
  struct Impl;
  ConnectionTransaction(std::shared_ptr<detail::ConnectionState>);
  void ensure_active() const;
  void finish() noexcept;
  void begin();
  std::shared_ptr<detail::ConnectionState> m_state{nullptr};
  std::unique_ptr<Impl> m_impl{nullptr};
  bool m_active{false};

  friend class Connection;
};
}; // namespace db::core
