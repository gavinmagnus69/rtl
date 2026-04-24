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
  explicit ThreadPoolStopped()
      : rtl::stp::ThreadPoolError(rtl::stp::ErrorCode::pool_stopped,
                                  "Thread pool is stopped") {};

  explicit ThreadPoolStopped(std::string_view message)
      : rtl::stp::ThreadPoolError(rtl::stp::ErrorCode::pool_stopped, message) {}
};

class TaskRejected : public ThreadPoolError {
public:
  explicit TaskRejected()
      : rtl::stp::ThreadPoolError(rtl::stp::ErrorCode::task_rejected,
                                  "Task rejected") {};
  explicit TaskRejected(std::string_view message)
      : rtl::stp::ThreadPoolError(rtl::stp::ErrorCode::task_rejected, message) {
  }
};

class QueueClosed : public ThreadPoolError {
public:
  explicit QueueClosed()
      : rtl::stp::ThreadPoolError(rtl::stp::ErrorCode::queue_closed,
                                  "Queue is closed") {}
  explicit QueueClosed(std::string_view message)
      : rtl::stp::ThreadPoolError(rtl::stp::ErrorCode::queue_closed, message) {}
};

class QueueFull : public ThreadPoolError {
public:
  explicit QueueFull()
      : rtl::stp::ThreadPoolError(rtl::stp::ErrorCode::queue_full,
                                  "Queue is full") {}
  explicit QueueFull(std::string_view message)
      : rtl::stp::ThreadPoolError(rtl::stp::ErrorCode::queue_full, message) {}
};

class InvalidPoolOptions : public ThreadPoolError {
public:
  explicit InvalidPoolOptions()
      : rtl::stp::ThreadPoolError(rtl::stp::ErrorCode::invalid_pool_options,
                                  "Invalid pool options") {}
  explicit InvalidPoolOptions(std::string_view message)
      : rtl::stp::ThreadPoolError(rtl::stp::ErrorCode::invalid_pool_options,
                                  message) {}
};

}; // namespace rtl::stp