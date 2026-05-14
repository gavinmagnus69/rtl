#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

#include "StpExceptions.hpp"
#include "UnbMpMcTemplateQueue.hpp"

namespace rtl {
namespace stp {

enum class RejectionPolicy { throw_exception, block, block_for, caller_runs };

struct ThreadPoolOptions {
  size_t workers_count{6};
  size_t max_workers{20};
  size_t max_periodic_tasks{10};
  size_t max_queue_size{0}; // 0 == unbounded
  RejectionPolicy rejection_policy{RejectionPolicy::throw_exception};
  size_t enqueue_timeout_ms{0}; // for block_for rejection_policy
};

using Task = std::function<void()>;

class ThreadPool {
  public:
  using QueueType = UnbMpMcTemplateQueue<Task>;

  enum class PoolState : uint8_t { running, stopping, stopped };

  struct ThreadPoolStats {
    PoolState state{PoolState::running};
    size_t workers_count{0};
    size_t max_workers{0};
    size_t periodic_workers_count{0}; // created periodic workers
    size_t max_periodic_tasks{0};
    size_t queued_tasks{0};
    size_t max_queue_size{0};
  };

  ThreadPool(size_t current_threads = 6, size_t max_threads = 20, size_t max_periodic_threads = 10) {
    m_opt = ThreadPoolOptions{.workers_count = current_threads, .max_workers = max_threads, .max_periodic_tasks = max_periodic_threads};

    setup_threads();
  };

  explicit ThreadPool(const ThreadPoolOptions& opt)
      : m_opt(opt) {
    setup_threads();
  };

  ~ThreadPool() {
    try {
      join();
    } catch (...) {
    };
  };
  public:
  //   throws exceptions
  template <typename F, typename... Args>
  [[nodiscard]] auto put(F&& func, Args&&... args) -> std::future<typename std::invoke_result<F, Args...>::type> {

    if (m_poolState.load() != PoolState::running) {
      throw ThreadPoolStopped{};
    };
    using ReturnType = typename std::invoke_result<F, Args...>::type; // this return type of
                                                                      // the function
    auto tasking = std::make_shared<std::packaged_task<ReturnType()>>(std::bind(std::forward<F>(func), std::forward<Args>(args)...));
    auto returnFuture = tasking->get_future();
    try {
      auto closureTask = [tasking]() mutable { (*tasking)(); };
      QueueType::PutResultErrorCode put_erc{QueueType::PutResultErrorCode::accepted};
      switch (m_opt.rejection_policy) {
      case RejectionPolicy::throw_exception:
        put_erc = m_taskQueue.put(std::move(closureTask));
        break;
      case RejectionPolicy::block:
        put_erc = m_taskQueue.put_wait(std::move(closureTask));
        break;
      case RejectionPolicy::block_for:
        put_erc = m_taskQueue.put_wait_for(std::move(closureTask), m_opt.enqueue_timeout_ms);
        break;
      case RejectionPolicy::caller_runs:
        put_erc = m_taskQueue.put(closureTask); // passing copy
        if (put_erc == QueueType::PutResultErrorCode::accepted) {
          return returnFuture;
        }
        if (put_erc == QueueType::PutResultErrorCode::closed) {
          throw ThreadPoolStopped{};
        }
        if (put_erc == QueueType::PutResultErrorCode::full) {
          closureTask();
          return returnFuture;
        }
        break;
      };
      if (put_erc == QueueType::PutResultErrorCode::accepted) {
        return returnFuture;
      }
      if (put_erc == QueueType::PutResultErrorCode::closed) {
        throw ThreadPoolStopped{};
      }
      if (put_erc == QueueType::PutResultErrorCode::full) {
        throw QueueFull{};
      }
      if (put_erc == QueueType::PutResultErrorCode::timeout) {
        throw TaskRejected{"Rejected on timeout"};
      }

    } catch (...) {
      throw;
    }

    return returnFuture;
  };
  //   throws exceptions
  template <typename F, typename... Args>
  void put_periodic(size_t repeat_time_ms, F&& func, Args&&... args) {
    {
      std::unique_lock<std::mutex> lock(m_mtx);
      if (m_poolState.load() != PoolState::running) {
        throw ThreadPoolStopped{};
      };

      try {
        if (add_periodic_thread(repeat_time_ms, std::bind(std::forward<F>(func), std::forward<Args>(args)...))) {
          return;
        };
        throw TaskRejected{};
      } catch (...) {
        throw;
      }
    }
    throw TaskRejected{};
  };

  [[deprecated]]
  void request_stop() {
    shutdown_graceful();
  };

  // stops accepting new tasks, wakes blocked submitters/periodic workers, and
  // drains already accepted queued tasks
  void shutdown_graceful() {
    std::unique_lock<std::mutex> lock{m_mtx};
    if (m_poolState.load() != PoolState::running) {
      return;
    }
    m_poolState.store(PoolState::stopping);
    m_cv.notify_all();
    m_taskQueue.close();
  };

