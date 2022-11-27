#include "cstdtime.h"
#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 201112L ||                \
    defined(_INC_TIME_INL)
#if defined(_MSC_VER) && defined(_WIN32)
#if _MSC_VER < 1900 || defined(_INC_TIME_INL)
#include <VersionHelpers.h>
#include <Windows.h>
int timespec_get(struct timespec *ts, int base) {
    if (base != TIME_UTC) return 0;
    static const unsigned long long EPOCH_DIFFERENCE_SECONDS = 11644473600u;
    FILETIME ft;
    ULARGE_INTEGER fnowtime;
    // #if (_WIN32_WINNT >= _WIN32_WINNT_WIN8)
    if (IsWindows8OrGreater()) {
        GetSystemTimePreciseAsFileTime(&ft);
        // #else
    } else {
        GetSystemTimeAsFileTime(&ft);
        // #endif
    }
    fnowtime.HighPart = ft.dwHighDateTime;
    fnowtime.LowPart = ft.dwLowDateTime;
    ts->tv_sec = fnowtime.QuadPart / (10000000u) - EPOCH_DIFFERENCE_SECONDS;
    ts->tv_nsec = 100 * (fnowtime.QuadPart % 10000000u);
    return base;
}
#endif
#else
int timespec_get(struct timespec *ts, int base) {
    if (base != TIME_UTC) return 0;
    return clock_gettime(CLOCK_REALTIME, ts) == 0 ? base : 0;
}

#endif
#endif

#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 202301L ||                \
    defined(_INC_TIME_INL)
#if defined(_MSC_VER) && defined(_WIN32)
#include <Windows.h>

#include <VersionHelpers.h>

#if (_WIN32_WINNT >= _WIN32_WINNT_WIN8)
#pragma comment(lib, "MinCore")
#endif
int timespec_getres(struct timespec *ts, int base) {
    if (base != TIME_UTC) return 0;
    // #if (_WIN32_WINNT >= _WIN32_WINNT_WIN10)
    if (IsWindows10OrGreater()) {
        DWORD64 tadj = 0;
        DWORD64 tinc = 0;
        BOOL tadjdis = 0;
        GetSystemTimeAdjustmentPrecise(&tadj, &tinc, &tadjdis);
        ts->tv_sec = 1 / tadj;
        ts->tv_nsec = 1000000000 / tadj;
        // #elif (_WIN32_WINNT >= _WIN32_WINNT_WIN8)
    } else if (IsWindows8OrGreater()) {
        ts->tv_sec = 0;
        ts->tv_nsec = 1000;
        // #else
    } else {
        DWORD tadj = 0;
        DWORD tinc = 0;
        BOOL tadjdis = 0;
        GetSystemTimeAdjustment(&tadj, &tinc, &tadjdis);
        ts->tv_sec = tadj / 10000000;
        ts->tv_nsec = (tadj % 10000000) * 100;
        // #endif
    }
    return base;
}
#else
int timespec_getres(struct timespec *ts, int base) {
    if (base != TIME_UTC) return 0;
    return clock_getres(CLOCK_REALTIME, ts) == 0 ? base : 0;
}
#endif
#endif
