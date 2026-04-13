#pragma once

#include <string>
#include <string_view>

#include "WsDtoCommon.hpp"

namespace ws::dto {

struct SetDeviceParameterRequestDto {
  static constexpr std::string_view method_name = "device.set_parameter";

  std::string device_id{};
  std::string parameter_name{};
  std::string value{};

  [[nodiscard]] bool is_valid() const noexcept {
    return is_non_empty(device_id) && is_non_empty(parameter_name);
  }
};

} // namespace ws::dto
