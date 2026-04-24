#pragma once

#include <algorithm>
#include <atomic>
#include <list>
#include <optional>
#include <queue>
#include <vector>

#include <condition_variable>
#include <deque>
#include <mutex>
#include <shared_mutex>
#include <utility> // for std::forward

namespace rtl {
namespace stp {

template <class T, class Compare = std::less<T>,
          class Container = std::vector<T>>
class MovablePriorityQueue {
public:
  using value_type = T;

  MovablePriorityQueue() = default;
  explicit MovablePriorityQueue(Compare compare)
      : m_compare(std::move(compare)) {}

  template <typename U> void emplace(U &&value) {
    m_heap.emplace_back(std::forward<U>(value));
    std::push_heap(m_heap.begin(), m_heap.end(), m_compare);
  }

  value_type pop_move() {
    std::pop_heap(m_heap.begin(), m_heap.end(), m_compare);
    value_type value = std::move(m_heap.back());
    m_heap.pop_back();
    return value;
  }

  bool empty() const { return m_heap.empty(); }

  size_t size() const { return m_heap.size(); }

  void clear() { m_heap.clear(); }

private:
  Compare m_compare{};
  Container m_heap{};
};

template <class Container> struct ContainerOps;

template <class T, class Allocator>
struct ContainerOps<std::deque<T, Allocator>> {
  using container_type = std::deque<T, Allocator>;
  using value_type = typename container_type::value_type;

  template <typename U> static void push(container_type &buffer, U &&task) {
    buffer.emplace_back(std::forward<U>(task));
  }

  static value_type pop(container_type &buffer) {
    value_type task = std::move(buffer.front());
    buffer.pop_front();
    return task;
  }

  static bool empty(const container_type &buffer) { return buffer.empty(); }

  static size_t size(const container_type &buffer) { return buffer.size(); }

  static void clear(container_type &buffer) { buffer.clear(); }
};

template <class T, class Allocator>
struct ContainerOps<std::list<T, Allocator>> {
  using container_type = std::list<T, Allocator>;
  using value_type = typename container_type::value_type;

  template <typename U> static void push(container_type &buffer, U &&task) {
    buffer.emplace_back(std::forward<U>(task));
  }

  static value_type pop(container_type &buffer) {
    value_type task = std::move(buffer.front());
    buffer.pop_front();
    return task;
  }

  static bool empty(const container_type &buffer) { return buffer.empty(); }

  static size_t size(const container_type &buffer) { return buffer.size(); }

  static void clear(container_type &buffer) { buffer.clear(); }
};

template <class T, class SequenceContainer, class Compare>
struct ContainerOps<std::priority_queue<T, SequenceContainer, Compare>> {
  using container_type = std::priority_queue<T, SequenceContainer, Compare>;
  using value_type = typename container_type::value_type;

  template <typename U> static void push(container_type &buffer, U &&task) {
    buffer.emplace(std::forward<U>(task));
  }

  static value_type pop(container_type &buffer) {
    // priority_queue::top() is const-reference. Copy here for safety.
    value_type task = buffer.top();
    buffer.pop();
    return task;
  }

  static bool empty(const container_type &buffer) { return buffer.empty(); }

  static size_t size(const container_type &buffer) { return buffer.size(); }

  static void clear(container_type &buffer) {
    while (!buffer.empty()) {
      buffer.pop();
    }
  }
};

template <class T, class Compare, class Container>
struct ContainerOps<MovablePriorityQueue<T, Compare, Container>> {
  using container_type = MovablePriorityQueue<T, Compare, Container>;
  using value_type = typename container_type::value_type;

  template <typename U> static void push(container_type &buffer, U &&task) {
    buffer.emplace(std::forward<U>(task));
  }

  static value_type pop(container_type &buffer) { return buffer.pop_move(); }

  static bool empty(const container_type &buffer) { return buffer.empty(); }

