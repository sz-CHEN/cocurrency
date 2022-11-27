#include "cstdthreads.h"
#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 201112L ||                \
    (defined(__STDC_NO_THREADS__) && __STDC_NO_THREADS__)

#include "cstdtime.h"
#if defined(_WIN32)
#include <Windows.h>
#include <process.h>

int thrd_create(thrd_t *thr, thrd_start_t func, void *arg) {
    HANDLE hthr = CreateThread(NULL, 0, func, arg, 0, thr);
    if (hthr == NULL) {
        switch (GetLastError()) {
        case ERROR_NOT_ENOUGH_MEMORY:
            return thrd_nomem;
            break;

        default:
            return thrd_error;
            break;
        }
    }
    CloseHandle(hthr);
    return thrd_success;
}

int thrd_equal(thrd_t lhs, thrd_t rhs) { return lhs == rhs; }

thrd_t thrd_current(void) { return GetCurrentThreadId(); }

static LARGE_INTEGER cpuFreq = {.QuadPart = 0};
static INIT_ONCE cpuFreqOnce = INIT_ONCE_STATIC_INIT;

static BOOL CALLBACK
cpuFreqGet(PINIT_ONCE InitOnce, // Pointer to one-time initialization structure
           PVOID freq,       // Optional parameter passed by InitOnceExecuteOnce
           PVOID *lpContext) // Receives pointer to event object
{
    if (!QueryPerformanceFrequency(freq)) {
        ((LARGE_INTEGER *)freq)->QuadPart = 0;
        return FALSE;
    }
    return TRUE;
}

int thrd_sleep(const struct timespec *duration, struct timespec *remaining) {
    InitOnceExecuteOnce(&cpuFreqOnce, cpuFreqGet, &cpuFreq, NULL);
    LARGE_INTEGER startCounter, endCounter;
    if (remaining != NULL) {
        if (!QueryPerformanceCounter(&startCounter)) {
            startCounter.QuadPart = 0;
        }
    }
    Sleep(duration->tv_sec * 1000 + duration->tv_nsec / 1000000);
    if (remaining != NULL) {
        if (!QueryPerformanceCounter(&endCounter)) {
            endCounter.QuadPart = 0;
        }
        if (endCounter.QuadPart == 0 || startCounter.QuadPart == 0 ||
            cpuFreq.QuadPart == 0) {
            remaining->tv_nsec = 0;
            remaining->tv_sec = 0;
        } else {
            ULARGE_INTEGER intervalCounter = {
                .QuadPart = endCounter.QuadPart - startCounter.QuadPart};
            ULARGE_INTEGER durationCounter = {
                .QuadPart =
                    cpuFreq.QuadPart * remaining->tv_sec +
                    cpuFreq.QuadPart * remaining->tv_nsec / 1000000000ull};
            if (durationCounter.QuadPart <= intervalCounter.QuadPart) {
                remaining->tv_nsec = 0;
                remaining->tv_sec = 0;
            } else {
                ULARGE_INTEGER differCounter = {.QuadPart =
                                                    intervalCounter.QuadPart -
                                                    durationCounter.QuadPart};
                remaining->tv_sec = differCounter.QuadPart / cpuFreq.QuadPart;
                remaining->tv_nsec = (differCounter.QuadPart -
                                      remaining->tv_sec * cpuFreq.QuadPart) *
                                     1000000000ull / cpuFreq.QuadPart;
            }
        }
    }
    return 0;
}

void thrd_yield(void) { SwitchToThread(); }

void thrd_exit(int res) { ExitThread(res); }

int thrd_detach(thrd_t thr) {
    // if (!CloseHandle(ThrdToHandle(thr))) {
    //     return thrd_error;
    // }
    return thrd_success;
}

int thrd_join(thrd_t thr, int *res) {
    HANDLE pthr = OpenProcess(THREAD_ALL_ACCESS, FALSE, thr);
    DWORD ret = WaitForSingleObject(pthr, INFINITE);
    int status = thrd_success;
    switch (ret) {
    case WAIT_OBJECT_0:
        if (res != NULL) {
            DWORD exitCode = 0;
            GetExitCodeThread(pthr, &exitCode);
            *res = (int)exitCode;
        }
        break;

    default:
        status = thrd_error;
        break;
    }
    CloseHandle(pthr);
    return status;
}

