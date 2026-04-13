#pragma once

#include <memory>

#include "ConnectionConfig.hpp"
#include <pqxx/pqxx>

namespace db::core::detail {

struct ConnectionState {
  std::unique_ptr<pqxx::connection> connection{nullptr};
  ConnectionConfig cfg{};
  bool transaction_active{false};
};

}; // namespace db::core::detail