#include "Schedule.hpp"
#include "CoroExceptions.hpp"

namespace rtl::coro {

ScheduleAwaiter::ScheduleAwaiter(ExecutorScheduler& scheduler) noexcept
    : r_scheduler(scheduler) {};

bool ScheduleAwaiter::await_ready() const noexcept {
  return false;
};

void ScheduleAwaiter::await_suspend(std::coroutine_handle<> handle) {
  if (!handle) {
    throw InvalidTask{"Invalid coroutine handle"};
  }

  bool post_result = r_scheduler.post(handle);
  if (!post_result) {
    throw ScheduleRejected{"Post rejected"};
  }
};

void ScheduleAwaiter::await_resume() const noexcept {};


ScheduleAwaiter schedule(ExecutorScheduler& scheduler) noexcept {
  return ScheduleAwaiter{scheduler};
};

ScheduleAwaiter resume_on(ExecutorScheduler& scheduler) noexcept {
  return schedule(scheduler);
};

}; // namespace rtl::coro