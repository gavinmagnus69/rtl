#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "WsDtoCommon.hpp"

namespace ws::dto {

struct StartStreamRequestDto {
  static constexpr std::string_view method_name = "device.start_stream";

  std::string device_id{};
  std::string stream_name{};
  std::optional<std::uint32_t> frame_rate_hz{};
  std::optional<std::uint32_t> sample_rate_hz{};
  std::optional<std::uint32_t> batch_size{};
  bool replay_last_value{false};

  [[nodiscard]] bool is_valid() const noexcept {
    return is_non_empty(device_id) && is_non_empty(stream_name);
  }
};

} // namespace ws::dto
