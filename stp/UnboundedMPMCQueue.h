#ifndef threadpool_src_unboundedmpmcqueue_h
#define threadpool_src_unboundedmpmcqueue_h


#include <deque>
#include <mutex>
#include <condition_variable>

namespace rtl{
namespace stp {
template<typename T>
class UnboundedMPMCQueue {
public:
    void put(T task) {
        std::lock_guard lock(m_mutex);
        m_buffer.emplace_back(std::move(task));
        m_notEmptyQueue.notify_one();
    }

    //blocking current thread
    T take() {
        std::unique_lock lock(m_mutex);
        while(m_buffer.empty()) {
            m_notEmptyQueue.wait(lock); //unlocks
        }
        return std::move(takeLocked());
    }
    bool empty() {
        std::lock_guard lock(m_mutex);
        return m_buffer.empty();
    }
private:
    T takeLocked() {
        T task = std::move(m_buffer.front());
        m_buffer.pop_front();
        // lock.unlock();
        return std::move(task);
    }
    std::mutex m_mutex;
    std::condition_variable m_notEmptyQueue;
    std::deque<T> m_buffer;
};
}


};
#endif