static BOOL CALLBACK InitOnceFunctionAgent(
    PINIT_ONCE InitOnce, // Pointer to one-time initialization structure
    PVOID Parameter,     // Optional parameter passed by InitOnceExecuteOnce
    PVOID *lpContext)    // Receives pointer to event object
{
    if (Parameter != NULL) {
        void (*func)(void) = Parameter;
        func();
    }
    return TRUE;
}

void call_once(once_flag *flag, void (*func)(void)) {
    InitOnceExecuteOnce(flag, InitOnceFunctionAgent, func, NULL);
}

#if (_WIN32_WINNT >= _WIN32_WINNT_WIN7)
#define INIT_MTX(mtx) InitializeSRWLock(mtx)
#define LOCK_MTX(mtx) AcquireSRWLockExclusive(mtx)
#define TRYLOCK_MTX(mtx) TryAcquireSRWLockExclusive(mtx)
#define UNLOCK_MTX(mtx) ReleaseSRWLockExclusive(mtx)
#define DESTROY_MTX(mtx)                                                       \
    do {                                                                       \
    } while (0)
#else
#define INIT_MTX(mtx) InitializeCriticalSection(mtx)
#define LOCK_MTX(mtx) EnterCriticalSection(mtx)
#define TRYLOCK_MTX(mtx) TryEnterCriticalSection(mtx)
#define UNLOCK_MTX(mtx) LeaveCriticalSection(mtx)
#define DESTROY_MTX(mtx) DeleteCriticalSection(mtx)
#endif

int mtx_init(mtx_t *mutex, int type) {
    if (type != mtx_plain && type != mtx_timed) {
        return thrd_error;
    }
    INIT_MTX(mutex);
    return thrd_success;
}

int mtx_lock(mtx_t *mutex) {
    LOCK_MTX(mutex);
    return thrd_success;
}

int mtx_trylock(mtx_t *mutex) {
    if (!TRYLOCK_MTX(mutex)) {
        return thrd_busy;
    }
    return thrd_success;
}

int mtx_timedlock(mtx_t *mutex, const struct timespec *abstime) {
    struct timespec nowtime, t;
    t.tv_sec = abstime->tv_sec + abstime->tv_nsec / 1000000000ull;
    t.tv_nsec = abstime->tv_nsec % 1000000000ull;
    while (thrd_success != mtx_trylock(mutex)) {
        timespec_get(&nowtime, TIME_UTC);
        if (nowtime.tv_sec > t.tv_sec ||
            (nowtime.tv_sec == t.tv_sec && nowtime.tv_nsec > t.tv_nsec)) {
            return thrd_timedout;
        }
        thrd_yield();
    }
    return thrd_success;
}

int mtx_unlock(mtx_t *mutex) {
    UNLOCK_MTX(mutex);
    return thrd_success;
}

void mtx_destroy(mtx_t *mutex) { DESTROY_MTX(mutex); }

int cnd_init(cnd_t *cond) {
    InitializeConditionVariable(cond);
    return thrd_success;
}

int cnd_signal(cnd_t *cond) {
    WakeConditionVariable(cond);
    return thrd_success;
}

int cnd_broadcast(cnd_t *cond) {
    WakeAllConditionVariable(cond);
    return thrd_success;
}

int cnd_wait(cnd_t *cond, mtx_t *mutex) {
#if (_WIN32_WINNT >= _WIN32_WINNT_WIN7)
    if (SleepConditionVariableSRW(cond, mutex, INFINITE, 0))
#else
    if (SleepConditionVariableCS(cond, mutex, INFINITE))
#endif
    {
        return thrd_success;
    }
    return thrd_error;
}

