#ifndef COMPAT_CSTD_TIME_H
#define COMPAT_CSTD_TIME_H

#ifdef __cplusplus
extern "C" {
#endif

#include <time.h>

#ifdef _MSC_VER
#if _MSC_VER < 1900
#define HAS_TIMESPEC 0
#define TIME_VERSION 199000L
#else
#ifdef _CRT_NO_TIME_T
#define HAS_TIMESPEC 0
#else
#define HAS_TIMESPEC 1
#endif
#define TIME_VERSION __STDC_VERSION__
#endif
#else
#define HAS_TIMESPEC 1
#define TIME_VERSION __STDC_VERSION__
#endif

struct timespec;
#if !HAS_TIMESPEC
struct timespec {
    time_t tv_sec;
    long tv_nsec;
};
#endif

#ifndef TIME_UTC
#define TIME_UTC 1
#endif

int timespec_get(struct timespec *ts, int base);
int timespec_getres(struct timespec *ts, int base);

// errno_t localtime_s(const time_t *timer, struct tm *buf);
// struct tm *localtime_r(const time_t *timer, struct tm *buf);

// errno_t asctime_s(char *buf, rsize_t bufsz, const struct tm *time_ptr);
// char *asctime_r(const struct tm *time_ptr, char *buf);

// errno_t ctime_s(char *buf, rsize_t bufsz, const time_t *timer);
// char *ctime_r(const time_t *timer, char *buf);

// struct tm *gmtime_s(const time_t * timer, struct tm * buf);
// struct tm *gmtime_r(const time_t *timer, struct tm *buf);

#ifdef __cplusplus
}
#endif

#endif
