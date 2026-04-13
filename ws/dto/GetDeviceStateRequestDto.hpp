#pragma once

#include <string>
#include <string_view>

#include "WsDtoCommon.hpp"

namespace ws::dto {

struct GetDeviceStateRequestDto {
  static constexpr std::string_view method_name = "device.get_state";

  std::string device_id{};
  bool include_capabilities{false};

  [[nodiscard]] bool is_valid() const noexcept {
    return is_non_empty(device_id);
  }
};

} // namespace ws::dto
