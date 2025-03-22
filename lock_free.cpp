#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <optional>
#include <utility>

#ifndef CACHE_LINE_SIZE
#define CACHE_LINE_SIZE 64
#endif

template <typename T, size_t SIZE> class LockFreeQueue {
private:
  struct Node {
    std::atomic<bool> written_{false};
    alignas(T) unsigned char storage_[sizeof(T)];

    Node() noexcept = default;
    ~Node() {
      if (written_.load(std::memory_order_relaxed)) {
        reinterpret_cast<T *>(storage_)->~T();
      }
    }

    Node(const Node &) = delete;
    Node &operator=(const Node &) = delete;
  };

  static constexpr size_t CAPACITY = SIZE;

  alignas(CACHE_LINE_SIZE) std::atomic<size_t> head_{0};
  alignas(CACHE_LINE_SIZE) std::atomic<size_t> tail_{0};
  alignas(CACHE_LINE_SIZE) size_t head_cache_{0};
  alignas(CACHE_LINE_SIZE) size_t tail_cache_{0};

  // ensure padding is the size of however many nodes fit in 1 cache line
  static constexpr size_t PADDING = (CACHE_LINE_SIZE - 1) / sizeof(Node) + 1;
  // pad the beginning and end of the array so that we are sharing cache line
  // with adjacent allocatiions at the location the queue is allocated
  alignas(CACHE_LINE_SIZE) std::array<Node, CAPACITY + 2 * PADDING> buffer_{};

  Node &get_node(size_t idx) { return buffer_[idx + PADDING]; }

public:
  LockFreeQueue() = default;
  ~LockFreeQueue() {
    size_t curr_head = head_.load(std::memory_order_acquire);
    size_t curr_tail = tail_.load(std::memory_order_acquire);

    while (curr_head != curr_tail) {
      Node &node = get_node(curr_head);
      if (node.written_.load(std::memory_order_acquire)) {
        reinterpret_cast<T *>(node.storage_)->~T();
        node.written_.store(false, std::memory_order_release);
      }
      curr_head = (curr_head + 1) % CAPACITY;
    }
  }

  LockFreeQueue(const LockFreeQueue &) = delete;
  LockFreeQueue &operator=(const LockFreeQueue &) = delete;
  LockFreeQueue(LockFreeQueue &&) = delete;
  LockFreeQueue &operator=(LockFreeQueue &&) = delete;

  template <typename U> bool enqueue(U &&item) {
    const size_t curr_tail = tail_.load(std::memory_order_acquire);
    const size_t next_tail = (curr_tail + 1) % CAPACITY;

    if (next_tail == head_cache_) {
      head_cache_ = head_.load(std::memory_order_acquire);
      if (next_tail == head_cache_) {
        return false;
      }
    }
    new (get_node(curr_tail).storage_) T(std::forward<U>(item));
    get_node(curr_tail).written_.store(true, std::memory_order_release);
    tail_.store(next_tail, std::memory_order_release); 
    return true;
  }

  T* front() {
    const size_t curr_head = head_.load(std::memory_order_acquire);
    if (curr_head == tail_cache_) {
      tail_cache_ = tail_.load(std::memory_order_acquire);
      if (curr_head == tail_cache_) {
        return nullptr; 
      }
    }

    Node &node = get_node(curr_head);
    if (!node.written_.load(std::memory_order_acquire)) {
      return nullptr; 
    }
    return reinterpret_cast<T*>(node.storage_); 
  }

  void pop() {
    const size_t curr_head = head_.load(std::memory_order_acquire);
    Node &node = get_node(curr_head);
    reinterpret_cast<T*>(node.storage_)->~T();
    node.written_.store(false, std::memory_order_release);
    head_.store((curr_head + 1) % CAPACITY, std::memory_order_release);
    
  }

  std::optional<T> dequeue() {
    const size_t curr_head = head_.load(std::memory_order_acquire);
    const size_t next_head = (curr_head + 1) % CAPACITY;

    if (curr_head == tail_cache_) {
      tail_cache_ = tail_.load(std::memory_order_acquire);
      if (curr_head == tail_cache_) {
        return std::nullopt;
      }
    }

    Node &node = get_node(curr_head);
    if (!node.written_.load(std::memory_order_acquire)) {
      return false;
    }

    T *item_ptr = reinterpret_cast<T *>(node.storage_);
    std::optional<T> result(std::move(*item_ptr));

    item_ptr->~T();
    node.written_.store(false, std::memory_order_release);
    head_.store((curr_head + 1) % CAPACITY, std::memory_order_release);
    return result;
  }

  bool empty() const { return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire); }

  size_t size() const {
    const size_t head = head_.load(std::memory_order_acquire);
    const size_t tail = tail_.load(std::memory_order_acquire);

    if (tail >= head) {
      return tail - head;
    } else {
      return CAPACITY - (head - tail); 
    }
  }

  size_t capacity() const {
    return CAPACITY - 1; 
  }
}; 