int cnd_timedwait(cnd_t *cond, mtx_t *mutex, const struct timespec *abstime) {
    struct timespec nowtime, t;
    t.tv_sec = abstime->tv_sec + abstime->tv_nsec / 1000000000ull;
    t.tv_nsec = abstime->tv_nsec % 1000000000ull;
    timespec_get(&nowtime, TIME_UTC);
    if (abstime == NULL || abstime->tv_sec < nowtime.tv_sec ||
        (abstime->tv_sec == nowtime.tv_sec &&
         abstime->tv_nsec < nowtime.tv_nsec)) {
        return thrd_timedout;
    }
    nowtime.tv_sec -= abstime->tv_sec;
    if (nowtime.tv_nsec < abstime->tv_nsec) {
        nowtime.tv_nsec = nowtime.tv_nsec + 1000000000 - abstime->tv_nsec;
        nowtime.tv_sec -= 1;
    }
#if (_WIN32_WINNT >= _WIN32_WINNT_WIN7)
    if (SleepConditionVariableSRW(
            cond, mutex, nowtime.tv_sec * 1000 + nowtime.tv_nsec / 1000000, 0))
#else
    if (SleepConditionVariableCS(
            cond, mutex, nowtime.tv_sec * 1000 + nowtime.tv_nsec / 1000000))
#endif
    {
        return thrd_success;
    }
    return thrd_timedout;
}

void cnd_destroy(cnd_t *cond) { return; }

static tss_dtor_t _dtors[FLS_MAXIMUM_AVAILABLE];

int tss_create(tss_t *tss_key, tss_dtor_t destructor) {
    *tss_key = TlsAlloc();
    if (*tss_key == TLS_OUT_OF_INDEXES) {
        return thrd_error;
    }
    _dtors[*tss_key] = destructor;
    return thrd_success;
}

int tss_set(tss_t tss_id, void *val) {
    TlsSetValue(tss_id, val);
    return 0;
}

void *tss_get(tss_t tss_key) { return TlsGetValue(tss_key); }

void tss_delete(tss_t tss_id) {
    if (_dtors[tss_id] != NULL) {
        _dtors[tss_id](TlsGetValue(tss_id));
        _dtors[tss_id] = NULL;
    }
    TlsFree(tss_id);
}

#elif defined(__VXWORKS__) &&                                                  \
    (!defined(INCLUDE_POSIX_PTHREADS) || !INCLUDE_POSIX_PTHREADS) && 0
#include <errnoLib.h>
#include <semLib.h>
#include <sysLib.h>
#include <taskLib.h>
#include <tickLib.h>

int thrd_create_withname(thrd_t *thr, thrd_start_t func, void *arg,
                         char *name) {
    int priority = 255;
    STATUS err = taskPriorityGet(taskIdSelf(), &priority);
    if (err != OK) return thrd_error;
    TASK_ID id = taskSpawn(name, priority, 0, 4096, func, arg, 2, 3, 4, 5, 6, 7,
                           8, 9, 10);
    if (id != TASK_ID_ERROR) {
        *thr = id;
        return thrd_success;
    }
    switch (errnoGet()) {
    case S_memLib_NOT_ENOUGH_MEMORY:
        return thrd_nomem;
        break;

    default:
        return thrd_error;
        break;
    }
}
int thrd_create(thrd_t *thr, thrd_start_t func, void *arg) {
    return thrd_create_withname(thr, func, arg, NULL);
}
int thrd_equal(thrd_t lhs, thrd_t rhs) { return lhs == rhs; }
thrd_t thrd_current(void) { return taskIdSelf(); }
int thrd_sleep(const struct timespec *duration, struct timespec *remaining) {
    UINT64 startTicks = tick64Get();
    int clkRate = sysClkRateGet();
    UINT64 ticks =
        (duration->tv_sec + duration->tv_nsec / 1000000000) * clkRate;
    UINT64 nticks = (duration->tv_nsec%1000000000) * clkRate / 1000000000);
    if (OK == taskDelay(ticks + nticks)) {
        return thrd_success;
    }
    int err = errnoGet();
    UINT64 dticks = tick64Get() - startTicks;
    if (remaining != NULL) {
        remaining->tv_sec = dticks / clkRate;
        remaining->tv_nsec =
            ((dticks - (remaining->tv_sec * clkRate)) * 1000000000) / clkRate;
        remaining->tv_sec = duration->tv_sec - remaining->tv_sec;
        remaining->tv_sec = duration->tv_nsec - remaining->tv_nsec;
    }
    switch (err) {
    case EINTR:
        return -1;
        break;

    default:
        return -(err + 1);
        break;
    }
}

