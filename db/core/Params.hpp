#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <variant>

namespace db::core {
using DbParamValue = std::variant<std::nullptr_t, std::int32_t, std::int64_t,
                                  double, bool, std::string>;

struct DbParam {
  std::string name;
  DbParamValue value;
  DbParam(std::string name, DbParamValue value)
      : name(std::move(name)), value(std::move(value)) {}
};

} // namespace db::core
