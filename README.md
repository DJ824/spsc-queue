Single producer single consumer lock free queue 

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



with these benchmarks, we see that later iterations are faster, and more realistic of real world performance, as the CPU has learned when to take/skip branches and the queue searches L1/L2 cache instead of main memory


