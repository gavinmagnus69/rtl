#ifndef threadpool_src_staticthreadpool_h
#define threadpool_src_staticthreadpool_h

#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>


#include "UnboundedMPMCQueue.h"

namespace rtl {
namespace stp {

using Task = std::function<void(void)>;

class StaticThreadPool {
public:
    StaticThreadPool(uint16_t);
    ~StaticThreadPool();
public:
    template <typename T>
    std::future<T> addTask(std::function<T()> func) {
        // std::function requires copyable callables, so keep the task behind a shared_ptr
        // to allow the wrapper to be copied as it moves through the queue.
        auto taskPtr = std::make_shared<std::packaged_task<T()>>(std::move(func));
        std::future<T> fut = taskPtr->get_future();
        Task wrappedTask = [taskPtr]() mutable { (*taskPtr)(); };
        m_taskQueue.put(std::move(wrappedTask));
        return fut;
    }
    void putTask(Task&&);
    void joinAll();
private:
    void initAll(uint16_t);
    void runWorker();
    uint16_t m_threadCount;
    std::vector<std::thread> m_workers;
    UnboundedMPMCQueue<Task> m_taskQueue;
    std::atomic<bool> m_releaseAllWorkers;
    std::atomic<bool> m_isJoined;
};
}; // namespace stp
}; // namespace rtl
#endif
