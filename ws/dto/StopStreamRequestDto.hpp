#pragma once

#include <string>
#include <string_view>

#include "WsDtoCommon.hpp"

namespace ws::dto {

struct StopStreamRequestDto {
  static constexpr std::string_view method_name = "device.stop_stream";

  std::string device_id{};
  std::string stream_name{};
  std::string reason{};

  [[nodiscard]] bool is_valid() const noexcept {
    return is_non_empty(device_id) && is_non_empty(stream_name);
  }
};

} // namespace ws::dto
