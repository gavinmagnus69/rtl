#include "ExecutorScheduler.hpp"

#include <utility>

namespace rtl::coro {

ExecutorScheduler::ExecutorScheduler(std::shared_ptr<stp::IExecutor> executor) noexcept
    : m_executor(std::move(executor)) {};

ExecutorScheduler::~ExecutorScheduler() {};

bool ExecutorScheduler::post(std::coroutine_handle<> handle) noexcept {
  if (!m_executor) {
    return false;
  }
  try {
    if (!handle) {
      return false;
    }
    return m_executor->submit_fire_and_forget(stp::Task{[handle]() mutable {
      if (!handle.done()) {
        handle.resume();
      }
    }});
  } catch (...) {
    return false;
  }
};

}; // namespace rtl::coro