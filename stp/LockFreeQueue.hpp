#ifndef rtl_stp_lockfreequeue_hpp
#define rtl_stp_lockfreequeue_hpp

#include <atomic>
#include <optional>


// UNDER CONSTRUCTION
// Michael-Scott queue
template <typename T>
class LockFreeQueue {
public:
  struct Node {
    std::optional<T> m_value{};
    std::atomic<Node*> next{nullptr};
    Node(const T& val)
        : m_value(val) {
    }
    Node() = default;
    ~Node() = default;
  };

  LockFreeQueue() {
    Node* dummy = new Node();
    m_head.store(dummy);
    m_tail.store(dummy);
  };

  ~LockFreeQueue() {
    Node* head = m_head.load();
    while (head) {
      auto next = head->next.load();
      delete head;
      head = next;
    }
  }

  void push(const T& value) {
    Node* new_node = new Node(value);
    while (true) {
      Node* cur_tail = m_tail.load(std::memory_order_acquire);
      Node* cur_tail_next = cur_tail->next.load(std::memory_order_acquire);
      if (cur_tail != m_tail.load(std::memory_order_acquire)) {
        continue;
      }
      if (cur_tail_next == nullptr) {
        if (cur_tail->next.compare_exchange_weak(cur_tail_next, new_node, std::memory_order_release, std::memory_order_relaxed)) {
          m_tail.compare_exchange_weak(cur_tail, new_node, std::memory_order_release, std::memory_order_relaxed);
          return;
        }
      } else {
        m_tail.compare_exchange_weak(cur_tail, cur_tail_next, std::memory_order_release, std::memory_order_relaxed);
      }
    }
  }

  std::optional<T> pop() {
    while (true) {
      Node* cur_head = m_head.load(std::memory_order_acquire);
      Node* cur_tail = m_tail.load(std::memory_order_acquire);
      Node* head_next = cur_head->next.load(std::memory_order_acquire);
      // check if head is expected head in current context
      if (cur_head == cur_tail) {
        if (head_next == nullptr) {
          return std::nullopt;
        }
        m_tail.compare_exchange_weak(cur_tail, head_next, std::memory_order_release, std::memory_order_relaxed);
        continue;
      }
      if (m_head.compare_exchange_weak(cur_head, head_next, std::memory_order_acq_rel, std::memory_order_relaxed)) {
        auto data = head_next->m_value;
        delete cur_head;
        return data;
      }
    }
  }
private:
  std::atomic<Node*> m_head{nullptr};
  std::atomic<Node*> m_tail{nullptr};
};


#endif