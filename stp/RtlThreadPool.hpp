#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <exception>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

#include "UnbMpMcTemplateQueue.hpp"

// TODO: add periodic task, shutdown mechanism, dynamic thread adding/removing
namespace rtl {
namespace stp {

using Task = std::function<void()>;

class ThreadPool {
public:
  ThreadPool(size_t current_threads = 6, size_t max_threads = 20,
             size_t max_periodic_threads = 10)
      : m_currentThreadCount(current_threads), m_maxThreadCount(max_threads),
        m_currentPeriodicThreadCount(0),
        m_maxPeriodicThreadCount(max_periodic_threads) {
    m_currentThreadCount = std::max((size_t)1, m_currentThreadCount.load());
    m_maxThreadCount = std::max(m_maxThreadCount, m_currentThreadCount.load());
    m_maxPeriodicThreadCount = std::max((size_t)1, m_maxPeriodicThreadCount);
    try {
      m_activeWorkers.reserve(m_maxThreadCount);
      m_periodicWorkers.reserve(m_maxPeriodicThreadCount);
      for (size_t i = 0; i < m_currentThreadCount; ++i) {
        add_thread();
      }
    } catch (const std::exception &exp) {
      std::cerr << exp.what() << '\n';
      throw;
    } catch (...) {
      std::cerr << "Unknown exception in ThreadPool constructor\n";
      throw;
    }
  };

  ~ThreadPool() {
    std::cout << "ThreadPool destructor called\n";
    request_stop();
    blocking_thread_stopping();
  };

public:
  //   throws exceptions
  template <typename F, typename... Args>
  auto put(F &&func, Args &&...args)
      -> std::future<typename std::invoke_result<F, Args...>::type> {

    if (m_poolState.load() != PoolState::running) {
      throw std::runtime_error{"TP is stopping or stopped"};
    };
    using ReturnType =
        typename std::invoke_result<F, Args...>::type; // this return type of
                                                       // the function
    auto tasking = std::make_shared<std::packaged_task<ReturnType()>>(
        std::bind(std::forward<F>(func), std::forward<Args>(args)...));
    auto returnFuture = tasking->get_future();
    try {
      auto closureTask = [tasking]() mutable { (*tasking)(); };
      bool put_result = m_taskQueue.put(std::move(closureTask));
      if (!put_result) {
        throw std::runtime_error{"Queue is closed"};
      }
      return returnFuture;
    } catch (const std::exception &exp) {
      std::cerr << exp.what() << '\n';
      // returnFuture.set_exception(std::current_exception());
      throw;
    } catch (...) {
      std::cerr << "Unknown exception in ThreadPool::put\n";
      throw;
    }

    return returnFuture;
  };
  //   throws exceptions
  template <typename F, typename... Args>
  void put_periodic(size_t repeat_time_ms, F &&func, Args &&...args) {
    {
      std::unique_lock<std::mutex> lock(m_mtx);
      if (m_poolState.load() != PoolState::running) {
        throw std::runtime_error{"TP is stopping or stopped"};
        // return false;
      };

      try {
        if (add_periodic_thread(repeat_time_ms,
                                std::bind(std::forward<F>(func),
                                          std::forward<Args>(args)...))) {
          // success case
          return;
        }; // copy of task?
        throw std::runtime_error{"Failed to add periodic thread"};
      } catch (const std::exception &exp) {
        std::cerr << exp.what() << '\n';
        // return false;
        throw;
      } catch (...) {
        std::cerr << "Unknown exception in ThreadPool::put_periodic\n";
        throw;
      }
    }
    throw std::runtime_error{"Exception in put_periodic"};
  };

  void request_stop() {
    std::unique_lock<std::mutex> lock{m_mtx};
    m_poolState.store(PoolState::stopping);
    m_cv.notify_all();
    m_taskQueue.close(); // sends stop signal to the queue, queue will not new
    // task and continue work unless its empty
  };

  size_t getCurrentThreadCount() const { return m_currentThreadCount; }

private:
  void blocking_thread_stopping() {
    auto caller_thread_id = std::this_thread::get_id();
    for (size_t i = 0; i < m_periodicWorkers.size(); ++i) {
      if (m_periodicWorkers[i].get_id() == caller_thread_id) {
        std::terminate();
      }
      if (m_periodicWorkers[i].joinable()) {
        m_periodicWorkers[i].join(); // or detach, depending on desired behavior
      }
    }
    for (size_t i = 0; i < m_activeWorkers.size(); ++i) {
      if (m_activeWorkers[i].get_id() == caller_thread_id) {
        std::terminate();
      }
      if (m_activeWorkers[i].joinable()) {
        m_activeWorkers[i].join(); // or join, depending on desired behavior
      }
    }
    m_poolState.store(PoolState::stopped);
    // m_currentThreadCount = 0;
  };

  void add_thread() {
    // TODO: add thread safely and checking vectors size and capacity
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

  bool add_periodic_thread(size_t task_interval_ms, Task &&task) {

    if (m_currentPeriodicThreadCount >= m_maxPeriodicThreadCount) {
      std::cerr << "Max periodic thread count reached\n";
      return false;
    }
    if (!task) {
      std::cerr << "Empty task\n";
      return false;
    }
    if (task_interval_ms == 0) {
      std::cerr << "Zero interval\n";
      return false;
    }

    // std::move? task
    m_periodicWorkers.emplace_back(
        [this, t = std::move(task), task_interval_ms]() {
          while (true) {
            if (m_poolState.load() != PoolState::running) {
              return;
            }
            std::unique_lock<std::mutex> lock{m_mtx};
            m_cv.wait_for(lock, std::chrono::milliseconds(task_interval_ms),
                          [this]() -> bool {
                            return m_poolState.load() == PoolState::stopping;
                          });
            lock.unlock();
            if (m_poolState.load() != PoolState::running) {
              return;
            }
            t();
          }
        });
    m_currentPeriodicThreadCount.fetch_add(1);

    return true;
  };

  enum class PoolState : uint8_t { running, stopping, stopped };

  UnbMpMcTemplateQueue<Task> m_taskQueue{}; // add container variation
  std::condition_variable m_cv;
  mutable std::mutex m_mtx;
  std::vector<std::thread> m_activeWorkers{};
  std::vector<std::thread> m_periodicWorkers{};
  std::atomic<PoolState> m_poolState{PoolState::running};
  std::atomic<size_t> m_currentThreadCount{0};
  size_t m_maxThreadCount{0};
  std::atomic<size_t> m_currentPeriodicThreadCount{0};
  size_t m_maxPeriodicThreadCount{0};
};

}; // namespace stp

}; // namespace rtl