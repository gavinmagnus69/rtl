#pragma once

#include <stdexcept>
#include <string>
#include <string_view>

namespace rtl::stp {

enum class ErrorCode {
  unknown,
  pool_stopped,
  task_rejected,
  queue_closed,
  queue_full,
  invalid_pool_options
};

class ThreadPoolError : public std::runtime_error {
public:
  ThreadPoolError(ErrorCode code, std::string_view message)
      : std::runtime_error(std::string(message)), m_code(code) {};
  [[nodiscard]] ErrorCode code() const noexcept { return m_code; }

private:
  ErrorCode m_code{rtl::stp::ErrorCode::unknown};
};

class ThreadPoolStopped : public ThreadPoolError {
public:
  ThreadPoolStopped()
      : rtl::stp::ThreadPoolError(rtl::stp::ErrorCode::pool_stopped,
                                  "Thread pool is stopped") {};

  explicit ThreadPoolStopped(std::string_view message)
      : rtl::stp::ThreadPoolError(rtl::stp::ErrorCode::pool_stopped, message) {}
};

class TaskRejected : public ThreadPoolError {
public:
  TaskRejected(std::string_view message)
      : rtl::stp::ThreadPoolError(rtl::stp::ErrorCode::task_rejected, message) {
  }
};

class QueueClosed : public ThreadPoolError {
public:
  QueueClosed(std::string_view message)
      : rtl::stp::ThreadPoolError(rtl::stp::ErrorCode::queue_closed, message) {}
};

}; // namespace rtl::stp