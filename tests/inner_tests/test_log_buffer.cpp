#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

#include "logger/buffer.h"
#include "logger/buffer_config.h"

constexpr int RUNS = 7;
std::uint64_t g_target = 0;
// 定义一个BENCHMARK_SINK变量，收集测量数据，让它产生一个编译器不可忽略的外部副作用，避免编译器把我们的逻辑优化掉
std::atomic<std::uint64_t> BENCHMARK_SINK{0};
std::uint64_t Observe(std::string_view value) noexcept {
    return static_cast<std::uint64_t>(value.size()) +
           static_cast<unsigned char>(value.front()) +
           static_cast<unsigned char>(value[value.size() / 2]) +
           static_cast<unsigned char>(value.back());
}

std::uint64_t WriteWithSmallStreamBuffer(std::string_view message) {
    pans::detail::InlineBuffer<pans::detail::LOG_MESSAGE_INLINE_CAPACITY> buffer;
    pans::detail::SmallStreamBuffer<pans::detail::LOG_MESSAGE_INLINE_CAPACITY> stream_buffer(buffer);
    std::ostream stream(&stream_buffer);
    stream << message;
    return Observe(buffer.view());
}

std::uint64_t WriteWithStringStream(std::string_view message) {
    std::stringstream stream;
    stream << message;
    return Observe(stream.view());
}

template <typename Operation>
std::chrono::nanoseconds BenchmarkWrites(Operation&& operation) {
    std::uint64_t checksum = 0;
    const auto begin = std::chrono::steady_clock::now();
    for(std::uint64_t iteration = 0; iteration < g_target; ++iteration) {
        checksum += operation();
    }
    const auto end = std::chrono::steady_clock::now();
    BENCHMARK_SINK.fetch_xor(checksum, std::memory_order_relaxed);
    return std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin);
}

template <typename Benchmark>
double Run(Benchmark&& benchmark) {
    long double total_nanoseconds = 0.0L;
    for(int run = 0; run < RUNS; ++run) {
        total_nanoseconds += static_cast<long double>(benchmark().count());
    }
    return static_cast<double>(total_nanoseconds / RUNS);
}

void PrintResult(std::string_view name, double total_nanoseconds) {
    std::cout << std::left << std::setw(30) << name << std::right
              << total_nanoseconds << " ns \ttotal, "
              << total_nanoseconds / static_cast<double>(g_target) << " ns/op\n";
}

int main(int argc, char** argv) {
    if(argc != 2) {
        std::cerr << "Usage: ./test_log_buffer count\n";
        return 1;
    }
    g_target = std::stoull(argv[1]);

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "operations: " << g_target << ", average of " << RUNS << " runs\n\n";
    constexpr std::array<std::size_t, 6> MESSAGE_SIZES = {50, 100, 200, 500, 1000, 2048};
    for(const std::size_t message_size : MESSAGE_SIZES) {
        const std::string message(message_size, 'x');
        const double small_stream_buffer_nanoseconds = Run([&]() {
            return BenchmarkWrites([&]() { return WriteWithSmallStreamBuffer(message); });
        });
        const double stringstream_nanoseconds = Run([&]() {
            return BenchmarkWrites([&]() { return WriteWithStringStream(message); });
        });

        std::cout << "message: " << message_size << " characters"  << '\n';
        PrintResult("SmallStreamBuffer", small_stream_buffer_nanoseconds);
        PrintResult("std::stringstream", stringstream_nanoseconds);
        std::cout << "speedup: " << stringstream_nanoseconds / small_stream_buffer_nanoseconds << "x\n\n";
    }

    return 0;
}

/** 
 *
实测数据如下(Mac mini M4 OrbStack虚拟机 arm64 Ubuntu)：

yinghuo@ubuntu:~/code/proj/pans/bin/tests$ ./test_log_buffer 1000000
operations: 1000000, average of 7 runs

message: 50 characters
SmallStreamBuffer             41437957.14 ns    total, 41.44 ns/op
std::stringstream             82749579.14 ns    total, 82.75 ns/op
speedup: 2.00x

message: 100 characters
SmallStreamBuffer             41170302.43 ns    total, 41.17 ns/op
std::stringstream             83149944.86 ns    total, 83.15 ns/op
speedup: 2.02x

message: 200 characters
SmallStreamBuffer             64814496.00 ns    total, 64.81 ns/op
std::stringstream             87148907.57 ns    total, 87.15 ns/op
speedup: 1.34x

message: 500 characters
SmallStreamBuffer             66448981.43 ns    total, 66.45 ns/op
std::stringstream             85671529.00 ns    total, 85.67 ns/op
speedup: 1.29x

message: 1000 characters
SmallStreamBuffer             70623368.14 ns    total, 70.62 ns/op
std::stringstream             112632519.57 ns   total, 112.63 ns/op
speedup: 1.59x

message: 2048 characters
SmallStreamBuffer             76701436.43 ns    total, 76.70 ns/op
std::stringstream             184516081.57 ns   total, 184.52 ns/op
speedup: 2.41x

*
*/