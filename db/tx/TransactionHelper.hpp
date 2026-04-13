#pragma once

#include <cstddef>
#include <functional>
#include <stdexcept>
#include <type_traits>

#include "IsolationLevel.hpp"
#include "Transaction.hpp"

#include "Errors.hpp"

#include "ConnectionPool.hpp"

namespace db::tx {

static const size_t g_max_retries = 10;

struct TransactionHelper {
  //   TransactionHelper(size_t max_retries = g_max_retries);
  template <typename Fn>
  auto with_retry(db::pool::ConnectionPool &pool, IsolationLevel level,
                  Fn &&fn) -> std::invoke_result_t<Fn &&, Transaction &> {
    using invoke_result_t = std::invoke_result_t<Fn &&, Transaction &>;
    if (m_maxRetries == 0) {
      throw std::logic_error{"Invalid number of retries"};
    }
    for (size_t i = 0; i < m_maxRetries; ++i) {
      Transaction tx{pool, level};
      try {
        if constexpr (std::is_void_v<invoke_result_t>) {
          std::forward<Fn>(fn)(tx);
          tx.commit();
          return;
        } else {
          auto result = std::forward<Fn>(fn)(tx);
          tx.commit();
          return result;
        };
      } catch (const core::SerializationFailure &fail) {
        if (tx.active()) {
          tx.rollback();
        }
        if (i == m_maxRetries - 1) {
          throw;
        }
      } catch (const core::DeadlockDetected &fail) {
        if (tx.active()) {
          tx.rollback();
        }
        if (i == m_maxRetries - 1) {
          throw;
        }
      }
    };
    throw std::runtime_error{"Failed"};
  };
  size_t m_maxRetries{g_max_retries};
};
}; // namespace db::tx