#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "WsDtoCommon.hpp"

namespace ws::dto {

struct SubscribeTelemetryRequestDto {
  static constexpr std::string_view method_name = "telemetry.subscribe";

  std::string device_id{};
  std::vector<std::string> topics{};
  bool include_snapshot{true};

  [[nodiscard]] bool is_valid() const noexcept {
    return is_non_empty(device_id) && !topics.empty() &&
           are_all_non_empty(topics);
  }
};

} // namespace ws::dto
