#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

namespace db::core {

struct ConnectionConfig {
  std::string host{"127.0.0.1"};
  std::string dbname{};
  std::string user{};
  std::string password{};
  std::uint16_t port{5432};

  std::optional<std::string> application_name{std::nullopt};
  std::optional<std::chrono::seconds> connect_timeout{std::nullopt};

  [[nodiscard]] bool valid() const noexcept {
    return !host.empty() && port > 0 && !dbname.empty() && !user.empty();
  };
};
}; // namespace db::core
