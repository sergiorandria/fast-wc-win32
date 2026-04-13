#pragma once

#define PROJ_PATH L"C:\\Users\\sergi\\source\\repos\\fast-wc\\fast-wc"

// tp
#define ASSERT_DECAY_EQUAL(T, DecayedType) static_assert(is_decay_equal<T, DecayedType>, "Type does not decay to the expected type.") 
#define ASSERT_TASK_FITS(Callable) static_assert(sizeof(std::decay_t<Callable>) <= FAST_WC_TASK_WORKER_SIZE, "Callable is too large to fit in the task worker.")

#define TASK_WORKER_ASSERTIONS_ENABLED 

#ifdef _GNU
#define __FORCE_INLINE [[gnu::always_inline]] inline
#else 
#define __FORCE_INLINE
#endif 

#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)

#define PROGRAM_NAME L"fast-wc"

#if defined(_MSC_VER) && !defined(__GNUG__)
#include <xmmintrin.h>

inline void __builtin_prefetch(const void* ptr, int /*rw*/ = 0, int locality = 0) {
#if defined(_MM_HINT_T0) || defined(_MM_HINT_T1) || defined(_MM_HINT_T2) || defined(_MM_HINT_NTA)
    if (locality == 1) {
#if defined(_MM_HINT_T1)
        _mm_prefetch(reinterpret_cast<const char*>(ptr), _MM_HINT_T1);
#else
        _mm_prefetch(reinterpret_cast<const char*>(ptr), _MM_HINT_T0);
#endif
    } else if (locality == 2) {
#if defined(_MM_HINT_T2)
        _mm_prefetch(reinterpret_cast<const char*>(ptr), _MM_HINT_T2);
#else
        _mm_prefetch(reinterpret_cast<const char*>(ptr), _MM_HINT_T0);
#endif
    } else if (locality == 3) {
#if defined(_MM_HINT_NTA)
        _mm_prefetch(reinterpret_cast<const char*>(ptr), _MM_HINT_NTA);
#else
        _mm_prefetch(reinterpret_cast<const char*>(ptr), _MM_HINT_T0);
#endif
    } else {
        _mm_prefetch(reinterpret_cast<const char*>(ptr), _MM_HINT_T0);
    }
#else
    // Fallback: call with 0 if no hint macros are available
    _mm_prefetch(reinterpret_cast<const char*>(ptr), 0);
#endif
}
#endif

#ifdef __GNUG__ 
#define __SSE2_TARGET [[gnu::target("sse2")]]
#else 
#define __SSE2_TARGET 
#endif