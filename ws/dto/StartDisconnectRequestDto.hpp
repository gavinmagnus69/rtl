#pragma once

#include <string>
#include <string_view>

#include "WsDtoCommon.hpp"

namespace ws::dto {

struct StartDisconnectRequestDto {
  static constexpr std::string_view method_name = "device.disconnect";

  std::string device_id{};
  bool force{false};
  std::string reason{};

  [[nodiscard]] bool is_valid() const noexcept {
    return is_non_empty(device_id);
  }
};

} // namespace ws::dto