void thrd_yield(void) { taskDelay(0); }

void thrd_exit(int res) { taskExit(res); }

int thrd_detach(thrd_t thr) { return; }

int thrd_join(thrd_t thr, int *res) {
    STATUS err = taskDelete(thr);
    if (err == OK) return thrd_success;
    return thrd_error;
}

void call_once(once_flag *flag, void (*func)(void)) {
    if (flag->init == SEM_ID_NULL) {
        flag->init = semBCreate(SEM_Q_FIFO, SEM_FULL);
    }
    if (OK != semBTake(flag->init, NO_WAIT)) {
        return;
    }
    func();
}

int mtx_init(mtx_t *mutex, int type) {
    if (type != mtx_plain || type != mtx_timed) {
        return thrd_error;
    }
    SEM_ID mtx = semMCreate(SEM_Q_FIFO);
    if (mtx != SEM_ID_NULL) {
        *mutex = mtx;
        return thrd_success;
    }
    switch (errnoGet()) {
    case S_memLib_NOT_ENOUGH_MEMORY:
        return thrd_nomem;
        break;
    default:
        return thrd_error;
        break;
    }
}

int mtx_lock(mtx_t *mutex) {
    STATUS err = semTake(*mutex, WAIT_FOREVER);
    if (err == OK) return thrd_success;
    switch (errnoGet()) {
    case S_objLib_OBJ_TIMEOUT:
        return thrd_busy;
        break;

    default:
        return thrd_error;
        break;
    }
}

int mtx_trylock(mtx_t *mutex) {
    STATUS err = semTake(*mutex, NO_WAIT);
    if (err == OK) return thrd_success;
    switch (errnoGet()) {
    case S_objLib_OBJ_TIMEOUT:
        return thrd_busy;
        break;

    default:
        return thrd_error;
        break;
    }
}
int mtx_timedlock(mtx_t *mutex, const struct timespec *abstime) {
    struct timespec nowtime, t;
    t.tv_sec = abstime->tv_sec + abstime->tv_nsec / 1000000000ull;
    t.tv_nsec = abstime->tv_nsec % 1000000000ull;
    timespec_get(&nowtime, TIME_UTC);
    if (nowtime.tv_sec > t.tv_sec ||
        (nowtime.tv_sec == t.tv_sec && nowtime.tv_nsec > t.tv_nsec)) {
        return mtx_trylock(mutex);
    }
    nowtime.tv_sec = t.tv_sec - nowtime.tv_sec;
    if (nowtime.tv_nsec > t.tv_nsec) {
        nowtime.tv_nsec = t.tv_nsec + 1000000000 - nowtime.tv_nsec;
        nowtime.tv_sec -= 1;
    } else {
        nowtime.tv_nsec = t.tv_nsec - nowtime.tv_nsec;
    }
    int clkRate = sysClkRateGet();
    UINT64 ticks = (nowtime->tv_sec + nowtime->tv_nsec / 1000000000) * clkRate;
    UINT64 nticks = (nowtime->tv_nsec%1000000000) * clkRate / 1000000000);
    STATUS err = semTake(*mutex, ticks + nticks);
    if (err == OK) return thrd_success;
    switch (errnoGet()) {
    case S_objLib_OBJ_TIMEOUT:
        return thrd_busy;
        break;

    default:
        return thrd_error;
        break;
    }
}

int mtx_unlock(mtx_t *mutex) {
    STATUS err = semGive(*mutex);
    if (err == OK) return thrd_success;
    return thrd_error;
}
void mtx_destroy(mtx_t *mutex) { semDelete(*mutex); }

