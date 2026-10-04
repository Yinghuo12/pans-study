#ifndef PANS_INCLUDE_PANS_MUTEX_H
#define PANS_INCLUDE_PANS_MUTEX_H

#include <atomic>
#include <mutex>

#if defined(__x86_64__) || defined(__i386__)
#include <immintrin.h>
#endif

#if defined(__linux__) && defined(__GLIBC__)
#include <pthread.h>
#endif

inline void cpu_relax() noexcept {
#if defined(__x86_64__) || defined(__i386__)
    _mm_pause();
#elif defined(__aarch64__) || defined(__arm__)
    #if defined(_MSC_VER)
        __yield();
    #else
        // 兼容 aarch64 和 arm32 (v6k以上)，GCC 与 Clang 通用
        asm volatile("yield" ::: "memory");
    #endif
#elif defined(__riscv)
    // RISC-V pause 扩展/回退
    asm volatile(".insn i 0x0F, 0, x0, x0, 0x010" ::: "memory");
#else
    // 其它架构兜底：纯编译器屏障，防止死循环被过度优化
    asm volatile("" ::: "memory");
#endif
}

namespace pans {

#if defined(__linux__) && defined(__GLIBC__)

class Spinlock {
public:
    Spinlock() noexcept {
        pthread_spin_init(&m_mutex, PTHREAD_PROCESS_PRIVATE);
    }

    ~Spinlock() noexcept {
        pthread_spin_destroy(&m_mutex);
    }

    Spinlock(const Spinlock&) = delete;
    Spinlock& operator=(const Spinlock&) = delete;

    void lock() noexcept {
        pthread_spin_lock(&m_mutex);
    }

    void unlock() noexcept {
        pthread_spin_unlock(&m_mutex);
    }

private:
    pthread_spinlock_t m_mutex{};
};

#else

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

#endif

} // namespace pans

#endif // PANS_INCLUDE_PANS_MUTEX_H