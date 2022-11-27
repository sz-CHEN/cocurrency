#ifndef COMPAT_CSTD_THREADS_H
#define COMPAT_CSTD_THREADS_H

#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 201112L ||                \
    (defined(__STDC_NO_THREADS__) && __STDC_NO_THREADS__)

#ifdef __cplusplus
extern "C" {
#endif

struct timespec;

#if defined(_MSC_VER)
#if __STDC_VERSION__ < 201112L
#define _Thread_local __declspec(thread)
#endif
#if __STDC_VERSION__ <= 201710L &&                                             \
    (!defined(__cplusplus) || __cplusplus < 201103L)
#define thread_local _Thread_local
#endif
#else
#if __STDC_VERSION__ < 201112L
#define _Thread_local __thread
#endif
#if __STDC_VERSION__ <= 201710L &&                                             \
    (!defined(__cplusplus) || __cplusplus < 201103L)
#define thread_local _Thread_local
#endif
#endif

#ifdef _WIN32
#include <Windows.h>
typedef DWORD thrd_t;
typedef INIT_ONCE once_flag;
#define ONCE_FLAG_INIT INIT_ONCE_STATIC_INIT
#if (_WIN32_WINNT >= _WIN32_WINNT_WIN7)
typedef SRWLOCK mtx_t;
#else
typedef CRITICAL_SECTION mtx_t;
#endif
typedef CONDITION_VARIABLE cnd_t;
typedef DWORD tss_t;
#elif defined(__VXWORKS__) &&                                                  \
    (!defined(INCLUDE_POSIX_PTHREADS) || !INCLUDE_POSIX_PTHREADS) && 0
#include <vxWorks.h>
typedef TASK_ID thrd_t;
typedef SEM_ID mtx_t;
typedef struct {
    SEM_ID _dummy[3];
} cnd_t;
typedef struct {
    SEM_ID init;
} once_flag;
#define ONCE_FLAG_INIT                                                         \
    { SEM_ID_NULL }
#else
#include <pthread.h>
typedef pthread_t thrd_t;
typedef pthread_once_t once_flag;
#define ONCE_FLAG_INIT PTHREAD_ONCE_INIT
typedef pthread_mutex_t mtx_t;
typedef pthread_cond_t cnd_t;
typedef pthread_key_t tss_t;
#endif

enum { thrd_success = 0, thrd_nomem, thrd_timedout, thrd_busy, thrd_error };

typedef int (*thrd_start_t)(void *);
#if defined(__VXWORKS__)
int thrd_create_withname(thrd_t *thr, thrd_start_t func, void *arg, char *name);
#else
#define thrd_create_withname(thr, func, arg, name) thrd_create(thr, func, arg)
#endif
int thrd_create(thrd_t *thr, thrd_start_t func, void *arg);
int thrd_equal(thrd_t lhs, thrd_t rhs);
thrd_t thrd_current(void);
int thrd_sleep(const struct timespec *duration, struct timespec *remaining);
void thrd_yield(void);
void thrd_exit(int res);
int thrd_detach(thrd_t thr);
int thrd_join(thrd_t thr, int *res);

void call_once(once_flag *flag, void (*func)(void));

enum { mtx_plain = 0, mtx_recursive = 1, mtx_timed = 2 };

int mtx_init(mtx_t *mutex, int type);
int mtx_lock(mtx_t *mutex);
int mtx_trylock(mtx_t *mutex);
int mtx_timedlock(mtx_t *mutex, const struct timespec *abstime);
int mtx_unlock(mtx_t *mutex);
void mtx_destroy(mtx_t *mutex);

int cnd_init(cnd_t *cond);
int cnd_signal(cnd_t *cond);
int cnd_broadcast(cnd_t *cond);
int cnd_wait(cnd_t *cond, mtx_t *mutex);
int cnd_timedwait(cnd_t *cond, mtx_t *mutex, const struct timespec *abstime);
void cnd_destroy(cnd_t *cond);

typedef void (*tss_dtor_t)(void *);
int tss_create(tss_t *tss_key, tss_dtor_t destructor);
int tss_set(tss_t tss_id, void *val);
void *tss_get(tss_t tss_key);
void tss_delete(tss_t tss_id);

#ifdef __cplusplus
}
#endif
#else
#include <threads.h>
#if defined(__VXWORKS__)
int thrd_create_withname(thrd_t *thr, thrd_start_t func, void *arg, char *name);
#else
#define thrd_create_withname(thr, func, arg, name) thrd_create(thr, func, arg)
#endif
#endif

#endif
