#pragma once

#include <coroutine>
#include <exception>
#include <optional>
#include <stdexcept>
#include <utility>

namespace rtl::coro {


template <typename T>
class Task {
  public:
  struct promise_type {
    std::optional<T> current_value;
    std::exception_ptr exp;
    std::coroutine_handle<> continuation{};
    Task get_return_object() {
      return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
    };

    std::suspend_always initial_suspend() {
      return {};
    };


    template <typename U>
    void return_value(U&& value) {
      current_value.emplace(std::forward<U>(value));
    };

    void unhandled_exception() {
      exp = std::current_exception();
    };

    struct FinalAwaiter {
      bool await_ready() noexcept {
        return false;
      };

      void await_resume() noexcept {};

      std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_type> handle) noexcept {
        if (!handle) {
          return std::noop_coroutine();
        }
        auto cont = handle.promise().continuation;
        if (cont) {
          return cont;
        }
        return std::noop_coroutine();
      }
    };


    FinalAwaiter final_suspend() noexcept {
      return {};
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
    if (m_handle) {
      m_handle.destroy();
    };
  }

  struct Awaiter {
    Handle handle;

    bool await_ready() const noexcept {
      return !handle || handle.done();
    };

    std::coroutine_handle<> await_suspend(std::coroutine_handle<> continuation) noexcept {
      //   if (!handle) {
      //     throw std::runtime_error{"Invalid coroutine"};
      //   }
      handle.promise().continuation = continuation;
      return handle;
    };

    T await_resume() {
      if (!handle) {
        throw std::runtime_error{"Invalid coroutine"};
      }
      auto& promise = handle.promise();
      if (promise.exp) {
        std::rethrow_exception(promise.exp);
      }
      if (!promise.current_value.has_value()) {
        throw std::runtime_error{"Empty value"};
      }
      return std::move(*promise.current_value);
    };
  };

  void resume() {
    if (!m_handle || m_handle.done()) {
      return;
    }
    m_handle.resume();
  };

  bool done() const {
    return !m_handle || m_handle.done();
  };


  T result() {
    if (!done()) {
      throw std::runtime_error{"Coroutine not finished yet"};
    }
    if (m_handle == nullptr) {
      throw std::runtime_error{"Invalid handler"};
    }
    auto& promise = m_handle.promise();
    if (promise.exp) {
      std::rethrow_exception(promise.exp);
    };
    if (!promise.current_value.has_value()) {
      throw std::runtime_error{"Empty value"};
    }
    return std::move(*promise.current_value);
  };

  Awaiter operator co_await() noexcept {
    return Awaiter{m_handle};
  }
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

    std::suspend_always final_suspend() noexcept {
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

  void resume() {
    if (!m_handle || m_handle.done()) {
      return;
    }
    m_handle.resume();
  };

  bool done() const {
    return !m_handle || m_handle.done();
  };
  private:
  Handle m_handle;
};


}; // namespace rtl::coro