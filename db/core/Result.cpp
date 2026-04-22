#include "Result.hpp"
#include "Errors.hpp"
#include "detail/ResultFactory.hpp"

#include <pqxx/pqxx>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

[[noreturn]] void throw_invalid_state(std::string_view object_name) {
  throw db::core::QueryError("Invalid " + std::string(object_name) + " state");
}

template <typename T>
T convert_field(const pqxx::field &field, std::string_view target_type) {
  try {
    return field.as<T>();
  } catch (const std::exception &ex) {
    throw db::core::MappingError("Failed to convert field to " +
                                 std::string(target_type) + ": " + ex.what());
  }
}

} // namespace

namespace db::core {

struct ResultState {
  pqxx::result result;
};

Result detail::ResultFactory::make_from_pqxx_result(pqxx::result &&result) {
  return Result(std::make_shared<ResultState>(ResultState{std::move(result)}));
}

[[nodiscard]] bool Field::is_null() const {
  if (m_state == nullptr) {
    throw_invalid_state("field");
  }
  const auto &field = m_state->result[m_row_index][m_column_index];
  return field.is_null();
}

[[nodiscard]] std::string_view Field::name() const {
  if (!m_state) {
    throw_invalid_state("field");
  }
  return m_state->result[m_row_index][m_column_index].name();
}
// string_view is only valid as long as the underlying data is valid
[[nodiscard]] std::string_view Field::view() const {
  if (!m_state) {
    throw_invalid_state("field");
  }
  return m_state->result[m_row_index][m_column_index].view();
}

std::int64_t Field::as_int64() const {
  if (!m_state) {
    throw_invalid_state("field");
  }
  return convert_field<std::int64_t>(
      m_state->result[m_row_index][m_column_index], "int64_t");
}

double Field::as_double() const {
  if (!m_state) {
    throw_invalid_state("field");
  }
  return convert_field<double>(m_state->result[m_row_index][m_column_index],
                               "double");
}

std::string Field::as_string() const {
  if (!m_state) {
    throw_invalid_state("field");
  }
  return convert_field<std::string>(
      m_state->result[m_row_index][m_column_index], "string");
}

bool Field::as_bool() const {
  if (!m_state) {
    throw_invalid_state("field");
  }
  return convert_field<bool>(m_state->result[m_row_index][m_column_index],
                             "bool");
}

Field::Field(std::shared_ptr<ResultState> state, std::size_t row_index,
             std::size_t column_index)
    : m_state(state), m_row_index(row_index), m_column_index(column_index) {}

[[nodiscard]] std::size_t Row::size() const {
  if (!m_state) {
    throw_invalid_state("row");
  }
  return m_state->result.columns();
}

[[nodiscard]] bool Row::contains(std::string_view column) const {
  if (!m_state) {
    throw_invalid_state("row");
  }
  try {
    m_state->result.column_number(std::string{column});
    return true;
  } catch (const std::exception &) {
    return false;
  }
}

[[nodiscard]] Field Row::operator[](std::size_t index) const {
  if (!m_state) {
    throw_invalid_state("row");
  }
  if (m_state->result.columns() <= index) {
    throw std::out_of_range("Column index out of bounds");
  }
  return Field(m_state, m_row_index, index);
}

[[nodiscard]] Field Row::operator[](std::string_view column_name) const {
  if (!m_state) {
    throw_invalid_state("row");
  }
  try {
    return Field{m_state, m_row_index,
                 static_cast<std::size_t>(
                       m_state->result.column_number(std::string{column_name}))};
  } catch (const std::exception &ex) {
    throw QueryError("Column not found: " + std::string(column_name) + ": " +
                     ex.what());
  }
}

Row::Row(std::shared_ptr<ResultState> state, std::size_t row_index)
    : m_state(std::move(state)), m_row_index(row_index) {}

Result::Result(const Result &rhs) { this->m_state = rhs.m_state; }

Result::Result(Result &&rhs) noexcept {
  this->m_state = std::move(rhs.m_state);
  rhs.m_state = nullptr;
}

Result &Result::operator=(const Result &rhs) {
  if (this == &rhs) {
    return *this;
  }
  this->m_state = rhs.m_state;
  return *this;
}

Result &Result::operator=(Result &&rhs) noexcept {
  if (this == &rhs) {
    return *this;
  }
  this->m_state = std::move(rhs.m_state);
  rhs.m_state = nullptr;
  return *this;
}

[[nodiscard]] bool Result::valid() const { return m_state != nullptr; }

[[nodiscard]] std::size_t Result::size() const {
  if (!m_state) {
    throw_invalid_state("result");
  }
  return m_state->result.size();
}

[[nodiscard]] bool Result::empty() const {
  if (!m_state) {
    throw_invalid_state("result");
  }
  return m_state->result.empty();
}
[[nodiscard]] Row Result::operator[](std::size_t index) const {
  if (!m_state) {
    throw_invalid_state("result");
  }
  if (index >= m_state->result.size()) {
    throw std::out_of_range("Row index out of bounds");
  }
  return Row{m_state, index};
}

Result::Result(std::shared_ptr<ResultState> state)
    : m_state(std::move(state)) {}

} // namespace db::core
