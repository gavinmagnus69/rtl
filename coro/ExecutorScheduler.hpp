#pragma once

#include "IExecutor.hpp"

#include <coroutine>

namespace rtl::coro {
class ExecutorScheduler {
public:
  explicit ExecutorScheduler(std::shared_ptr<stp::IExecutor>) noexcept;
  ~ExecutorScheduler();
  bool post(std::coroutine_handle<> handle) noexcept;
private:
  std::shared_ptr<stp::IExecutor> m_executor{nullptr};
};
}; // namespace rtl::coro