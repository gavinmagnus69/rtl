#ifndef threadpool_src_unboundedmpmcqueue_h
#define threadpool_src_unboundedmpmcqueue_h


#include <condition_variable>
#include <deque>
#include <mutex>
#include <shared_mutex>
namespace rtl {
namespace stp {

// Thread-safe unounded Multi-producer Multi-consumer queue.
template <typename T>
class UnboundedMPMCQueue {
public:
    // Thread-safe puts and pushes.
    void put(T task) {
        std::lock_guard lock(m_mutex);
        m_buffer.emplace_back(std::move(task));
        m_notEmptyQueue.notify_one();
    }
    // Thread-safe takes and pops. Blocks if empty.
    T take() {
        std::unique_lock lock(m_mutex);
        while (m_buffer.empty()) {
            m_notEmptyQueue.wait(lock); // unlocks
        }
        return std::move(takeLocked());
    }
    bool empty() const {
        std::lock_guard lock(m_mutex);
        return m_buffer.empty();
    }
    size_t size() const {
        std::lock_guard lock(m_mutex);
        return m_buffer.size();
    }
    void clear() {
        std::lock_guard lock(m_mutex);
        m_buffer.clear();
    }
private:
    T takeLocked() {
        T task = std::move(m_buffer.front());
        m_buffer.pop_front();
        // lock.unlock();
        return std::move(task);
    }
    mutable std::mutex m_mutex;
    std::condition_variable m_notEmptyQueue;
    std::deque<T> m_buffer;
};
} // namespace stp


}; // namespace rtl
#endif
