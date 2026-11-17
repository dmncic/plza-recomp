// Force-included (-include) when the Lumiose Online runtime (tools/linux/build_online.sh) is built with zig cc: zig's
// compiler runtime has no __cpu_model / __cpu_indicator_init, which clang's __builtin_cpu_supports needs. The features
// that runtime asks about, read with cpuid instead (AVX2 also needs the OS to save the AVX state: XCR0).
#pragma once
#if defined(__x86_64__)
#include <cpuid.h>  // (the compiler's own header: no libc header here, it would fix the feature macros before a file sets _GNU_SOURCE)
static inline int zr_cpu_supports(const char *f) {
    unsigned a, b, c, d, b7 = 0, c7, d7;
    if (!__get_cpuid(1, &a, &b, &c, &d)) return 0;
    if (__get_cpuid_max(0, 0) >= 7) __cpuid_count(7, 0, a, b7, c7, d7);
    int avx_os = 0;
    if (c >> 27 & 1) {  // OSXSAVE
        unsigned lo, hi;
        __asm__ volatile("xgetbv" : "=a"(lo), "=d"(hi) : "c"(0));
        avx_os = (lo & 6) == 6;
    }
    if (!__builtin_strcmp(f, "sse4.2")) return c >> 20 & 1;
    if (!__builtin_strcmp(f, "popcnt")) return c >> 23 & 1;
    if (!__builtin_strcmp(f, "fma")) return (c >> 12 & 1) && avx_os;
    if (!__builtin_strcmp(f, "avx2")) return (b7 >> 5 & 1) && avx_os;
    if (!__builtin_strcmp(f, "bmi2")) return b7 >> 8 & 1;
    return 0;
}
#define __builtin_cpu_init() ((void)0)
#define __builtin_cpu_supports(f) zr_cpu_supports(f)
#endif
