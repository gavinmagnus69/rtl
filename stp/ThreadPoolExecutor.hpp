#pragma once

#include "IExecutor.hpp"
#include "RtlThreadPool.hpp"


namespace rtl {
namespace stp {


struct ThreadPoolExecutor final : public IExecutor {
public:
    virtual ~ThreadPoolExecutor() override {
    }
    ThreadPoolExecutor(size_t current_threads = 6, size_t max_threads = 20) {
    }
private:
    std::unique_ptr<ThreadPool> m_threadPool;
};


}; // namespace stp
}; // namespace rtl