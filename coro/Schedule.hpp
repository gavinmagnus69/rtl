#pragma once

#include <coroutine>

#include "ExecutorScheduler.hpp"

namespace rtl::coro {
class ScheduleAwaiter {
public:
  explicit ScheduleAwaiter(ExecutorScheduler& scheduler) noexcept;
  bool await_ready() const noexcept;
  void await_suspend(std::coroutine_handle<> handle);
  void await_resume() const noexcept;
private:
  ExecutorScheduler& r_scheduler;
};

ScheduleAwaiter schedule(ExecutorScheduler& scheduler) noexcept;

ScheduleAwaiter resume_on(ExecutorScheduler& scheduler) noexcept;

}; // namespace rtl::coro