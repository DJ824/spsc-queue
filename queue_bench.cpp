#include <atomic>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <thread>
#include <vector>
#include <array>
#include <cstring>
#include <pthread.h>
#include <sched.h>
#include <cstdlib>
#include <cstdio>
#include <immintrin.h>

#include "../dj/utils/spsc.h"

template <size_t BYTES>
struct Payload {
    static_assert(BYTES >= 4, "Payload must be at least 4 bytes");
    std::array<unsigned char, BYTES> data{};

    void set_data(int v) {
        std::memcpy(data.data(), &v, sizeof(int));
    }

    int seq() const {
        int v;
        std::memcpy(&v, data.data(), sizeof(int));
        return v;
    }
};

void pinThread(int cpu) {
    if (cpu < 0) {
        return;
    }
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);
    const int rc = ::pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    if (rc != 0) {
        std::cerr << "pthread_setaffinity_np: " << std::strerror(rc) << " (rc=" << rc << ")\n";
        std::exit(1);
    }
}

int main(int argc, char* argv[]) {
    int producer_cpu = -1;
    int consumer_cpu = -1;

    if (argc >= 3) {
        producer_cpu = std::stoi(argv[1]);
        consumer_cpu = std::stoi(argv[2]);
        std::cout << "Pinning producer to CPU " << producer_cpu
            << " and consumer to CPU " << consumer_cpu << std::endl;
    }

    constexpr size_t QUEUE_SIZE = 8192;
    constexpr size_t NUM_ITERATIONS = 10000000; // 10M
    constexpr int NUM_RUNS = 5;

    std::cout << "Queue capacity: " << (QUEUE_SIZE - 1) << " elements" << std::endl;
    std::cout << "Operations per test: " << NUM_ITERATIONS << std::endl;
    std::cout << "Number of test runs: " << NUM_RUNS << std::endl << std::endl;

    std::cout << "Single Producer, Single Consumer Throughput Test" << std::endl;
    std::cout << "-----------------------------------------------" << std::endl;

    const std::vector<size_t> sizes = {4, 8, 16, 32, 64, 128, 256};

    for (size_t bytes : sizes) {
        std::cout << "Payload: " << bytes << " bytes" << std::endl;
        // throughput test
        for (int run = 0; run < NUM_RUNS; ++run) {
            auto run_throughput = [&](auto tag) {
                using P = decltype(tag);
                LockFreeQueue<P, QUEUE_SIZE> queue;
                std::atomic<bool> producer_done(false);

                // consumer loop
                auto consumer = std::thread([&] {
                    if (consumer_cpu >= 0)
                        pinThread(consumer_cpu);

                    int expected_value = 0;
                    for (size_t i = 0; i < NUM_ITERATIONS; ++i) {
                        P* result = queue.front();
                        while (!result) {
                            _mm_pause();
                            result = queue.front();
                            if (producer_done.load(std::memory_order_acquire) && !result) {
                                break;
                            }
                        }
                        if (result) {
                            if (result->seq() != expected_value) {
                                std::cerr << "error, expected " << expected_value << " but got "
                                    << result->seq() << std::endl;
                                exit(1);
                            }
                            expected_value++;
                            queue.pop();
                        }
                    }
                });

                if (producer_cpu >= 0) {
                    pinThread(producer_cpu);
                }


                // producer loop
                auto start_time = std::chrono::high_resolution_clock::now();
                for (size_t i = 0; i < NUM_ITERATIONS; ++i) {
                    P msg;
                    msg.set_data(static_cast<int>(i));
                    while (!queue.enqueue(msg)) {
                        _mm_pause();
                    }
                }

                producer_done.store(true, std::memory_order_release);
                consumer.join();

                auto end_time = std::chrono::high_resolution_clock::now();
                auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                        end_time - start_time)
                    .count();

                double throughput_ops_per_ms = (NUM_ITERATIONS * 1000000.0) / duration_ns;
                double latency_ns_per_op = static_cast<double>(duration_ns) / NUM_ITERATIONS;

                std::cout << "  Run " << (run + 1) << ": "
                    << std::fixed << std::setprecision(2)
                    << throughput_ops_per_ms << " ops/ms, "
                    << latency_ns_per_op << " ns/op" << std::endl;
            };

            switch (bytes) {
            case 4: run_throughput(Payload<4>{});
                break;
            case 8: run_throughput(Payload<8>{});
                break;
            case 16: run_throughput(Payload<16>{});
                break;
            case 32: run_throughput(Payload<32>{});
                break;
            case 64: run_throughput(Payload<64>{});
                break;
            case 128: run_throughput(Payload<128>{});
                break;
            case 256: run_throughput(Payload<256>{});
                break;
            default: break;
            }
        }
        std::cout << std::endl;
    }

    // rtt test
    // producer thread enqueues a msg onto ping q
    // worker thread pops the msg, and enqueues it onto pong q
    // producer thread then reads the msg from the pong q
    std::cout << "Round-Trip Latency Test" << std::endl;
    std::cout << "-----------------------------------------------" << std::endl;

    for (size_t bytes : sizes) {
        std::cout << "Payload: " << bytes << " bytes" << std::endl;
        for (int run = 0; run < NUM_RUNS; ++run) {
            auto run_rtt = [&](auto tag) {
                using P = decltype(tag);
                LockFreeQueue<P, QUEUE_SIZE> ping_queue;
                LockFreeQueue<P, QUEUE_SIZE> pong_queue;

                auto worker = std::thread([&] {
                    if (consumer_cpu >= 0)
                        pinThread(consumer_cpu);

                    for (size_t i = 0; i < NUM_ITERATIONS; ++i) {
                        P* req = ping_queue.front();
                        while (!req) {
                            req = ping_queue.front();
                        }
                        P value = *req;
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
                    P msg;
                    msg.set_data(static_cast<int>(i));
                    while (!ping_queue.enqueue(msg)) {
                        std::this_thread::yield();
                    }

                    P* resp = pong_queue.front();
                    while (!resp) {
                        resp = pong_queue.front();
                    }
                    if (resp->seq() != static_cast<int>(i)) {
                        std::cerr << "Error: Expected " << i << " but got " << resp->seq() << std::endl;
                        exit(1);
                    }
                    pong_queue.pop();
                }

                auto end_time = std::chrono::high_resolution_clock::now();
                auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                        end_time - start_time)
                    .count();

                worker.join();

                double rtt_ns = static_cast<double>(duration_ns) / NUM_ITERATIONS;

                std::cout << "  Run " << (run + 1) << ": "
                    << std::fixed << std::setprecision(2)
                    << rtt_ns << " ns (RTT)" << std::endl;
            };

            switch (bytes) {
            case 4: run_rtt(Payload<4>{});
                break;
            case 8: run_rtt(Payload<8>{});
                break;
            case 16: run_rtt(Payload<16>{});
                break;
            case 32: run_rtt(Payload<32>{});
                break;
            case 64: run_rtt(Payload<64>{});
                break;
            case 128: run_rtt(Payload<128>{});
                break;
            case 256: run_rtt(Payload<256>{});
                break;
            default: break;
            }
        }
        std::cout << std::endl;
    }
    return 0;
}
