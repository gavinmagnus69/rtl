#pragma once

#include <string_view>

namespace ws::dto {

struct ListDevicesRequestDto {
  static constexpr std::string_view method_name = "device.list";
  bool include_disconnected{true};
  bool include_capabilities{false};

  [[nodiscard]] bool is_valid() const noexcept { return true; }
};

} // namespace ws::dto
