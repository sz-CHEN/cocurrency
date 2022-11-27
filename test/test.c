#include <cfuture.h>
#include <cstdthreads.h>
#include <cstdtime.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    Promise *promise;
    int *arr;
    int szArr;
} ArgsPromise;

int accumulate(ArgsPromise *args) {
    int sum = 0;
    int i;
    for (i = 0; i < args->szArr; ++i) {
        sum += args->arr[i];
    }
    promiseSetValue(args->promise, &sum);
    return 0;
}

int do_work(Promise *args) {
    struct timespec ts;
    ts.tv_sec = 2;
    ts.tv_nsec = 0;
    thrd_sleep(&ts, NULL);
    promiseSetValue(args, NULL);
    return 0;
}

typedef struct {
    Future ____;
    int value;
} IntFuture;

void intFutureSetValue(IntFuture *_future, const int *pvalue) {
    _future->value = *pvalue;
}

void intFutureGetValue(IntFuture *_future, int *pvalue) {
    *pvalue = _future->value;
}

int testPromise() {
    IntFuture intFuture;
    futureInit(&intFuture, intFutureGetValue, intFutureSetValue);
    Promise promise;
    promiseBind(&promise, &intFuture);
    Future *future = getFuture(&promise);
    int arr[] = {1, 2, 3, 4, 5, 6};
    thrd_t thr;
    ArgsPromise args;
    args.promise = &promise;
    args.arr = arr;
    args.szArr = sizeof(arr) / sizeof(int);
    thrd_create(&thr, accumulate, &args);
    futureWait(future);
    int result;
    futureGet(future, &result);
    printf("result=%d\n", result);
    thrd_join(thr, NULL);
    futureDestroy(&intFuture);

    Future voidFuture;
    futureInit(&voidFuture, NULL, NULL);
    Promise barrier;
    promiseBind(&barrier, &voidFuture);
    Future *barrier_future = getFuture(&barrier);
    thrd_create(&thr, do_work, &barrier);
    futureWait(barrier_future);
    thrd_join(thr, NULL);
    futureDestroy(&voidFuture);
    return 0;
}

typedef struct {
    PackagedTask ____;
    int valueBuffer;
} IntPackagedTask;

int *intPackTaskBufferGet(IntPackagedTask *task) { return &task->valueBuffer; }

typedef struct {
    int x;
    int y;
} XY;

void f(XY *xy, int *pret) { *pret = pow(xy->x, xy->y); }

int testPackagedTask() {
    IntPackagedTask task;
    Promise promise;
    IntFuture future;
    futureInit(&future, intFutureGetValue, intFutureSetValue);
    promiseBind(&promise, &future);
    packagedTaskBind(&task, &promise, f, &task.valueBuffer);
    Future *resFuture = getFuture(task.____._promise);
    int result;
    XY xy;
    xy.x = 2;
    xy.y = 9;
    packagedTaskCall(&task, &xy);
    futureGet(resFuture, &result);
    printf("%d\n", result);
    packagedTaskReset(&task);
    xy.x = 3;
    xy.y = 9;
    packagedTaskCall(&task, &xy);
    futureGet(resFuture, &result);
    printf("%d\n", result);
    futureDestroy(&future);
    return 0;
}

typedef struct {
    int *arr;
    int beg;
    int end;
} ArgAsyncSum;

int parallel_sum(int *arr, int beg, int end);
void async_parallel_sum(ArgAsyncSum *arg, int *pret) {
    *pret = parallel_sum(arg->arr, arg->beg, arg->end);
    printf("%u, [%d, %d), %d\n", thrd_current(), arg->beg, arg->end, *pret);
}

int parallel_sum(int *arr, int beg, int end) {
    int len = end - beg;
    if (len <= 1000) {
        int sum = 0;
        int i;
        for (i = beg; i < end; ++i) {
            sum += arr[i];
        }
        return sum;
    }
    int mid = beg + len / 2;
    IntFuture future;
    futureInit(&future, intFutureGetValue, intFutureSetValue);
    ArgAsyncSum args;
    args.arr = arr;
    args.beg = mid;
    args.end = end;
    async(&future, async_parallel_sum, &args, sizeof(int));
    int sum;
    ArgAsyncSum args1;
    args1.arr = arr;
    args1.beg = beg;
    args1.end = mid;
    async_parallel_sum(&args1, &sum);
    int result;
    futureGet(&future, &result);
    futureDestroy(&future);
    return sum + result;
}

int testAsync() {
    int v[10000];
    int i;
    for (i = 0; i < 10000; ++i) {
        v[i] = 1;
    }
    printf("%d\n", parallel_sum(v, 0, 10000));
    return 0;
}

int main(int argc, char const *argv[]) {
    testPromise();
    testPackagedTask();
    testAsync();
}
