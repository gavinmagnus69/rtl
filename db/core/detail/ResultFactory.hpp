#pragma once
#include "Result.hpp"

namespace pqxx {
class result;
}

namespace db::core::detail {
class ResultFactory {
public:
  static Result make_from_pqxx_result(pqxx::result &&result);
};
} // namespace db::core::detail
