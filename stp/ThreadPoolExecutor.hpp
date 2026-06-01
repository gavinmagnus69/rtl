#pragma once

#include <memory>

#include "IExecutor.hpp"
#include "RtlThreadPool.hpp"

namespace rtl {
namespace stp {

struct ThreadPoolExecutor final : public IExecutor {
public:
  virtual ~ThreadPoolExecutor() override {
  }

  explicit ThreadPoolExecutor(const ThreadPoolOptions& opt) {
    m_threadPool = std::make_unique<ThreadPool>(opt);
  };

  ThreadPoolExecutor(size_t current_threads = 6, size_t max_threads = 20) {
    m_threadPool = std::make_unique<ThreadPool>(current_threads, max_threads, max_threads);
  }
protected:
  virtual bool post(Task&& task, TaskOptions opt) noexcept final {
    if (!m_threadPool) {
      return false;
    }
    if (!task) {
      return false;
    }
    try {
      if (opt.is_periodic) {
        // periodic case
        m_threadPool->put_periodic(opt.periodic_interval_ms, std::move(task));
        return true;
      }
      (void)m_threadPool->put(std::move(task));
      return true;
    } catch (...) {
      return false;
    };
  }
private:
  std::unique_ptr<ThreadPool> m_threadPool;
};

auto makeThreadPoolExecutor(const ThreadPoolOptions& opt) -> std::unique_ptr<IExecutor>;

auto makeThreadPoolExecutor(size_t current_threads = 6, size_t max_threads = 20) -> std::unique_ptr<IExecutor>;

}; // namespace stp
}; // namespace rtl
