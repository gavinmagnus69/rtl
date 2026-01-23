#pragma once

#include <cassert>


#include "IExecutor.hpp"
#include "RtlThreadPool.hpp"


namespace rtl {
namespace stp {


struct ThreadPoolExecutor final : public IExecutor {
public:
    virtual ~ThreadPoolExecutor() override {

        // assert(false && "TODO");
        //  RAII threadPool destruction
    }
    ThreadPoolExecutor(size_t current_threads = 6, size_t max_threads = 20) {
        m_threadPool = std::make_unique<ThreadPool>(current_threads, max_threads, max_threads);
        // assert(false && "TODO");
    }
protected:
    virtual bool post(Task&& task, TaskOptions opt) final {
        if (!m_threadPool) {
            return false;
        }
        if (!task) {
            return false;
        }
        if (opt.is_periodic) {
            // periodic case
            m_threadPool->put_periodic(opt.periodic_interval_ms, std::move(task));
            return true;
        }
        m_threadPool->put(std::move(task));
        return true;
    }
private:
    std::unique_ptr<ThreadPool> m_threadPool;
};


auto makeThreadPoolExecutor(size_t current_threads = 6, size_t max_threads = 20) -> std::unique_ptr<IExecutor> {
    return std::make_unique<ThreadPoolExecutor>(current_threads, max_threads);
}


}; // namespace stp
}; // namespace rtl