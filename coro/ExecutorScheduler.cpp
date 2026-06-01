#include "ExecutorScheduler.hpp"


namespace rtl::coro {

ExecutorScheduler::ExecutorScheduler(std::shared_ptr<stp::IExecutor> executor) noexcept
    : m_executor(std::move(executor)) {};

ExecutorScheduler::~ExecutorScheduler() {};

bool ExecutorScheduler::post(std::coroutine_handle<> handle) noexcept {
  if (!m_executor) {
    return false;
  }
};

}; // namespace rtl::coro