#include "lock_free.cpp"
#include <atomic>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <thread>
#include <vector>

void pinThread(int cpu) {
  if (cpu < 0) {
    return;
  }
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(cpu, &cpuset);
  if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) ==
      -1) {
    perror("pthread_setaffinity_np");
    exit(1);
  }
}

int main(int argc, char *argv[]) {
  int producer_cpu = -1;
  int consumer_cpu = -1;

  if (argc >= 3) {
    producer_cpu = std::stoi(argv[1]);
    consumer_cpu = std::stoi(argv[2]);
    std::cout << "Pinning producer to CPU " << producer_cpu
              << " and consumer to CPU " << consumer_cpu << std::endl;
  }

  const size_t QUEUE_SIZE = 1024;
  const size_t NUM_ITERATIONS = 10000000;
  const int NUM_RUNS = 5;

  std::cout << "Queue capacity: " << QUEUE_SIZE - 1 << " elements" << std::endl;
  std::cout << "Operations per test: " << NUM_ITERATIONS << std::endl;
  std::cout << "Number of test runs: " << NUM_RUNS << std::endl << std::endl;

  std::cout << "Single Producer, Single Consumer Throughput Test" << std::endl;
  std::cout << "-----------------------------------------------" << std::endl;

  for (int run = 0; run < NUM_RUNS; ++run) {
    LockFreeQueue<int, QUEUE_SIZE> queue;
    std::atomic<bool> producer_done(false);

    auto consumer = std::thread([&] {
      if (consumer_cpu >= 0)
        pinThread(consumer_cpu);

      size_t count = 0;
      int expected_value = 0;

      for (int i = 0; i < NUM_ITERATIONS; ++i) {
        auto result = queue.front();
        while (!result) {
          result = queue.front();
          if (producer_done.load(std::memory_order_acquire) && !result) {
            break;
          }
        }

        if (result) {
          if (*result != expected_value) {
            std::cerr << "Error: Expected " << expected_value << " but got "
                      << *result << std::endl;
            exit(1);
          }
          expected_value++;
          count++;
          queue.pop();
        }
      }
    });

    if (producer_cpu >= 0)
      pinThread(producer_cpu);

    auto start_time = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < NUM_ITERATIONS; ++i) {
      while (!queue.enqueue(static_cast<int>(i))) {
        std::this_thread::yield();
      }
    }

    producer_done.store(true, std::memory_order_release);
    consumer.join();

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                           end_time - start_time)
                           .count();

    double throughput = (NUM_ITERATIONS * 1000000.0) / duration_ns;
    double latency = static_cast<double>(duration_ns) / NUM_ITERATIONS;

    std::cout << "Run " << (run + 1) << ":" << std::endl;
    std::cout << "  Operations: " << NUM_ITERATIONS << std::endl;
    std::cout << "  Duration: " << std::fixed << std::setprecision(2)
              << (duration_ns / 1000000.0) << " ms" << std::endl;
    std::cout << "  Throughput: " << std::fixed << std::setprecision(2)
              << throughput << " ops/ms" << std::endl;
    std::cout << "  Latency: " << std::fixed << std::setprecision(2) << latency
              << " ns/op" << std::endl;
    std::cout << std::endl;
  }

  std::cout << "Round-Trip Latency Test" << std::endl;
  std::cout << "-----------------------------------------------" << std::endl;

  for (int run = 0; run < NUM_RUNS; ++run) {
    LockFreeQueue<int, QUEUE_SIZE> ping_queue;
    LockFreeQueue<int, QUEUE_SIZE> pong_queue;

    auto worker = std::thread([&] {
      if (consumer_cpu >= 0)
        pinThread(consumer_cpu);

      for (size_t i = 0; i < NUM_ITERATIONS; ++i) {
        auto result = ping_queue.front();
        while (!result) {
          result = ping_queue.front();
        }

        int value = *result;
        ping_queue.pop();

        while (!pong_queue.enqueue(value)) {
          std::this_thread::yield();
        }
      }
    });

    if (producer_cpu >= 0)
      pinThread(producer_cpu);

    auto start_time = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < NUM_ITERATIONS; ++i) {
      while (!ping_queue.enqueue(static_cast<int>(i))) {
        std::this_thread::yield();
      }

      auto response = pong_queue.front();
      while (!response) {
        response = pong_queue.front();
      }

      if (*response != static_cast<int>(i)) {
        std::cerr << "Error: Expected " << i << " but got " << *response
                  << std::endl;
        exit(1);
      }
      
      pong_queue.pop();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                           end_time - start_time)
                           .count();

    worker.join();

    double rtt = static_cast<double>(duration_ns) / NUM_ITERATIONS;

    std::cout << "Run " << (run + 1) << ":" << std::endl;
    std::cout << "  Round trips: " << NUM_ITERATIONS << std::endl;
    std::cout << "  Duration: " << std::fixed << std::setprecision(2)
              << (duration_ns / 1000000.0) << " ms" << std::endl;
    std::cout << "  RTT Latency: " << std::fixed << std::setprecision(2) << rtt
              << " ns" << std::endl;
    std::cout << std::endl;
  }

  return 0;
}