  // Calls graceful shutdown, blocks until all workers exit, is idempotent, and
  // terminates on self-join.
  void join() {
    shutdown_graceful();
    {
      std::unique_lock<std::mutex> lock{m_joinMtx};
      if (m_poolState.load() == PoolState::stopped) {
        return;
      }
      blocking_thread_stopping();
    }
  };

  [[nodiscard]] size_t get_current_thread_count() const {
    return m_opt.workers_count;
  }

  [[nodiscard]] PoolState state() const noexcept {
    std::lock_guard<std::mutex> lock(m_mtx);
    return m_poolState;
  };

  [[nodiscard]] bool is_running() const noexcept {
    std::lock_guard<std::mutex> lock(m_mtx);
    return m_poolState == PoolState::running;
  };

  [[nodiscard]] bool is_stopping() const noexcept {
    std::lock_guard<std::mutex> lock(m_mtx);
    return m_poolState == PoolState::stopping;
  };

  [[nodiscard]] bool is_stopped() const noexcept {
    std::lock_guard<std::mutex> lock(m_mtx);
    return m_poolState == PoolState::stopped;
  };

  [[nodiscard]] ThreadPoolStats stats() const noexcept {
    std::lock_guard<std::mutex> lock(m_mtx);
    ThreadPoolStats stats = m_stats;
    stats.max_periodic_tasks = m_opt.max_periodic_tasks;
    stats.workers_count = m_opt.workers_count;
    stats.max_workers = m_opt.max_workers;
    stats.max_queue_size = m_opt.max_queue_size;
    stats.queued_tasks = m_taskQueue.size();
    stats.state = m_poolState;
    return stats;
  };
  private:
  void setup_threads() {
    m_taskQueue.set_queue_max_size(m_opt.max_queue_size);
    m_opt.workers_count = std::max((size_t)1, m_opt.workers_count);
    m_opt.max_workers = std::max(m_opt.max_workers, m_opt.workers_count);
    m_opt.max_periodic_tasks = std::max((size_t)1, m_opt.max_periodic_tasks);
    try {
      m_activeWorkers.reserve(m_opt.max_workers);
      m_periodicWorkers.reserve(m_opt.max_periodic_tasks);
      for (size_t i = 0; i < m_opt.workers_count; ++i) {
        add_thread();
      }
    } catch (...) {
      join();
      throw;
    }
  };

  // Terminates if called from a worker owned by this pool.
  void blocking_thread_stopping() {
    auto caller_thread_id = std::this_thread::get_id();
    for (size_t i = 0; i < m_periodicWorkers.size(); ++i) {
      if (m_periodicWorkers[i].get_id() == caller_thread_id) {
        std::terminate();
      }
      if (m_periodicWorkers[i].joinable()) {
        m_periodicWorkers[i].join();
      }
    }
    for (size_t i = 0; i < m_activeWorkers.size(); ++i) {
      if (m_activeWorkers[i].get_id() == caller_thread_id) {
        std::terminate();
      }
      if (m_activeWorkers[i].joinable()) {
        m_activeWorkers[i].join();
      }
    }
    m_poolState.store(PoolState::stopped);
  };

  void add_thread() {
    m_activeWorkers.emplace_back([this]() {
      while (true) {
        auto task = m_taskQueue.tryTake();
        if (task.erc == TakeResultErrorCode::closed) {
          return;
        }
        if (task.erc == TakeResultErrorCode::timeout) {
          continue;
        }

        if (!task.result.has_value()) {
          continue;
        }
        if (!task.result.value()) {
          continue;
        }
        task.result.value()();
      }
    });
  };

  [[nodiscard]] bool add_periodic_thread(size_t task_interval_ms, Task&& task) {

    if (m_stats.periodic_workers_count >= m_opt.max_periodic_tasks) {
      return false;
    }
    if (!task) {
      return false;
    }
    if (task_interval_ms == 0) {
      return false;
    }

    m_periodicWorkers.emplace_back([this, t = std::move(task), task_interval_ms]() {
      while (true) {
        if (m_poolState.load() != PoolState::running) {
          return;
        }
        std::unique_lock<std::mutex> lock{m_mtx};
        m_cv.wait_for(lock, std::chrono::milliseconds(task_interval_ms), [this]() -> bool { return m_poolState.load() == PoolState::stopping; });
        lock.unlock();
        if (m_poolState.load() != PoolState::running) {
          return;
        }
        try {
          if (!t) {
            return;
          }
          t();
        } catch (...) {
          continue;
        };
      }
    });
    ++m_stats.periodic_workers_count;

    return true;
  };

  QueueType m_taskQueue{};
  std::condition_variable m_cv;
  mutable std::mutex m_mtx;
  mutable std::mutex m_joinMtx;
  std::vector<std::thread> m_activeWorkers{};
  std::vector<std::thread> m_periodicWorkers{};
  ThreadPoolOptions m_opt{};
  ThreadPoolStats m_stats;
  std::atomic<PoolState> m_poolState{PoolState::running};
};

}; // namespace stp

}; // namespace rtl
