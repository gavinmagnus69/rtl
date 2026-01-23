#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>


#include "UnboundedMPMCQueue.h"

// TODO: add periodic task, shutdown mechanism, dynamic thread adding/removing
namespace rtl {
namespace stp {

using Task = std::function<void()>;

class ThreadPool {
public:
    ThreadPool(size_t current_threads = 6, size_t max_threads = 20, size_t max_periodic_threads = 10)
        : m_stopRequested(false)
        , m_activeWorkers(current_threads)
        , m_currentThreadCount(current_threads)
        , m_maxThreadCount(max_threads)
        , m_currentPeriodicThreadCount(0)
        , m_maxPeriodicThreadCount(max_periodic_threads) {
        m_currentThreadCount = std::max((size_t)1, m_currentThreadCount);
        m_maxThreadCount = std::max(m_maxThreadCount, m_currentThreadCount);
        m_maxPeriodicThreadCount = std::max((size_t)1, m_maxPeriodicThreadCount);
        try {
            m_activeWorkers.reserve(m_maxThreadCount);
            m_periodicWorkers.reserve(m_maxPeriodicThreadCount);
            for (size_t i = 0; i < m_currentThreadCount; ++i) {
                add_thread();
            }
        } catch (const std::exception& exp) {
            std::cerr << exp.what() << '\n';
            throw;
        } catch (...) {
            std::cerr << "Unknown exception in ThreadPool constructor\n";
            throw;
        }
    }
    ~ThreadPool() {
        std::cout << "ThreadPool destructor called\n";
        request_stop();
        blocking_thread_stopping();
    }
public:
    template <typename F, typename... Args>
    auto put(F&& func, Args&&... args) -> std::future<typename std::invoke_result<F, Args...>::type> {
        if (m_stopRequested.load(std::memory_order_relaxed)) {
            throw std::runtime_error("ThreadPool has been stopped, cannot add new tasks.");
        }
        using ReturnType = typename std::invoke_result<F, Args...>::type; // this return type of the function
        auto tasking = std::make_shared<std::packaged_task<ReturnType()>>(std::bind(std::forward<F>(func), std::forward<Args>(args)...));
        auto returnFuture = tasking->get_future();
        try {
            auto closureTask = [tasking]() mutable { (*tasking)(); };
            m_taskQueue.put(std::move(closureTask));
            return returnFuture;
        } catch (const std::exception& exp) {
            std::cerr << exp.what() << '\n';
            // returnFuture.set_exception(std::current_exception());
        } catch (...) {
            std::cerr << "Unknown exception in ThreadPool::put\n";
        }
        return returnFuture;
    }
    template <typename F, typename... Args>
    void put_periodic(size_t repeat_time_ms, F&& func, Args&&... args) {
        if (m_currentPeriodicThreadCount >= m_maxPeriodicThreadCount) {
            std::cerr << "Max periodic thread count reached\n";
            return;
        }
        try {
            add_periodic_thread(repeat_time_ms, std::bind(std::forward<F>(func), std::forward<Args>(args)...)); // copy of task?
        } catch (const std::exception& exp) {
            std::cerr << exp.what() << '\n';
        } catch (...) {
            std::cerr << "Unknown exception in ThreadPool::put_periodic\n";
        }
    }
    void request_stop() {
        m_taskQueue.request_stop();                             // sends stop signal to the queue, will return nullopt on tryTake
        m_stopRequested.store(true, std::memory_order_relaxed); // signal threads to stop
    }
private:
    void blocking_thread_stopping() {
        for (size_t i = 0; i < m_periodicWorkers.size(); ++i) {
            if (m_periodicWorkers[i].joinable()) {
                m_periodicWorkers[i].join(); // or detach, depending on desired behavior
            }
        }
        for (size_t i = 0; i < m_activeWorkers.size(); ++i) {
            if (m_activeWorkers[i].joinable()) {
                m_activeWorkers[i].join(); // or join, depending on desired behavior
            }
        }
        // m_currentThreadCount = 0;
    }
    void add_thread() {
        // TODO: add thread safely and checking vectors size and capacity
        m_activeWorkers.emplace_back([this]() {
            while (true) {
                if (m_stopRequested.load(std::memory_order_relaxed)) {
                    break;
                }
                auto task = m_taskQueue.tryTake();
                if (!task.has_value()) {
                    continue;
                }
                if (!task.value()) {
                    continue;
                }
                task.value()();
            }
        });
    }
    void add_periodic_thread(size_t task_interval_ms, Task&& task) {
        if (!task) {
            std::cerr << "Empty task\n";
            return;
        }
        if (task_interval_ms == 0) {
            std::cerr << "Zero interval\n";
            return;
        }
        ++m_currentPeriodicThreadCount;
        // std::move? task
        m_periodicWorkers.emplace_back([this, t = std::move(task), task_interval_ms]() {
            while (true) {
                if (m_stopRequested.load(std::memory_order_relaxed)) {
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(task_interval_ms)); // change it on cv wait later
                t();
            }
        });
    }
    std::atomic<bool> m_stopRequested{false};
    std::vector<std::thread> m_activeWorkers{};
    std::vector<std::thread> m_periodicWorkers{};
    size_t m_currentThreadCount{0};
    size_t m_maxThreadCount{0};
    size_t m_currentPeriodicThreadCount{0};
    size_t m_maxPeriodicThreadCount{0};
    UnboundedMPMCQueue<Task> m_taskQueue{}; // add container variation
};

}; // namespace stp


}; // namespace rtl