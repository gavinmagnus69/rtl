#pragma once

#include "ConnectionConfig.hpp"
#include "ConnectionTransaction.hpp"

#include "Errors.hpp"
#include "Params.hpp"
#include "Result.hpp"

#include <memory>
#include <string_view>
#include <vector>

namespace db::core {

namespace detail {
struct ConnectionState;
};

class Connection {
public:
  explicit Connection(const ConnectionConfig& cfg);

  Connection(const Connection&) = delete;

  Connection& operator=(const Connection&) = delete;

  Connection(Connection&& connection) noexcept;

  Connection& operator=(Connection&& connection) noexcept;

  ~Connection();
public:
  [[nodiscard]] const ConnectionConfig& config() const noexcept;

  [[nodiscard]] bool is_open() const noexcept;

  [[nodiscard]] bool close() noexcept;

  [[nodiscard]] Result exec(std::string_view sql);

  [[nodiscard]] Result exec_params(std::string_view sql, const std::vector<DbParam>& params);

  void prepare(std::string_view name, std::string_view sql);

  Result exec_prepared(std::string_view name, const std::vector<DbParam>& params);

  ConnectionTransaction begin_transaction();
private:
  void try_to_establish_connection(const ConnectionConfig&);
  void ensure_open();
private:
  std::shared_ptr<detail::ConnectionState> m_connectionState{nullptr};
  // std::unique_ptr<pqxx::connection> m_connection{nullptr};
  // ConnectionConfig m_config{};
};
} // namespace db::core
