#include <iostream>


#include "StaticThreadPool.h"


using namespace rtl;


stp::StaticThreadPool::StaticThreadPool(uint16_t threadCount)
    : m_threadCount(threadCount) {
    initAll(threadCount);
}


stp::StaticThreadPool::~StaticThreadPool() {
    if (!m_isJoined) {
        joinAll();
    }
}


void stp::StaticThreadPool::putTask(Task&& task) {
    m_taskQueue.put(std::move(task));
}


void stp::StaticThreadPool::runWorker() {
    while (m_releaseAllWorkers == false) {
        if (m_releaseAllWorkers && m_taskQueue.empty()) {
            break;
        }
        Task task = m_taskQueue.take(); // can be blocked here
        // checking empty task
        if (!task) {
            break;
        }
        task();
    }
}


void stp::StaticThreadPool::initAll(uint16_t threadCount) {
    m_releaseAllWorkers.store(false);
    m_isJoined.store(false);
    for (int i = 0; i < threadCount; ++i) {
        m_workers.emplace_back([this]() { this->runWorker(); });
    }
}


void stp::StaticThreadPool::joinAll() {
    m_releaseAllWorkers.store(true);
    // throwing empty task
    for (int i = 0; i < m_threadCount; ++i) {
        Task empty{};
        m_taskQueue.put(empty);
    }
    // for(auto& worker : m_workers) {
    // }
    for (auto& worker : m_workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    m_workers.clear();
    m_isJoined.store(true);
}
