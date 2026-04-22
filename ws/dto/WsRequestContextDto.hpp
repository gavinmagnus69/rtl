#pragma once

#include <string>

#include "WsDtoCommon.hpp"

namespace ws::dto {

struct WsRequestContextDto {
  std::string request_id{};
  std::string correlation_id{};
  std::string method{};
  std::string session_id{};
  std::string user_id{};
  std::string version{"1"};

  [[nodiscard]] bool is_valid() const noexcept {
    return is_non_empty(request_id) && is_non_empty(method);
  }
};

} // namespace ws::dto
