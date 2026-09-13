import { asyncify, asyncSequence, asyncParallel, asyncRetry, delay, withTimeout, asyncTryCatch, asyncFinally, asyncAll, asyncRace, asyncMap, asyncFilter, asyncReduce, asyncChain, asyncChain3, asyncDebounce, asyncThrottle, asyncResolve, asyncReject, asyncThen, asyncCatch, asyncDrain, asyncIsPending, asyncIsFulfilled, asyncIsRejected, asyncGetValue, asyncWrap, asyncSeries } from "art/async";
import { resolvedPromise, rejectedPromise, createPromise, isFulfilled, isRejected } from "art/promise";

function testAsyncify(): number {
  let fn: () => number = function(): number { return 42; };
  let promise: number = asyncify::<number>(fn);
  if (!isFulfilled(promise)) { return 1; }
  return 0;
}

function testAsyncifyExecutes(): number {
  let called: boolean = false;
  let fn: () => number = function(): number {
    called = true;
    return 99;
  };
  let promise: number = asyncify::<number>(fn);
  if (!called) { return 1; }
  return 0;
}

function testDelay(): number {
  let promise: number = delay(1000);
  if (!isFulfilled(promise)) { return 1; }
  if (asyncGetValue(promise) != 1000) { return 2; }
  return 0;
}

function testAsyncResolve(): number {
  let promise: number = asyncResolve::<number>(42);
  if (!isFulfilled(promise)) { return 1; }
  if (asyncGetValue(promise) != 42) { return 2; }
  return 0;
}

function testAsyncReject(): number {
  let promise: number = asyncReject::<number>(99);
  if (!isRejected(promise)) { return 1; }
  return 0;
}

function testAsyncIsPending(): number {
  let p1: number = createPromise(function(resolve: (value: number) => void, reject: (reason: number) => void): void {
  });
  if (!asyncIsPending(p1)) { return 1; }

  let p2: number = resolvedPromise(42);
  if (asyncIsPending(p2)) { return 2; }
  return 0;
}

function testAsyncIsFulfilled(): number {
  let p1: number = resolvedPromise(42);
  if (!asyncIsFulfilled(p1)) { return 1; }

  let p2: number = rejectedPromise(99);
  if (asyncIsFulfilled(p2)) { return 2; }
  return 0;
}

function testAsyncIsRejected(): number {
  let p1: number = rejectedPromise(99);
  if (!asyncIsRejected(p1)) { return 1; }

  let p2: number = resolvedPromise(42);
  if (asyncIsRejected(p2)) { return 2; }
  return 0;
}

function testAsyncGetValue(): number {
  let p1: number = resolvedPromise(42);
  if (asyncGetValue(p1) != 42) { return 1; }

  let p2: number = rejectedPromise(99);
  if (asyncGetValue(p2) != 99) { return 2; }
  return 0;
}

function testAsyncThen(): number {
  let called: boolean = false;
  let value: number = 0;

  let promise: number = asyncThen(resolvedPromise(42), function(v: number): void {
    called = true;
    value = v;
  });

  if (!called) { return 1; }
  if (value != 42) { return 2; }
  return 0;
}

function testAsyncCatch(): number {
  let called: boolean = false;
  let reason: number = 0;

  let promise: number = asyncCatch(rejectedPromise(99), function(r: number): void {
    called = true;
    reason = r;
  });

  if (!called) { return 1; }
  if (reason != 99) { return 2; }
  return 0;
}

function testAsyncAll(): number {
  let p1: number = resolvedPromise(1);
  let p2: number = resolvedPromise(2);
  let p3: number = resolvedPromise(3);

  let promise: number = asyncAll([p1, p2, p3]);
  if (!isFulfilled(promise)) { return 1; }
  return 0;
}

function testAsyncAllWithRejection(): number {
  let p1: number = resolvedPromise(1);
  let p2: number = rejectedPromise(99);
  let p3: number = resolvedPromise(3);

  let promise: number = asyncAll([p1, p2, p3]);
  if (!isRejected(promise)) { return 1; }
  return 0;
}

function testAsyncRace(): number {
  let p1: number = resolvedPromise(42);
  let p2: number = resolvedPromise(99);

  let promise: number = asyncRace([p1, p2]);
  if (!isFulfilled(promise)) { return 1; }
  return 0;
}

function testAsyncRaceWithRejection(): number {
  let p1: number = rejectedPromise(55);
  let p2: number = resolvedPromise(99);

  let promise: number = asyncRace([p1, p2]);
  if (!isRejected(promise)) { return 1; }
  return 0;
}

function testAsyncParallel(): number {
  let operations: (() => number)[] = [
    function(): number { return resolvedPromise(1); },
    function(): number { return resolvedPromise(2); },
    function(): number { return resolvedPromise(3); }
  ];

  let promise: number = asyncParallel(operations);
  if (!isFulfilled(promise)) { return 1; }
  return 0;
}

function testAsyncParallelEmpty(): number {
  let promise: number = asyncParallel([]);
  if (!isFulfilled(promise)) { return 1; }
  return 0;
}

function testAsyncSequence(): number {
  let operations: ((value: number) => number)[] = [
    function(v: number): number { return resolvedPromise(v + 1); },
    function(v: number): number { return resolvedPromise(v + 2); },
    function(v: number): number { return resolvedPromise(v + 3); }
  ];

  let promise: number = asyncSequence(operations);
  if (!isFulfilled(promise)) { return 1; }
  return 0;
}

function testAsyncSequenceEmpty(): number {
  let promise: number = asyncSequence([]);
  if (!isFulfilled(promise)) { return 1; }
  return 0;
}