typedef union {
    cnd_t cnd;
    struct {
        SEM_ID count;
        SEM_ID wait;
        SEM_ID waited;
    } real;
} real_cnd_t;

int cnd_init(cnd_t *cond) {
    real_cnd_t *cnd = (real_cnd_t *)cond;
    cnd->real.count = SEM_ID_NULL;
    cnd->real.wait = SEM_ID_NULL;
    cnd->real.waited = SEM_ID_NULL;
    int err = OK;
    do {
        cnd->real.count = semCCreate(SEM_Q_FIFO, 0);
        if (err != OK) break;
        cnd->real.wait = semCCreate(SEM_Q_FIFO, 0);
        if (err != OK) break;
        cnd->real.waited = semCCreate(SEM_Q_FIFO, 0);
        if (err != OK) break;
    } while (0);
    if (err == OK) {
        return thrd_success;
    }
    semDelete(cnd->real.count);
    semDelete(cnd->real.wait);
    semDelete(cnd->real.waited);
    switch (err) {
    case S_memLib_NOT_ENOUGH_MEMORY:
        return thrd_nomem;
        break;
    default:
        return thrd_error;
        break;
    }
}
int cnd_signal(cnd_t *cond) {
    real_cnd_t *cnd = (real_cnd_t *)cond;
    if (OK == semTake(cnd->real.count, NO_WAIT)) {
        if (OK != semGive(cnd->real.wait)) {
            return thrd_error;
        }
        if (OK == semTake(cnd->real.waited, WAIT_FOREVER)) {
            return thrd_success;
        }
        return thrd_error;
    }
    switch (errnoGet()) {
    case S_objLib_OBJ_TIMEOUT:
        return thrd_success;
        break;
    default:
        return thrd_error;
        break;
    }
}

int cnd_broadcast(cnd_t *cond) {
    real_cnd_t *cnd = (real_cnd_t *)cond;
    int beerr = 0;
    int n = 0;
    while (OK == semTake(cnd->real.count, NO_WAIT)) {
        if (OK == semGive(cnd->real.wait)) {
            ++n;
        } else {
            beerr = 1;
        }
    }
    while (n--) {
        if (OK != semTake(cnd->real.waited, WAIT_FOREVER)) {
            beerr = 1;
        }
    }
    if (beerr == 0) return thrd_success;
    return thrd_error;
}

