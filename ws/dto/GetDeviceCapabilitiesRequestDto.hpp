#pragma once

#include <string>
#include <string_view>

#include "WsDtoCommon.hpp"

namespace ws::dto {

struct GetDeviceCapabilitiesRequestDto {
  static constexpr std::string_view method_name = "device.get_capabilities";

  std::string device_id{};

  [[nodiscard]] bool is_valid() const noexcept {
    return is_non_empty(device_id);
  }
};

} // namespace ws::dto
