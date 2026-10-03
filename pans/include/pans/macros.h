#ifndef PANS_INCLUDE_PANS_MACROS_H_
#define PANS_INCLUDE_PANS_MACROS_H_

#include <cassert>
#include <iostream>
#include <cstdint>

#define PANS_ASSERT(condition) \
    if (!(condition)) [[unlikely]] { \
        std::cerr << __FILE__ << ":" << __LINE__ << "ASSERT FAILED: " << #condition << "\nStacktrace: to do \n"; \
        assert(condition); \
    }

#define PANS_ASSERT2(condition, message) \
    if (!(condition)) [[unlikely]] { \
        std::cerr << __FILE__ << ":" << __LINE__ << " Assert " << #condition << " failed. [" << message << "].\nStacktrace: to do \n"; \
        assert(condition); \
    }

#define ASSERT_RETVAL(condition, retval) \
    do { \
        if (condition) [[likely]] break; \
        PANS_ASSERT(condition) \
        return retval; \
    } while (false)

#define ASSERT_RETVAL2(condition, retval, message) \
    do { \
        if (condition) [[likely]] break; \
        PANS_ASSERT2(condition, message) \
        return retval; \
    } while (false)

#define ASSERT_RETNONE(condition) \
    do { \
        if (condition) [[likely]] break; \
        PANS_ASSERT(condition) \
        return; \
    } while (false)

#define ASSERT_RETNONE2(condition, message) \
    do { \
        if (condition) [[likely]] break; \
        PANS_ASSERT2(condition, message) \
        return; \
    } while (false)

#define ASSERT_NOEFFECT(condition) \
    do { \
        if (condition) [[likely]] break; \
        PANS_ASSERT(condition) \
    } while (false)

#define ASSERT_NOEFFECT2(condition, message) \
    do { \
        if (condition) [[likely]] break; \
        PANS_ASSERT2(condition, message) \
    } while (false)

#define ASSERT_CONTINUE(condition) \
    if (!(condition)) [[unlikely]] { \
        PANS_ASSERT(condition) \
        continue; \
    } else { }

#define ASSERT_CONTINUE2(condition, message) \
    if (!(condition)) [[unlikely]] { \
        PANS_ASSERT2(condition, message) \
        continue; \
    } else { }

#define ASSERT_BREAK(condition) \
    if (!(condition)) [[unlikely]] { \
        PANS_ASSERT(condition) \
        break; \
    } else { }

#define ASSERT_BREAK2(condition, message) \
    if (!(condition)) [[unlikely]] { \
        PANS_ASSERT2(condition, message) \
        break; \
    } else { }


#define INVALID64 (~0ULL)
#define INVALID32 0xFFFFFFFF
#define INVALID16 0xFFFF
#define INVALID8 0xFF

#define MAX_U8 0xFF
#define MAX_U16 0xFFFF
#define MAX_U32 0xFFFFFFFF
#define MAX_U64 (~0ULL)

using u8 = uint8_t;
using s8 = int8_t;
using u16 = uint16_t;
using s16 = int16_t;
using u32 = uint32_t;
using s32 = int32_t;
using u64 = uint64_t;
using s64 = int64_t;


#endif

