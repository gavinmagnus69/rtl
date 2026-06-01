#pragma once

#include <stdexcept>
#include <string>
#include <string_view>


namespace rtl::coro {

enum class ErrorCode { unknown, invalid_task, task_not_ready, empty_result, schedule_rejected };


class CoroError : public std::runtime_error {
public:
  CoroError(ErrorCode code, std::string_view message)
      : std::runtime_error(std::string{message})
      , m_erc(code) {};
  [[nodiscard]] ErrorCode code() const noexcept {
    return m_erc;
  };
private:
  ErrorCode m_erc{ErrorCode::unknown};
};


class InvalidTask : public CoroError {
public:
  explicit InvalidTask(std::string_view message = "Invalid task")
      : CoroError(ErrorCode::invalid_task, message) {};
};


class TaskNotReady : public CoroError {
public:
  explicit TaskNotReady(std::string_view message = "Task not ready")
      : CoroError(ErrorCode::task_not_ready, message) {};
};


class EmptyResult : public CoroError {
public:
  explicit EmptyResult(std::string_view message = "Empty result")
      : CoroError(ErrorCode::empty_result, message) {};
};


class ScheduleRejected : public CoroError {
public:
  explicit ScheduleRejected(std::string_view message = "Schedule rejected")
      : CoroError(ErrorCode::schedule_rejected, message) {};
};


}; // namespace rtl::coro