#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <iomanip>
#include "lock_free.cpp"

class Benchmark {
private:
    static constexpr size_t QUEUE_SIZE = 1024;
    static constexpr size_t NUM_OPERATIONS = 10'000'000;

    struct TestData {
        int value;
        char padding[60];

        TestData(int v = 0) : value(v) {}
    };

public:
    void run_spsc() {
        std::cout << "Single Producer Single Consumer Test\n";
        std::cout << "===================================\n";

        for (int iteration = 0; iteration < 5; ++iteration) {
            std::cout << "\nIteration " << iteration + 1 << ":\n";
            run_single_iteration();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

private:
    void run_single_iteration() {
        LockFreeQueue<TestData, QUEUE_SIZE> queue;
        std::atomic<bool> start{false};
        std::atomic<bool> producer_done{false};
        std::atomic<size_t> consumed_count{0};

        auto producer = [&]() {
            while (!start.load(std::memory_order_acquire)) {}

            auto start_time = std::chrono::high_resolution_clock::now();
            size_t successful_enqueues = 0;
            size_t total_attempts = 0;

            for (size_t i = 0; i < NUM_OPERATIONS; ++i) {
                total_attempts++;
                while (!queue.enqueue(TestData(i))) {
                    total_attempts++;
                    std::this_thread::yield();
                }
                successful_enqueues++;
            }

            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);

            double ops_per_sec = (successful_enqueues * 1e9) / duration.count();
            double ns_per_op = static_cast<double>(duration.count()) / successful_enqueues;
            double contention_ratio = static_cast<double>(total_attempts) / successful_enqueues;

            std::cout << "Producer Statistics:\n"
                      << "  Throughput:     " << std::fixed << std::setprecision(2) << ops_per_sec << " ops/sec\n"
                      << "  Latency:        " << std::fixed << std::setprecision(2) << ns_per_op << " ns/op\n"
                      << "  Contention:     " << std::fixed << std::setprecision(2) << contention_ratio << " attempts/op\n";

            producer_done.store(true, std::memory_order_release);
        };

        auto consumer = [&]() {
            while (!start.load(std::memory_order_acquire)) {}

            auto start_time = std::chrono::high_resolution_clock::now();
            size_t successful_dequeues = 0;
            size_t total_attempts = 0;

            while (successful_dequeues < NUM_OPERATIONS) {
                total_attempts++;
                if (auto item = queue.dequeue()) {
                    successful_dequeues++;
                } else if (producer_done.load(std::memory_order_acquire)) {
                    if (successful_dequeues >= NUM_OPERATIONS) break;
                    std::this_thread::yield();
                } else {
                    std::this_thread::yield();
                }
            }

            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);

            double ops_per_sec = (successful_dequeues * 1e9) / duration.count();
            double ns_per_op = static_cast<double>(duration.count()) / successful_dequeues;
            double contention_ratio = static_cast<double>(total_attempts) / successful_dequeues;

            std::cout << "Consumer Statistics:\n"
                      << "  Throughput:     " << std::fixed << std::setprecision(2) << ops_per_sec << " ops/sec\n"
                      << "  Latency:        " << std::fixed << std::setprecision(2) << ns_per_op << " ns/op\n"
                      << "  Contention:     " << std::fixed << std::setprecision(2) << contention_ratio << " attempts/op\n";

            consumed_count.store(successful_dequeues, std::memory_order_release);
        };

        std::thread producer_thread(producer);
        std::thread consumer_thread(consumer);

        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        auto test_start = std::chrono::high_resolution_clock::now();
        start.store(true, std::memory_order_release);

        producer_thread.join();
        consumer_thread.join();

        auto test_end = std::chrono::high_resolution_clock::now();
        auto total_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(test_end - test_start);

        double total_ops_per_sec = (consumed_count.load() * 1e9) / total_duration.count();
        double total_ns_per_op = static_cast<double>(total_duration.count()) / consumed_count.load();

        std::cout << "\nOverall Statistics:\n"
                  << "  Total items:    " << consumed_count.load() << "\n"
                  << "  Total time:     " << std::fixed << std::setprecision(2)
                  << total_duration.count() / 1e6 << " ms\n"
                  << "  Throughput:     " << std::fixed << std::setprecision(2)
                  << total_ops_per_sec << " ops/sec\n"
                  << "  Avg latency:    " << std::fixed << std::setprecision(2)
                  << total_ns_per_op << " ns/op\n";
    }
};

int main() {
    Benchmark benchmark;
    benchmark.run_spsc();
    return 0;
}