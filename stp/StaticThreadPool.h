#ifndef threadpool_src_staticthreadpool_h
#define threadpool_src_staticthreadpool_h


#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <functional>


#include "UnboundedMPMCQueue.h"

namespace rtl {
namespace stp {

using Task = std::function<void(void)>;
    
class StaticThreadPool {
public:
    StaticThreadPool(uint16_t);
    ~StaticThreadPool();
public:
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
};
};
#endif