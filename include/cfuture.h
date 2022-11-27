#ifndef CFUTURE_H
#define CFUTURE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <cstdthreads.h>

struct timespec;

typedef struct stPromise Promise;
typedef struct stPackagedTask PackagedTask;
typedef struct stFuture Future;

enum FutureStatus { FUTURE_READY = 10001, FUTURE_TIMEOUT, FUTURE_DEFERRED };

enum FutureErrc {
    BROKEN_PROMISE = 1,
    FUTURE_ALREADY_RETRIEVED,
    PROMISE_ALREADY_SATISIFED,
    NO_STATE
};

typedef void (*fooFutureGetValue)(Future *_future, void *pvalue);
typedef void (*fooFutureSetValue)(Future *_future, const void *pvalue);

typedef Future *(*fooGetFuture)(Promise *);
typedef void (*fooPromiseSetValue)(Promise *, const void *);

struct stFuture {
    mtx_t mtx;
    cnd_t cnd;
    int valid;
    int validResult;
    fooFutureGetValue getValue;
    fooFutureSetValue setValue;
    /* T value; */
};

int futureInit(Future *, fooFutureGetValue getValue,
               fooFutureSetValue setValue);
int futureDestroy(Future *);
int futureGet(Future *, void *pvalue);
int futureValid(Future *);
int futureWait(Future *);
int futureWaitUntil(Future *, const struct timespec *abstime);

struct stPromise {
    Future *_future;
};

Future *getFuture(Promise *);
int promiseBind(Promise *, Future *);
int promiseSetValue(Promise *, const void *pvalue);

typedef void *(*fooPackagedTaskBufferGet)(PackagedTask *);

struct stPackagedTask {
    Promise *_promise;
    void (*_foo)(void *, void *pret);
    void *_valueBuffer;
};

int packagedTaskBind(PackagedTask *, Promise *, void (*foo)(void *, void *pret),
                     void *valueBuffer);

int packagedTaskCall(PackagedTask *, void *args);

int packagedTaskReset(PackagedTask *);

int async(Future *, void (*foo)(void *, void *pret), void *args,
          size_t szValue);

#ifdef __cplusplus
}
#endif

#endif
