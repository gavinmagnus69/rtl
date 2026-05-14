#ifndef threadpool_src_unboundedmpmcqueue_h
#define threadpool_src_unboundedmpmcqueue_h

#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <utility> // for std::forward

namespace rtl {
namespace stp {

// Thread-safe unbounded Multi-producer Multi-consumer queue.
template <typename T>
class [[deprecated("Use template version instead")]] UnboundedMPMCQueue {
public:
    // Thread-safe puts and pushes.
    template <typename U>
    void put(U&& task) {
        std::lock_guard lock(m_mutex);
        m_buffer.emplace_back(std::forward<U>(task));
        m_notEmptyQueue.notify_one();
    }

    // Thread-safe takes and pops. Blocks if empty.
    // use tryTake instead to avoid blocking
    T take() {
        std::unique_lock lock(m_mutex);
        while (m_buffer.empty()) {
            m_notEmptyQueue.wait(lock); // unlocks
        }
        return std::move(takeLocked());
    }

    // trying to take with timeout, returns nullopt if timed out
    // memory issues here? extra copying/moving happens when returning optional
    std::optional<T> tryTake(size_t time_out_ms = 100) {
        std::chrono::milliseconds duration(time_out_ms);
        if (time_out_ms == 0) {
            duration = std::chrono::milliseconds::max(); // wait indefinitely
        }
        std::unique_lock lock(m_mutex);
        if (!m_notEmptyQueue.wait_for(lock, duration, [this] { return m_stopRequested.load() || !m_buffer.empty(); })) { // wakes when stop requested or not empty
            return std::nullopt;                                                                                         // timed out // timed out and still empty
        }
        if (m_stopRequested.load()) {
            return std::nullopt; // stop requested
        }
        if (m_buffer.empty()) {
            return std::nullopt; // spurious wakeup
        }
        // have item and no timeout
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
    void request_stop() {
        m_stopRequested.store(true, std::memory_order_relaxed);
        m_notEmptyQueue.notify_all();
    }
private:
    T takeLocked() {
        T task = std::move(m_buffer.front());
        m_buffer.pop_front();
        // lock.unlock();
        return std::move(task);
    }
    std::atomic<bool> m_stopRequested{false};
    // bool m_stopFlag{false};
    mutable std::mutex m_mutex;
    std::condition_variable m_notEmptyQueue;
    std::deque<T> m_buffer;
};
} // namespace stp

}; // namespace rtl
#endif
