#pragma once

#include <memory>
#include <string>
#include <string_view>

namespace db::tx {

struct Transaction;

struct Savepoint {
  Savepoint(const Savepoint &) = delete;
  Savepoint &operator=(const Savepoint &) = delete;
  explicit Savepoint(Transaction &tx);
  Savepoint(Savepoint &&) noexcept;
  Savepoint &operator=(Savepoint &&) noexcept;
  ~Savepoint();

  void rollback();
  void release();
  [[nodiscard]] bool active() const noexcept;
  [[nodiscard]] std::string_view name() const noexcept;

private:
  Savepoint(Transaction &tx, std::string_view name); // sql-injection warning

  void ensure_valid();
  struct Impl;
  std::unique_ptr<Impl> m_impl{nullptr};
};
}; // namespace db::tx