int cnd_wait(cnd_t *cond, mtx_t *mutex) {
    real_cnd_t *cnd = (real_cnd_t *)cond;
    int ret = thrd_success;
    if (thrd_success != (ret = mtx_unlock(mutex))) {
        return ret;
    }
    if (OK != semGive(cnd->real.count)) {
        return thrd_error;
    }
    if (OK == semTake(cnd->real.wait, WAIT_FOREVER)) {
        if(thrd_success == mtx_trylock(mutex){
            if (OK == semGive(cnd->real.waited)) {
                return thrd_success;
            }
        } else{
            if (OK == semGive(cnd->real.waited)) {
                return mtx_lock(mutex);
            }
        }
    }
    return thrd_error;
}

int cnd_timedwait(cnd_t *cond, mtx_t *mutex, const struct timespec *abstime) {
    real_cnd_t *cnd = (real_cnd_t *)cond;
    int ret = thrd_success;
    if (thrd_success != (ret = mtx_unlock(mutex))) {
        return ret;
    }
    if (OK != semGive(cnd->real.count)) {
        return thrd_error;
    }
    unsigned int waitTicks = NO_WAIT;
    struct timespec nowtime, t;
    t.tv_sec = abstime->tv_sec + abstime->tv_nsec / 1000000000ull;
    t.tv_nsec = abstime->tv_nsec % 1000000000ull;
    timespec_get(&nowtime, TIME_UTC);
    if (nowtime.tv_sec > t.tv_sec ||
        (nowtime.tv_sec == t.tv_sec && nowtime.tv_nsec > t.tv_nsec)) {
        waitTicks = NO_WAIT;
    } else {
        nowtime.tv_sec = t.tv_sec - nowtime.tv_sec;
        if (nowtime.tv_nsec > t.tv_nsec) {
            nowtime.tv_nsec = t.tv_nsec + 1000000000 - nowtime.tv_nsec;
            nowtime.tv_sec -= 1;
        } else {
            nowtime.tv_nsec = t.tv_nsec - nowtime.tv_nsec;
        }
        int clkRate = sysClkRateGet();
        UINT64 ticks =
            (nowtime->tv_sec + nowtime->tv_nsec / 1000000000) * clkRate;
        UINT64 nticks = (nowtime->tv_nsec%1000000000) * clkRate / 1000000000);
        waitTicks = ticks + nticks;
    }
    if (OK == semTake(cnd->real.wait, waitTicks)) {
            if(thrd_success == mtx_trylock(mutex){
            if (OK == semGive(cnd->real.waited)) {
                return thrd_success;
            }
            } else{
            if (OK == semGive(cnd->real.waited)) {
                return mtx_lock(mutex);
            }
            }
    }
    return thrd_error;
}

void cnd_destroy(cnd_t *cond) {
    real_cnd_t *cnd = (real_cnd_t *)cond;
    semDelete(cnd->real.count);
    semDelete(cnd->real.wait);
    semDelete(cnd->real.waited);
}

#else
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#if defined(__VXWORKS__)
int thrd_create_withname(thrd_t *thr, thrd_start_t func, void *arg,
                         char *name) {
    int err = 0;
    pthread_attr_t attr;
    err = pthread_attr_init(&attr);
    if (err != 0) {
        return thrd_error;
    }
    do {
        err = pthread_attr_setname(&attr, name);
        if (err != 0) {
            break;
        }
        err = pthread_create(thr, &attr, func, arg);
    } while (0);
    pthread_attr_destroy(&attr);
    if (err == 0) {
        return thrd_success;
    }
    switch (err) {
    case EAGAIN:
        return thrd_nomem;
        break;

    default:
        return thrd_error;
        break;
    }
}
#endif

int thrd_create(thrd_t *thr, thrd_start_t func, void *arg) {
    int err = pthread_create(thr, NULL, func, arg);
    if (err == 0) {
        return thrd_success;
    }
    switch (err) {
    case EAGAIN:
        return thrd_nomem;
        break;

    default:
        return thrd_error;
        break;
    }
}

int thrd_equal(thrd_t lhs, thrd_t rhs) { return pthread_equal(lhs, rhs); }
thrd_t thrd_current(void) { return pthread_self(); }

int thrd_sleep(const struct timespec *duration, struct timespec *remaining) {
    if (-1 == nanosleep(duration, remaining)) {
        int err = errno;
        switch (err) {
        case EINTR:
            return -1;
            break;

        default:
            return -(err + 1);
            break;
        }
    }
    return 0;
}

void thrd_yield(void) { sched_yield(); }
void thrd_exit(int res) { pthread_exit(res); }
int thrd_detach(thrd_t thr) {
    if (0 == pthread_detach(thr)) {
        return thrd_success;
    }
    return thrd_error;
}
int thrd_join(thrd_t thr, int *res) {
    void *pres = NULL;
    if (0 == pthread_join(thr, &pres)) {
        if (res != NULL) *res = pres;
        return thrd_success;
    }
    return thrd_error;
}

void call_once(once_flag *flag, void (*func)(void)) {
    pthread_once(flag, func);
}

int mtx_init(mtx_t *mutex, int type) {
    int mtx_type;
    switch (type) {
    case mtx_plain:
        mtx_type = (PTHREAD_MUTEX_NORMAL);
        break;
    case mtx_recursive:
        mtx_type = (PTHREAD_MUTEX_RECURSIVE);
        break;
    case mtx_timed:
        mtx_type = (PTHREAD_MUTEX_NORMAL);
        break;
    default:
        return thrd_error;
        break;
    }
    int err = 0;
    pthread_mutexattr_t attr;
    err = pthread_mutexattr_init(&attr);
    if (err != 0) {
        if (err == ENOMEM) {
            return thrd_nomem;
        } else {
            return thrd_error;
        }
    }
    do {
        err = pthread_mutexattr_settype(&attr, mtx_type);
        if (err != 0) {
            break;
        }
        err = pthread_mutex_init(mutex, &attr);
    } while (0);
    pthread_mutexattr_destroy(&attr);
    switch (err) {
    case 0:
        return thrd_success;
    case ENOMEM:
        return thrd_nomem;
        break;

    default:
        return thrd_error;
        break;
    }
}
int mtx_lock(mtx_t *mutex) {
    int err = pthread_mutex_lock(mutex);
    if (err == 0) {
        return thrd_success;
    }
    switch (err) {
    case EBUSY:
        return thrd_busy;
        break;

    default:
        return thrd_error;
        break;
    }
}

int mtx_trylock(mtx_t *mutex) {
    int err = pthread_mutex_trylock(mutex);
    if (err == 0) {
        return thrd_success;
    }
    switch (err) {
    case EBUSY:
        return thrd_busy;
        break;

    default:
        return thrd_error;
        break;
    }
}

int mtx_timedlock(mtx_t *mutex, const struct timespec *abstime) {
    int err = pthread_mutex_timedlock(mutex, abstime);
    if (err == 0) {
        return thrd_success;
    }
    switch (err) {
    case ETIMEDOUT:
        return thrd_timedout;
        break;
    default:
        return thrd_error;
        break;
    }
}

int mtx_unlock(mtx_t *mutex) {
    int err = pthread_mutex_unlock(mutex);
    if (err == 0) {
        return thrd_success;
    }
    switch (err) {
    case EBUSY:
        return thrd_busy;
        break;

    default:
        return thrd_error;
        break;
    }
}
void mtx_destroy(mtx_t *mutex) { pthread_mutex_destroy(mutex); }

int cnd_init(cnd_t *cond) {
    int err = pthread_cond_init(cond, NULL);
    if (err == 0) {
        return thrd_success;
    }
    switch (err) {
    case ENOMEM:
        return thrd_nomem;
        break;

    default:
        return thrd_error;
        break;
    }
}
int cnd_signal(cnd_t *cond) {
    int err = pthread_cond_signal(cond);
    if (err == 0) {
        return thrd_success;
    }
    switch (err) {
    default:
        return thrd_error;
        break;
    }
}
int cnd_broadcast(cnd_t *cond) {
    int err = pthread_cond_broadcast(cond);
    if (err == 0) {
        return thrd_success;
    }
    switch (err) {
    default:
        return thrd_error;
        break;
    }
}
int cnd_wait(cnd_t *cond, mtx_t *mutex) {
    int err = pthread_cond_wait(cond, mutex);
    if (err == 0) {
        return thrd_success;
    }
    switch (err) {
    default:
        return thrd_error;
        break;
    }
}

int cnd_timedwait(cnd_t *cond, mtx_t *mutex, const struct timespec *abstime) {
    int err = pthread_cond_timedwait(cond, mutex, abstime);
    if (err == 0) {
        return thrd_success;
    }
    switch (err) {
    case ETIMEDOUT:
        return thrd_timedout;
        break;
    default:
        return thrd_error;
        break;
    }
}
void cnd_destroy(cnd_t *cond) { pthread_cond_destroy(cond); }

int tss_create(tss_t *tss_key, tss_dtor_t destructor) {
    int err = pthread_key_create(tss_key, destructor);
    if (err == 0) {
        return thrd_success;
    }
    switch (err) {
    case ENOMEM:
        return thrd_nomem;
        break;
    default:
        return thrd_error;
        break;
    }
}
int tss_set(tss_t tss_id, void *val) {
    int err = pthread_setspecific(tss_id, val);
    if (err == 0) {
        return thrd_success;
    }
    switch (err) {
    case ENOMEM:
        return thrd_nomem;
        break;
    default:
        return thrd_error;
        break;
    }
}
void *tss_get(tss_t tss_key) { return pthread_getspecific(tss_key); }
void tss_delete(tss_t tss_id) { pthread_key_delete(tss_id); }

#endif
#endif
