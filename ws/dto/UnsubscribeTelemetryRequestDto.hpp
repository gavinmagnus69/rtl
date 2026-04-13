#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "WsDtoCommon.hpp"

namespace ws::dto {

struct UnsubscribeTelemetryRequestDto {
  static constexpr std::string_view method_name = "telemetry.unsubscribe";

  std::string device_id{};
  std::vector<std::string> topics{};

  [[nodiscard]] bool is_valid() const noexcept {
    return is_non_empty(device_id) && !topics.empty() &&
           are_all_non_empty(topics);
  }
};

} // namespace ws::dto
