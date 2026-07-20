#pragma once

#include <chrono>
#include <condition_variable>
#include <functional>
#include <utility>


#include "IExecutor.hpp"


namespace rtl::stp {


class RtlTimer {
public:
  enum class Result { expired, cancelled };
  using clock = std::chrono::steady_clock;
  using Handler = std::function<void(Result)>;
public:
  explicit RtlTimer(std::shared_ptr<IExecutor> executor)
      : m_executor(std::move(executor)) {
    if (m_executor) {
      m_timerThreadFuture = m_executor->submit(TaskOptions{}, [this]() { run_timer(); });
    };
  };

  ~RtlTimer() {
    try {
      shutdown();
      m_timerThreadFuture.get();
    } catch (...) {
    };
  };

  RtlTimer(const RtlTimer&) = delete;
  RtlTimer& operator=(const RtlTimer&) = delete;
  //   TODO: add move const and operator
  void expires_at(clock::time_point time_point) {
    {
      std::lock_guard lock{m_mtx};
      m_expiry = time_point;
      ++m_version;
    }
    m_cv.notify_all();
  };

  void expires_after(clock::duration duration) {
    expires_at(clock::now() + duration);
  };

  bool async_wait(Handler handler) {
    {
      std::lock_guard lock{m_mtx};
      if (m_stopping || m_pending) {
        return false;
      }
      m_handler = std::move(handler);
      m_pending = true;
      m_cancelRequested = false;
      ++m_version;
    }
    m_cv.notify_all();
    return true;
  }; // namespace rtl::stp

  bool cancel() {
    {
      std::lock_guard lock{m_mtx};
      if (!m_pending) {
        return false;
      }
      m_cancelRequested = true;
      ++m_version;
    }
    m_cv.notify_all();
    return true;
  }; // namespace rtl::stp

  void shutdown() {
    {
      std::lock_guard lock{m_mtx};
      if (m_stopping) {
        return;
      }
      m_stopping = true;
      m_pending = false;
      m_handler = {};
      ++m_version;
    }
    m_cv.notify_all();
    return;
  }; // namespace rtl::stp
private:
  void run_timer() {
    for (;;) {
      Handler handler;
      Result result;
      {
        std::unique_lock lock{m_mtx};
        m_cv.wait(lock, [this]() { return m_stopping || m_pending; });
        if (m_stopping) {
          return;
        }
        for (;;) {
          const auto deadline = m_expiry;
          const auto version = m_version;
          const bool woke_by_state_change = m_cv.wait_until(lock, deadline, [this, version]() { return m_stopping || !m_pending || m_cancelRequested || m_version != version; });
          if (m_stopping) {
            return;
          }
          if (!m_pending) {
            break;
          }
          if (m_cancelRequested) {
            m_pending = false;
            m_cancelRequested = false;
            handler = std::move(m_handler);
            result = Result::cancelled;
            break;
          }
          //   state changed
          if (version != m_version) {
            continue;
          }
          //   expired
          if (!woke_by_state_change) {
            m_pending = false;
            handler = std::move(m_handler);
            result = Result::expired;
            break;
          }
        };
      }
      if (handler) {
        run_handler_on_executor(std::move(handler), result);
      };
    };
  };

  void run_handler_on_executor(Handler&& handler, Result result) {
    if (!m_executor) {
      return;
    };
    m_executor->submit(TaskOptions{}, std::move(handler), result);
  };
private:
  mutable std::mutex m_mtx{};
  std::condition_variable m_cv{};
  Handler m_handler{};
  clock::time_point m_expiry = clock::now();
  bool m_pending{false};
  bool m_cancelRequested{false};
  bool m_stopping{false};
  size_t m_version{0};
  std::future<void> m_timerThreadFuture;
  std::shared_ptr<IExecutor> m_executor{nullptr};
}; // namespace rtl::stp
}; // namespace rtl::stp