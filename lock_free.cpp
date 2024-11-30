#ifndef LOCK_FREE_QUEUE_H
#define LOCK_FREE_QUEUE_H

#include <atomic>
#include <array>
#include <optional>
#include <type_traits>

template <typename T, size_t SIZE>
class LockFreeQueue {
private:
    struct Node {
        std::atomic<bool> written{false};
        alignas(T) unsigned char storage[sizeof(T)];

        Node() noexcept : written(false) {}
        ~Node() {
            if (written.load()) {
                reinterpret_cast<T*>(storage)->~T();
            }
        }

        Node(const Node&) = delete;
        Node& operator=(const Node&) = delete;
    };

    std::array<Node, SIZE> buffer_;
    std::atomic<size_t> head_{0};
    std::atomic<size_t> tail_{0};

public:
    LockFreeQueue() = default;

    ~LockFreeQueue() {
        size_t current_head = head_.load();
        size_t current_tail = tail_.load();

        while (current_head != current_tail) {
            Node& node = buffer_[current_head];
            if (node.written.load()) {
                // call destructor of original element
                reinterpret_cast<T*>(node.storage)->~T();
                node.written.store(false);
            }
            current_head = (current_head + 1) % SIZE;
        }

        head_.store(0);
        tail_.store(0);
    }

    LockFreeQueue(LockFreeQueue&& other) noexcept {
        size_t head = other.head_.load(std::memory_order_acquire);
        size_t tail = other.tail_.load(std::memory_order_acquire);

        buffer_ = std::move(other.buffer_);

        head_.store(head, std::memory_order_release);
        tail_.store(tail, std::memory_order_release);

        other.head_.store(0, std::memory_order_release);
        other.tail_.store(0, std::memory_order_release);
    }


    LockFreeQueue(const LockFreeQueue&) = delete;
    LockFreeQueue& operator=(const LockFreeQueue&) = delete;

    template<typename U>
    __attribute__((always_inline)) bool enqueue(U&& item) {

        size_t curr_tail = tail_.load(std::memory_order_acquire);
        size_t next_tail = (curr_tail + 1) % SIZE;

        // queue is full
        if (next_tail == head_.load(std::memory_order_acquire)) {
            return false;
        }

        // use placement new to construct a new T object at the location of curr_tail.storage
        // std::forward preserves the value of the arg (lval/rval)
        new (buffer_[curr_tail].storage) T(std::forward<U>(item));
        buffer_[curr_tail].written.store(true, std::memory_order_release);
        tail_.store(next_tail, std::memory_order_release);

        return true;
    }

    __attribute__((always_inline)) std::optional<T> dequeue() {
        size_t curr_head = head_.load(std::memory_order_acquire);

        // queue is full
        if (curr_head == tail_.load(std::memory_order_acquire)) {
            return std::nullopt;
        }

        // data not written yet
        if (!buffer_[curr_head].written.load(std::memory_order_release)) {
            return std::nullopt;
        }

        // use std move to produce an rval ref, which is used for move/copy constructor of T
        std::optional<T> result(std::move(*reinterpret_cast<T*>(buffer_[curr_head].storage)));
        // call destructor of T to completely delete the object
        reinterpret_cast<T*>(buffer_[curr_head].storage)->~T();
        buffer_[curr_head].written.store(false, std::memory_order_release);
        head_.store((curr_head + 1) % SIZE, std::memory_order_release);

        return result;
    }

    __attribute__((always_inline))  bool empty() const {
        return head_.load() == tail_.load();
    }

    __attribute__((always_inline)) size_t size() const {
        size_t head = head_.load();
        size_t tail = tail_.load();
        if (tail >= head) {
            return tail - head;
        } else {
            return SIZE - (head - tail);
        }
    }

    __attribute__((always_inline))  size_t capacity() const {
        return SIZE - 1;
    }
};

#endif


/*
 * "/Users/devangjaiswal/CLionProjects/benchmarks /lock_free/cmake-build-debug/queue_benchmark"
Single Producer Single Consumer Test
===================================

Iteration 1:
Producer Statistics:
  Throughput:     10047434.36 ops/sec
  Latency:        99.53 ns/op
  Contention:     1.01 attempts/op
Consumer Statistics:
  Throughput:     10047229.94 ops/sec
  Latency:        99.53 ns/op
  Contention:     1.01 attempts/op

Overall Statistics:
  Total items:    10000000
  Total time:     995.33 ms
  Throughput:     10046963.69 ops/sec
  Avg latency:    99.53 ns/op

Iteration 2:
Producer Statistics:
  Throughput:     12435804.68 ops/sec
  Latency:        80.41 ns/op
  Contention:     1.02 attempts/op
Consumer Statistics:
  Throughput:     12435275.05 ops/sec
  Latency:        80.42 ns/op
  Contention:     1.00 attempts/op

Overall Statistics:
  Total items:    10000000
  Total time:     804.19 ms
  Throughput:     12434880.74 ops/sec
  Avg latency:    80.42 ns/op

Iteration 3:
Producer Statistics:
  Throughput:     12800582.35 ops/sec
  Latency:        78.12 ns/op
  Contention:     1.02 attempts/op
Consumer Statistics:
  Throughput:     12800058.03 ops/sec
  Latency:        78.12 ns/op
  Contention:     1.00 attempts/op

Overall Statistics:
  Total items:    10000000
  Total time:     781.27 ms
  Throughput:     12799722.17 ops/sec
  Avg latency:    78.13 ns/op

Iteration 4:
Producer Statistics:
  Throughput:     12511928.70 ops/sec
  Latency:        79.92 ns/op
  Contention:     1.02 attempts/op
Consumer Statistics:
  Throughput:     12511431.66 ops/sec
  Latency:        79.93 ns/op
  Contention:     1.00 attempts/op

Overall Statistics:
  Total items:    10000000
  Total time:     799.29 ms
  Throughput:     12511082.08 ops/sec
  Avg latency:    79.93 ns/op

Iteration 5:
Producer Statistics:
  Throughput:     Consumer Statistics:
  Throughput:     12597434.59 ops/sec
  Latency:        79.38 ns/op
  Contention:     1.00 attempts/op
12597910.03 ops/sec
  Latency:        79.38 ns/op
  Contention:     1.02 attempts/op

Overall Statistics:
  Total items:    10000000
  Total time:     793.87 ms
  Throughput:     12596465.96 ops/sec
  Avg latency:    79.39 ns/op

