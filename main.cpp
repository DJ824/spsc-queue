#include <atomic>
#include <iostream>
#include <memory>
#include <optional>
#include <thread>
#include <vector>

#ifndef CACHE_LINE_SIZE
#define CACHE_LINE_SIZE 64
#endif

template <typename T, size_t SIZE> class LockFreeQueue {
private:
  static constexpr size_t CAPACITY = SIZE;
  static constexpr size_t PADDING = (CACHE_LINE_SIZE - 1) / sizeof(T) + 1;
  static constexpr bool IS_POWER_OF_TWO = (CAPACITY & (CAPACITY - 1)) == 0;
  static constexpr size_t INDEX_MASK = IS_POWER_OF_TWO ? (CAPACITY - 1) : 0;

  alignas(CACHE_LINE_SIZE) std::atomic<size_t> head_{0};
  alignas(CACHE_LINE_SIZE) std::atomic<size_t> tail_{0};
  alignas(CACHE_LINE_SIZE) size_t head_cache_{0};
  alignas(CACHE_LINE_SIZE) size_t tail_cache_{0};

  alignas(CACHE_LINE_SIZE)
      std::unique_ptr<std::aligned_storage_t<sizeof(T), alignof(T)>[]> buffer_;

  T *get_slot(size_t idx) {
    size_t buffer_idx;
    if constexpr (IS_POWER_OF_TWO) {
      buffer_idx = (idx & INDEX_MASK) + PADDING;
    } else {
      buffer_idx = (idx % CAPACITY) + PADDING;
    }
    return reinterpret_cast<T *>(&buffer_[buffer_idx]);
  }

public:
  LockFreeQueue(size_t capacity)
      : buffer_(
            std::make_unique<std::aligned_storage_t<sizeof(T), alignof(T)>[]>(
                CAPACITY + 2 * PADDING)) {}

  ~LockFreeQueue() {
    size_t curr_head = head_.load(std::memory_order_acquire);
    size_t curr_tail = tail_.load(std::memory_order_acquire);
    while (curr_head != curr_tail) {
      get_slot(curr_head)->~T();
      curr_head = (curr_head + 1) % CAPACITY;
    }
  }

  LockFreeQueue(const LockFreeQueue &) = delete;
  LockFreeQueue &operator=(const LockFreeQueue &) = delete;
  LockFreeQueue(LockFreeQueue &&) = delete;
  LockFreeQueue &operator=(LockFreeQueue &&) = delete;

  bool push(int val) {
    const size_t curr_tail = tail_.load(std::memory_order_relaxed);
    const size_t next_tail = (curr_tail + 1) % CAPACITY;
    if (next_tail == head_cache_) {
      head_cache_ = head_.load(std::memory_order_acquire);
      if (next_tail == head_cache_) {
        return false;
      }
    }
    new (get_slot(curr_tail)) T(val);
    tail_.store(next_tail, std::memory_order_release);
    return true;
  }

  bool pop(int &val) {
    const size_t curr_head = head_.load(std::memory_order_relaxed);
    if (curr_head == tail_cache_) {
      tail_cache_ = tail_.load(std::memory_order_acquire);
      if (curr_head == tail_cache_) {
        return false;
      }
    }
    T *item_ptr = get_slot(curr_head);
    val = *item_ptr;
    item_ptr->~T();
    head_.store((curr_head + 1) % CAPACITY, std::memory_order_release);
    return true;
  }

  const std::atomic<size_t> &readIdx_ = head_;
  const std::atomic<size_t> &writeIdx_ = tail_;
};

void pin_thread(int cpu) {
  if (cpu < 0) {
    return;
  }
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(cpu, &cpuset);
  if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) ==
      -1) {
    perror("pthread_setaffinity_no");
    exit(1);
  }
}

template <typename T> void bench(int cpu1, int cpu2) {
  const size_t queueSize = 100000;
  const int64_t iters = 100000000;

  T q(queueSize);
  auto t = std::thread([&] {
    pin_thread(cpu1);
    for (int i = 0; i < iters; ++i) {
      int val;
      while (!q.pop(val))
        ;
      if (val != i) {
        throw std::runtime_error("");
      }
    }
  });

  pin_thread(cpu2);

  auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < iters; ++i) {
    while (!q.push(i))
      ;
  }
  while (q.readIdx_.load(std::memory_order_relaxed) !=
         q.writeIdx_.load(std::memory_order_relaxed))
    ;
  auto stop = std::chrono::steady_clock::now();
  t.join();
  std::cout << iters * 1000000000 /
                   std::chrono::duration_cast<std::chrono::nanoseconds>(stop -
                                                                        start)
                       .count()
            << " ops/s" << std::endl;
}

int main(int argc, char *argv[]) {

  int cpu1 = -1;
  int cpu2 = -1;

  if (argc == 3) {
    cpu1 = std::stoi(argv[1]);
    cpu2 = std::stoi(argv[2]);
  }

  bench<LockFreeQueue<int, 100001>>(cpu1, cpu2);

  return 0;
}
