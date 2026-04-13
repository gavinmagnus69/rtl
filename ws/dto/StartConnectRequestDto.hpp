#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "WsDtoCommon.hpp"

namespace ws::dto {

struct StartConnectRequestDto {
  static constexpr std::string_view method_name = "device.connect";

  std::string device_id{};
  bool force_reconnect{false};
  std::optional<std::uint32_t> timeout_ms{};

  [[nodiscard]] bool is_valid() const noexcept {
    return is_non_empty(device_id);
  }
};

} // namespace ws::dto
