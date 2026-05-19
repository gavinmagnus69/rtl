#pragma once

#include <coroutine>
#include <exception>
#include <optional>

namespace rtl::coro {


template <typename T>
class Task {
  public:
  struct promise_type {
    std::optional<T> current_value;
    std::exception_ptr exp;

    Task get_return_object() {
      return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
    };

    std::suspend_always initial_suspend() {
      return {};
    };

    std::suspend_always final_suspend() {
      return {};
    };

    void return_value(T value) {
      current_value = value;
    };

    void unhandled_exception() {
      exp = std::current_exception();
    };
  };

  using Handle = std::coroutine_handle<promise_type>;

  explicit Task(Handle handle)
      : m_handle(handle) {
  }

  Task(Task&& task) noexcept {
    m_handle = std::move(task.m_handle);
    task.m_handle = nullptr;
  };

  Task(const Task&) = delete;

  Task& operator=(const Task&) = delete;

  Task& operator=(Task&& task) noexcept {
    if (&task != &*this) {
      if (m_handle) {
        m_handle.destroy();
      }
      m_handle = std::move(task.m_handle);
      task.m_handle = nullptr;
    }
    return *this;
  };

  ~Task() {
    try {
      if (m_handle) {
        m_handle.destroy();
      }
    } catch (...) {
    }
  };
  private:
  Handle m_handle;
};


template <>
class Task<void> {
  public:
  struct promise_type {
    std::exception_ptr exp;

    Task get_return_object() {
      return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
    };

    std::suspend_always initial_suspend() {
      return {};
    }

    std::suspend_always final_suspend() {
      return {};
    };

    void return_void() {};

    void unhandled_exception() {
      exp = std::current_exception();
    };
  };


  using Handle = std::coroutine_handle<promise_type>;

  explicit Task(Handle handle)
      : m_handle(handle) {
  }

  Task(Task&& task) noexcept {
    m_handle = std::move(task.m_handle);
    task.m_handle = nullptr;
  };

  Task(const Task&) = delete;

  Task& operator=(const Task&) = delete;

  Task& operator=(Task&& task) noexcept {
    if (&task != &*this) {
      if (m_handle) {
        m_handle.destroy();
      }

      m_handle = std::move(task.m_handle);
      task.m_handle = nullptr;
    }
    return *this;
  };

  ~Task() {
    try {
      if (m_handle) {
        m_handle.destroy();
      }
    } catch (...) {
    }
  };
  private:
  Handle m_handle;
};


}; // namespace rtl::coro