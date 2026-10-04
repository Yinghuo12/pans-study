#include <pans/mutex.h>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <latch>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

constexpr int RUNS = 7;
uint64_t g_Counter = 0;
std::uint64_t g_target = 0;

class Spinlock {
public:
    using Lock = std::lock_guard<Spinlock>;

    Spinlock() noexcept = default;
    ~Spinlock() noexcept = default;
    Spinlock(const Spinlock&) = delete;
    Spinlock& operator=(const Spinlock&) = delete;

    void lock() noexcept {
        while (m_mutex.test_and_set(std::memory_order_acquire)) {
            while (m_mutex.test(std::memory_order_relaxed)) {
                cpu_relax();
            }
        }
    }

    [[nodiscard]] bool try_lock() noexcept {
        return !m_mutex.test_and_set(std::memory_order_acquire);
    }

    void unlock() noexcept {
        m_mutex.clear(std::memory_order_release);
    }

private:
    std::atomic_flag m_mutex = ATOMIC_FLAG_INIT;
};


class AtomicWaitLock {
public:
    using Lock = std::lock_guard<AtomicWaitLock>;

    AtomicWaitLock() noexcept = default;
    ~AtomicWaitLock() noexcept = default;
    AtomicWaitLock(const AtomicWaitLock&) = delete;
    AtomicWaitLock& operator=(const AtomicWaitLock&) = delete;

    void lock() noexcept {
        bool expected = false;
        while (!m_mutex.compare_exchange_weak(expected, true, std::memory_order_acquire, std::memory_order_relaxed)) {
            std::atomic_wait_explicit(&m_mutex, true, std::memory_order_relaxed);
            expected = false;
        }
    }

    [[nodiscard]] bool try_lock() noexcept {
        bool expected = false;
        return m_mutex.compare_exchange_strong(expected, true, std::memory_order_acquire, std::memory_order_relaxed);
    }

    void unlock() noexcept {
        m_mutex.store(false, std::memory_order_release);
        std::atomic_notify_one(&m_mutex);
    }

private:
    std::atomic<bool> m_mutex{false};
};


std::uint64_t OperationsForThread(std::size_t thread_index, std::size_t thread_count) {
    const std::uint64_t base = g_target / thread_count;
    const std::uint64_t remainder = g_target % thread_count;
    return base + (thread_index < remainder ? 1 : 0);
}

template <typename Operation>
std::chrono::nanoseconds BenchmarkThreads(std::size_t thread_count, Operation&& operation) {
    std::latch ready(static_cast<std::ptrdiff_t>(thread_count));
    std::latch start_gate(1);
    std::latch finished(static_cast<std::ptrdiff_t>(thread_count));

    std::vector<std::thread> threads;
    threads.reserve(thread_count);
    for (std::size_t thread_index = 0; thread_index < thread_count; ++thread_index) {
        threads.emplace_back([&, thread_index]() {
            ready.count_down();
            start_gate.wait();
            operation(thread_index, OperationsForThread(thread_index, thread_count));
            finished.count_down();
        });
    }

    ready.wait();
    const auto begin = std::chrono::steady_clock::now();
    start_gate.count_down();
    finished.wait();
    const auto end = std::chrono::steady_clock::now();

    for (std::thread& thread : threads) {
        thread.join();
    }

    return std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin);
}

template <typename Mutex>
std::chrono::nanoseconds BenchmarkMutex(std::size_t thread_count) {
    Mutex mutex;
    g_Counter = 0;
    const auto elapsed = BenchmarkThreads(thread_count, [&mutex](std::size_t, uint64_t operations) {
            for(uint64_t index = 0; index < operations; ++index) {
                std::lock_guard<Mutex> lock(mutex);
                ++g_Counter;
            }
        });

    if (g_Counter != g_target) {
        throw std::runtime_error("mutex correctness check failed");
    }

    return elapsed;
}

template <typename Benchmark>
double Run(Benchmark&& benchmark) {
    long double total_nanoseconds = 0.0L;
    for (int run = 0; run < RUNS; ++run) {
        total_nanoseconds += static_cast<long double>(benchmark().count());
    }
    return static_cast<double>(total_nanoseconds / RUNS);
}

void PrintResult(std::string_view name, double total_nanoseconds) {
    const double nanoseconds_per_operation = total_nanoseconds / static_cast<double>(g_target);
    std::cout << std::left << std::setw(30) << name << std::right
              << total_nanoseconds << " ns \ttotal, "
              << nanoseconds_per_operation << " ns/op\n";
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: ./test_mutex count\n";
        return EXIT_FAILURE;
    }
    g_target = std::stoull(argv[1]);

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "operations: " << g_target << ", average of " << RUNS << " runs\n\n";

    const std::vector<std::size_t> thread_counts = {1, 2, 4, 8, 10};
    for (const std::size_t thread_count : thread_counts) {
        std::cout << "threads: " << thread_count << '\n';
        PrintResult("pthread_spinlock::Spinlock", Run([&]() { return BenchmarkMutex<pans::Spinlock>(thread_count); }));
        PrintResult("atomic_flag::Spinlock", Run([&]() { return BenchmarkMutex<Spinlock>(thread_count); }));
        PrintResult("atomic_wait", Run([&]() { return BenchmarkMutex<AtomicWaitLock>(thread_count); }));
        PrintResult("std::mutex", Run([&]() { return BenchmarkMutex<std::mutex>(thread_count); }));
        std::cout << '\n';
    }

    return EXIT_SUCCESS;
}

/** 
 *
实测数据如下(Mac mini M4 OrbStack虚拟机 arm64 Ubuntu)：

yinghuo@ubuntu:~/code/proj/pans/bin/tests$ ./test_mutex 1000000
operations: 1000000, average of 7 runs

threads: 1
pthread_spinlock::Spinlock    2397893.29 ns     total, 2.40 ns/op
atomic_flag::Spinlock         1216560.00 ns     total, 1.22 ns/op
atomic_wait                   3147855.29 ns     total, 3.15 ns/op
std::mutex                    4585821.29 ns     total, 4.59 ns/op

threads: 2
pthread_spinlock::Spinlock    59068263.00 ns    total, 59.07 ns/op
atomic_flag::Spinlock         49750988.71 ns    total, 49.75 ns/op
atomic_wait                   15456811.86 ns    total, 15.46 ns/op
std::mutex                    24376310.29 ns    total, 24.38 ns/op

threads: 4
pthread_spinlock::Spinlock    112037411.00 ns   total, 112.04 ns/op
atomic_flag::Spinlock         122690674.71 ns   total, 122.69 ns/op
atomic_wait                   169240908.71 ns   total, 169.24 ns/op
std::mutex                    16785860.43 ns    total, 16.79 ns/op

threads: 8
pthread_spinlock::Spinlock    195271382.14 ns   total, 195.27 ns/op
atomic_flag::Spinlock         219041564.00 ns   total, 219.04 ns/op
atomic_wait                   240164803.86 ns   total, 240.16 ns/op
std::mutex                    26282909.57 ns    total, 26.28 ns/op

threads: 10
pthread_spinlock::Spinlock    221717553.86 ns   total, 221.72 ns/op
atomic_flag::Spinlock         219999795.43 ns   total, 220.00 ns/op
atomic_wait                   256410298.57 ns   total, 256.41 ns/op
std::mutex                    30459186.71 ns    total, 30.46 ns/op

*
*/