function testAsyncRetry(): number {
  let attempts: number = 0;
  let operation: () => number = function(): number {
    attempts = attempts + 1;
    return resolvedPromise(42);
  };

  let promise: number = asyncRetry(operation, 3);
  if (!isFulfilled(promise)) { return 1; }
  return 0;
}

function testWithTimeout(): number {
  let promise: number = resolvedPromise(42);
  let result: number = withTimeout(promise, 5000);
  if (!isFulfilled(result)) { return 1; }
  return 0;
}

function testAsyncTryCatch(): number {
  let called: boolean = false;
  let operation: () => number = function(): number {
    return resolvedPromise(42);
  };

  let promise: number = asyncTryCatch(operation, function(reason: number): void {
    called = true;
  });

  return 0;
}

function testAsyncFinally(): number {
  let called: boolean = false;
  let promise: number = resolvedPromise(42);

  let result: number = asyncFinally(promise, function(): void {
    called = true;
  });

  if (!called) { return 1; }
  return 0;
}

function testAsyncFinallyOnRejection(): number {
  let called: boolean = false;
  let promise: number = rejectedPromise(99);

  let result: number = asyncFinally(promise, function(): void {
    called = true;
  });

  if (!called) { return 1; }
  return 0;
}

function testAsyncMap(): number {
  let items: number[] = [1, 2, 3];
  let operation: (item: number) => number = function(item: number): number {
    return resolvedPromise(item * 2);
  };

  let promise: number = asyncMap::<number>(items, operation);
  if (!isFulfilled(promise)) { return 1; }
  return 0;
}

function testAsyncMapEmpty(): number {
  let promise: number = asyncMap::<number>([], function(item: number): number {
    return resolvedPromise(item);
  });
  if (!isFulfilled(promise)) { return 1; }
  return 0;
}

function testAsyncFilter(): number {
  let items: number[] = [1, 2, 3, 4, 5];
  let predicate: (item: number) => number = function(item: number): number {
    if (item - ((item / 2) * 2) == 0) {
      return resolvedPromise(1);
    }
    return resolvedPromise(0);
  };

  let promise: number = asyncFilter::<number>(items, predicate);
  if (!isFulfilled(promise)) { return 1; }
  return 0;
}

function testAsyncFilterEmpty(): number {
  let promise: number = asyncFilter::<number>([], function(item: number): number {
    return resolvedPromise(1);
  });
  if (!isFulfilled(promise)) { return 1; }
  return 0;
}

function testAsyncReduce(): number {
  let items: number[] = [1, 2, 3, 4];
  let reducer: (acc: number, item: number) => number = function(acc: number, item: number): number {
    return acc + item;
  };

  let promise: number = asyncReduce::<number>(items, reducer, 0);
  if (!isFulfilled(promise)) { return 1; }
  let result: number = asyncGetValue(promise);
  if (result != 10) { return 2; }
  return 0;
}

function testAsyncReduceEmpty(): number {
  let reducer: (acc: number, item: number) => number = function(acc: number, item: number): number {
    return acc + item;
  };

  let promise: number = asyncReduce::<number>([], reducer, 42);
  if (!isFulfilled(promise)) { return 1; }
  if (asyncGetValue(promise) != 42) { return 2; }
  return 0;
}

function testAsyncChain(): number {
  let op1: () => number = function(): number { return resolvedPromise(10); };
  let op2: (value: number) => number = function(value: number): number { return resolvedPromise(value + 5); };

  let promise: number = asyncChain(op1, op2);
  if (!isFulfilled(promise)) { return 1; }
  return 0;
}

function testAsyncChain3(): number {
  let op1: () => number = function(): number { return resolvedPromise(10); };
  let op2: (value: number) => number = function(value: number): number { return resolvedPromise(value + 5); };
  let op3: (value: number) => number = function(value: number): number { return resolvedPromise(value + 3); };

  let promise: number = asyncChain3::<number>(op1, op2, op3);
  if (!isFulfilled(promise)) { return 1; }
  return 0;
}

function testAsyncDebounce(): number {
  let callCount: number = 0;
  let operation: () => number = function(): number {
    callCount = callCount + 1;
    return resolvedPromise(42);
  };

  let debounced: () => number = asyncDebounce(operation, 100);
  debounced();

  if (callCount != 1) { return 1; }
  return 0;
}

function testAsyncThrottle(): number {
  let callCount: number = 0;
  let operation: () => number = function(): number {
    callCount = callCount + 1;
    return resolvedPromise(42);
  };

  let throttled: () => number = asyncThrottle(operation, 100);
  throttled();
  throttled();

  if (callCount < 1) { return 1; }
  return 0;
}

function testAsyncWrap(): number {
  let fn: () => number = function(): number { return 42; };
  let wrapped: () => number = asyncWrap::<number>(fn);

  let promise: number = wrapped();
  if (!isFulfilled(promise)) { return 1; }
  return 0;
}

function testAsyncSeries(): number {
  let operations: (() => number)[] = [
    function(): number { return resolvedPromise(1); },
    function(): number { return resolvedPromise(2); },
    function(): number { return resolvedPromise(3); }
  ];

  let promise: number = asyncSeries(operations);
  if (!isFulfilled(promise)) { return 1; }
  return 0;
}

function testAsyncSeriesEmpty(): number {
  let promise: number = asyncSeries([]);
  if (!isFulfilled(promise)) { return 1; }
  return 0;
}

function testAsyncDrain(): number {
  asyncDrain();
  return 0;
}
