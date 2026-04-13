#include "Savepoint.hpp"

#include "Result.hpp"
#include "Transaction.hpp"


#include <format>
#include <stdexcept>

namespace db::tx {

struct Savepoint::Impl {
  Transaction *tx{nullptr};
  std::string savepoint_name{};
  bool active{false};
};

Savepoint::Savepoint(Transaction &tx) {
  if (!tx.active()) {
    throw std::logic_error("Transaction is inactive");
  }
  m_impl = std::make_unique<Savepoint::Impl>();
  m_impl->tx = &tx;
  m_impl->savepoint_name = tx.make_savepoint_name();
  m_impl->tx->exec(std::format("SAVEPOINT {}", m_impl->savepoint_name));
  m_impl->active = true;
};

Savepoint::Savepoint(Transaction &tx, std::string_view name) {
  if (!tx.active()) {
    throw std::logic_error("Transaction is inactive");
  }
  m_impl = std::make_unique<Savepoint::Impl>();
  m_impl->tx = &tx;
  m_impl->savepoint_name = name;
  m_impl->tx->exec(std::format("SAVEPOINT {}", m_impl->savepoint_name));
  m_impl->active = true;
};

Savepoint::Savepoint(Savepoint &&sp) noexcept {
  m_impl = std::move(sp.m_impl);
  sp.m_impl.reset();
};

Savepoint &Savepoint::operator=(Savepoint &&sp) noexcept {
  if (this != &sp) {
    m_impl.reset();
    m_impl = std::move(sp.m_impl);
    sp.m_impl.reset();
  }
  return *this;
};

Savepoint::~Savepoint() {
  try {
    if (m_impl && m_impl->active) {
      rollback();
    }
  } catch (...) {
  }
  m_impl.reset();
};

void Savepoint::rollback() {
  ensure_valid();
  m_impl->tx->exec(
      std::format("ROLLBACK TO SAVEPOINT {}", m_impl->savepoint_name));
  m_impl->active = false;
};

void Savepoint::release() {
  ensure_valid();
  //   m_impl->tx->commit();
  m_impl->tx->exec(std::format("RELEASE SAVEPOINT {}", m_impl->savepoint_name));
  m_impl->active = false;
};

[[nodiscard]] bool Savepoint::active() const noexcept {
  return m_impl == nullptr ? false : m_impl->active;
};

[[nodiscard]] std::string_view Savepoint::name() const noexcept {
  return m_impl == nullptr ? std::string_view{""} : m_impl->savepoint_name;
};

void Savepoint::ensure_valid() {
  if (!m_impl || !m_impl->active || m_impl->tx == nullptr) {
    throw std::logic_error{"Savepoint is not active"};
  }
};

}; // namespace db::tx
