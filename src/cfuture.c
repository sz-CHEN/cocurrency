#include "cfuture.h"

#include <cstdthreads.h>
#include <stdlib.h>

int futureInit(Future *_future, fooFutureGetValue getValue,
               fooFutureSetValue setValue) {

    _future->getValue = getValue;
    _future->setValue = setValue;
    if (thrd_success != mtx_init(&_future->mtx, mtx_timed)) {
        return -1;
    }
    if (thrd_success != cnd_init(&_future->cnd)) {
        mtx_destroy(&_future->mtx);
        return -1;
    }
    _future->validResult = 0;
    _future->valid = 1;
    return 0;
}
int futureDestroy(Future *_future) {
    _future->valid = 0;
    _future->validResult = 0;
    cnd_destroy(&_future->cnd);
    mtx_destroy(&_future->mtx);
    return 0;
}

static int futureSet(Future *_future, const void *pvalue) {
    int ret = 0;
    mtx_lock(&_future->mtx);
    if (_future->setValue) _future->setValue(_future, pvalue);
    _future->validResult = 1;
    ret = cnd_broadcast(&_future->cnd);
    mtx_unlock(&_future->mtx);
    return ret;
}

int futureGet(Future *_future, void *pvalue) {
    int ret = futureWait(_future);
    if (ret != 0) return ret;
    if (_future->getValue) _future->getValue(_future, pvalue);
    return 0;
}

int futureValid(Future *_future) { return _future->valid; }

int futureWait(Future *_future) {
    int ret = 0;
    if (!_future->valid) return NO_STATE;
    mtx_lock(&_future->mtx);
    while (!_future->validResult) {
        cnd_wait(&_future->cnd, &_future->mtx);
    }
    mtx_unlock(&_future->mtx);
    return ret;
}

int futureWaitUntil(Future *_future, const struct timespec *abstime) {
    int ret = 0;
    if (!_future->valid) return NO_STATE;
    ret = mtx_timedlock(&_future->mtx, abstime);
    if (ret != thrd_success) {
        if (ret == thrd_timedout) {
            return FUTURE_TIMEOUT;
        }
        return -1;
    }
    if (!_future->validResult) {
        cnd_timedwait(&_future->cnd, &_future->mtx, abstime);
    }
    mtx_unlock(&_future->mtx);
    if (_future->validResult) {
        return FUTURE_READY;
    }
    return FUTURE_TIMEOUT;
}

Future *getFuture(Promise *_promise) { return _promise->_future; }

int promiseBind(Promise *_promise, Future *_future) {
    _promise->_future = _future;
    return 0;
}
int promiseSetValue(Promise *_promise, const void *pvalue) {
    return futureSet(_promise->_future, pvalue);
}

int packagedTaskBind(PackagedTask *_task, Promise *_promise,
                     void (*foo)(void *, void *pret), void *valueBuffer) {
    _task->_promise = _promise;
    _task->_valueBuffer = valueBuffer;
    _task->_foo = foo;
    return 0;
}

int packagedTaskCall(PackagedTask *_task, void *args) {
    _task->_foo(args, _task->_valueBuffer);
    return promiseSetValue(_task->_promise, _task->_valueBuffer);
}

int packagedTaskReset(PackagedTask *_task) {
    int ret = 0;
    Future *_future = getFuture(_task->_promise);
    mtx_lock(&_future->mtx);
    _future->validResult = 0;
    mtx_unlock(&_future->mtx);
    return ret;
}

typedef struct {
    Future *_future;
    void (*foo)(void *, void *pret);
    void *args;
    size_t szValue;
} ArgsAsync;

static int delegateAsync(void *args) {
    ArgsAsync arg = *(ArgsAsync *)args;
    free(args);
    void *buffer = malloc(arg.szValue);
    arg.foo(arg.args, buffer);
    futureSet(arg._future, buffer);
    free(buffer);
    return 0;
}

int async(Future *_future, void (*foo)(void *, void *pret), void *args,
          size_t szValue) {
    thrd_t thrd;
    ArgsAsync *arg = (ArgsAsync *)malloc(sizeof(ArgsAsync));
    arg->args = args;
    arg->foo = foo;
    arg->szValue = szValue;
    arg->_future = _future;
    int ret = thrd_create(&thrd, delegateAsync, arg);
    if (ret != thrd_success) {
        free(arg);
        return ret;
    }
    thrd_detach(thrd);
    return 0;
}