  static size_t size(const container_type &buffer) { return buffer.size(); }

  static void clear(container_type &buffer) { buffer.clear(); }
};

enum class TakeResultErrorCode : uint8_t { item, timeout, closed };
template <class T> struct TakeResult {
  std::optional<T> result{std::nullopt};
  TakeResultErrorCode erc{TakeResultErrorCode::item};
};

template <class T, class Container = std::deque<T>> class UnbMpMcTemplateQueue {
public:
  using Ops = ContainerOps<Container>;

  // Thread-safe puts and pushes.
  template <typename U> bool put(U &&task) {
    std::lock_guard lock(m_mutex);
    if (m_state.load() != ContainerState::open) {
      return false;
    }
    if (Ops::size(m_buffer) + 1 > m_maxQueueSize && m_maxQueueSize != 0) {
      return false;
    }
    Ops::push(m_buffer, std::forward<U>(task));
    m_notEmptyQueue.notify_one();
    return true;
  }

  // Thread-safe takes and pops. Blocks if empty.
  // use tryTake instead to avoid blocking
  [[deprecated("Use tryTake instead")]]
  T take() {
    std::unique_lock lock(m_mutex);
    while (Ops::empty(m_buffer)) {
      m_notEmptyQueue.wait(lock); // unlocks
    }
    return std::move(takeLocked());
  }

  // trying to take with timeout, returns nullopt if timed out
  // memory issues here? extra copying/moving happens when returning optional
  TakeResult<T> tryTake(size_t time_out_ms = 100) {
    std::chrono::milliseconds duration(time_out_ms);
    if (time_out_ms == 0) {
      duration = std::chrono::milliseconds::max(); // wait indefinitely
    }
    std::unique_lock lock(m_mutex);
    if (!Ops::empty(m_buffer)) {
      return TakeResult<T>{.result = std::move(takeLocked())};
    }
    while (Ops::empty(m_buffer)) {
      if (!m_notEmptyQueue.wait_for(lock, duration, [this] {
            return m_state.load() == ContainerState::closing ||
                   !Ops::empty(m_buffer);
          })) { // wakes when stop requested or not empty
        // this is timeout path
        return TakeResult<T>{
            .erc = TakeResultErrorCode::timeout}; // timed out // timed out
                                                  // and still empty
      }
      // no-timeout path
      if (m_state.load() == ContainerState::closing && Ops::empty(m_buffer)) {
        return TakeResult<T>{.erc = TakeResultErrorCode::closed};
      }
    }
    // if (m_state.load() == ContainerState::closing && Ops::empty(m_buffer)) {
    //   return TakeResult<T>{.erc = TakeResultErrorCode::closed};
    // }
    // have item and no timeout
    return TakeResult<T>{.result = std::move(takeLocked())};
  }

  bool empty() const {
    std::lock_guard lock(m_mutex);
    return Ops::empty(m_buffer);
  }

  size_t size() const {
    std::lock_guard lock(m_mutex);
    return Ops::size(m_buffer);
  }

  void clear() {
    std::lock_guard lock(m_mutex);
    Ops::clear(m_buffer);
  }

  void close() {
    m_state.store(ContainerState::closing);
    // m_stopRequested.store(true, std::memory_order_relaxed);
    m_notEmptyQueue.notify_all();
  }

  void set_queue_max_size(size_t max_queue_size) {
    std::lock_guard lock(m_mutex);
    m_maxQueueSize = max_queue_size;
  };

private:
  enum class ContainerState : uint8_t { open, closing };

  T takeLocked() {
    T task = Ops::pop(m_buffer);
    // lock.unlock();
    return std::move(task);
  }
  std::atomic<ContainerState> m_state{ContainerState::open};
  mutable std::mutex m_mutex;
  std::condition_variable m_notEmptyQueue;
  size_t m_maxQueueSize{0}; // 0 == unbounded
  Container m_buffer;
};

} // namespace stp
} // namespace rtl
