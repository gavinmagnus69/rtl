#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace db::core {

namespace detail {
class ResultFactory;
}

struct ResultState;

class Field {
public:
  Field() = default;
  [[nodiscard]] bool is_null() const;
  // string_view is only valid as long as the underlying data is valid
  [[nodiscard]] std::string_view name() const;
  // string_view is only valid as long as the underlying data is valid
  [[nodiscard]] std::string_view view() const;
  [[nodiscard]] std::int64_t as_int64() const;
  [[nodiscard]] double as_double() const;
  [[nodiscard]] std::string as_string() const;
  [[nodiscard]] bool as_bool() const;

private:
  friend class Row;
  friend class Result;
  Field(std::shared_ptr<ResultState> state, std::size_t row_index,
        std::size_t column_index);
  std::shared_ptr<ResultState> m_state{nullptr};
  std::size_t m_row_index{0};
  std::size_t m_column_index{0};
};

class Row {
public:
  Row() = default;
  [[nodiscard]] std::size_t size() const;
  [[nodiscard]] bool contains(std::string_view column) const;
  [[nodiscard]] Field operator[](std::size_t index) const;
  [[nodiscard]] Field operator[](std::string_view column_name) const;

private:
  friend class Result;
  Row(std::shared_ptr<ResultState> state, std::size_t row_index);
  std::shared_ptr<ResultState> m_state{nullptr};
  std::size_t m_row_index{0};
};

class Result {
public:
  Result() = default;
  ~Result() = default;
  Result(const Result &);
  Result(Result &&) noexcept;
  Result &operator=(const Result &);
  Result &operator=(Result &&) noexcept;

  [[nodiscard]] bool valid() const;
  [[nodiscard]] std::size_t size() const;
  [[nodiscard]] bool empty() const;
  [[nodiscard]] Row operator[](std::size_t index) const;

private:
  friend class detail::ResultFactory;
  explicit Result(std::shared_ptr<ResultState> state);
  std::shared_ptr<ResultState> m_state{nullptr};
};

} // namespace db::core
