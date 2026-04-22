#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace ws::dto {

[[nodiscard]] inline bool is_non_empty(std::string_view value) noexcept {
  return !value.empty();
}

[[nodiscard]] inline bool
are_all_non_empty(const std::vector<std::string> &values) noexcept {
  for (const auto &value : values) {
    if (value.empty()) {
      return false;
    }
  }
  return true;
}

} // namespace ws::dto
