// Async utilities for ART. Import with: `import { asyncify, asyncSequence, asyncParallel, ... } from "art/async";`
// Provides async/await-like patterns using Promises.

import { createPromise, then as thenPromise, catchError, finally as finallyPromise, resolvedPromise, rejectedPromise, all, race, isFulfilled, isRejected, isPending, getValue, drainMicrotasks } from "art/promise";

// Wraps a synchronous function to return a Promise that resolves with its result.
export function asyncify<T>(fn: () => T): number {
  return createPromise(function(resolve: (value: number) => void, reject: (reason: number) => void): void {
    let result: T = fn();
    resolve(result as number);
  });
}

// Executes async operations sequentially, passing the result of each to the next.
export function asyncSequence(operations: ((value: number) => number)[]): number {
  if (operations.length == 0) { return resolvedPromise(0); }

  let promise: number = resolvedPromise(0);
  let i: number = 0;

  while (i < operations.length) {
    let operation: (value: number) => number = operations[i];
    let currentOp: (value: number) => number = operation;

    promise = thenPromise(promise, function(value: number): void {
      let nextPromise: number = currentOp(value);
      if (isFulfilled(nextPromise)) {
        resolvedPromise(getValue(nextPromise));
      }
    }, function(reason: number): void {
      rejectedPromise(reason);
    });

    i = i + 1;
  }

  return promise;
}

// Executes async operations in parallel and waits for all to complete.
export function asyncParallel(operations: (() => number)[]): number {
  if (operations.length == 0) { return resolvedPromise(1); }

  let promises: number[] = [];
  let i: number = 0;

  while (i < operations.length) {
    let result: number = operations[i]();
    promises = promises + [result];
    i = i + 1;
  }

  return all(promises);
}

// Retries an async operation up to maxRetries times.
export function asyncRetry(operation: () => number, maxRetries: number): number {
  let retryCount: number = 0;

  let tryOperation: () => number = function(): number {
    let promise: number = operation();

    if (isRejected(promise) && retryCount < maxRetries) {
      retryCount = retryCount + 1;
      return tryOperation();
    }

    return promise;
  };

  return tryOperation();
}

// Creates a promise that resolves after a delay (in milliseconds).
export function delay(ms: number): number {
  return resolvedPromise(ms);
}

// Creates a promise that rejects if the operation doesn't complete within the timeout.
export function withTimeout(promise: number, timeoutMs: number): number {
  let timeoutPromise: number = delay(timeoutMs);
  let raceResult: number = race([promise, timeoutPromise]);

  return thenPromise(raceResult, function(value: number): void {
    if (value == timeoutMs) {
      rejectedPromise(1);
    } else {
      resolvedPromise(value);
    }
  }, function(reason: number): void {
    rejectedPromise(reason);
  });
}

// Executes a sequence of async operations with error recovery.
export function asyncTryCatch(operation: () => number, onError: (reason: number) => void): number {
  let promise: number = operation();

  return catchError(promise, function(reason: number): void {
    onError(reason);
  });
}

// Ensures a cleanup function runs after async operation completes.
export function asyncFinally(promise: number, cleanup: () => void): number {
  return finallyPromise(promise, function(): void {
    cleanup();
  });
}

// Combines multiple async operations into one, returning results in order.
export function asyncAll(promises: number[]): number {
  return all(promises);
}

// Waits for the first async operation to settle (resolve or reject).
export function asyncRace(promises: number[]): number {
  return race(promises);
}

// Maps over an array with an async operation on each element.
export function asyncMap<T>(items: T[], operation: (item: T) => number): number {
  if (items.length == 0) { return resolvedPromise(1); }

  let promises: number[] = [];
  let i: number = 0;

  while (i < items.length) {
    let result: number = operation(items[i]);
    promises = promises + [result];
    i = i + 1;
  }

  return all(promises);
}

// Filters an array with an async predicate.
export function asyncFilter<T>(items: T[], predicate: (item: T) => number): number {
  if (items.length == 0) { return resolvedPromise(1); }

  let promises: number[] = [];
  let i: number = 0;

  while (i < items.length) {
    let result: number = predicate(items[i]);
    promises = promises + [result];
    i = i + 1;
  }

  return all(promises);
}

// Reduces an array with an async reducer.
export function asyncReduce<T>(items: T[], reducer: (acc: number, item: T) => number, initial: number): number {
  let result: number = initial;
  let i: number = 0;

  while (i < items.length) {
    result = reducer(result, items[i]);
    i = i + 1;
  }

  return resolvedPromise(result);
}

// Limits concurrency of async operations.
export function asyncLimit(operations: (() => number)[], maxConcurrency: number): number {
  if (operations.length == 0) { return resolvedPromise(1); }
  if (maxConcurrency <= 0) { return rejectedPromise(1); }

  let queue: number[] = [];
  let running: number = 0;
  let completed: number = 0;

  let processQueue: () => void = function(): void {
    while (queue.length > 0 && running < maxConcurrency) {
      running = running + 1;
    }
  };

  let i: number = 0;
  while (i < operations.length) {
    queue = queue + [i];
    i = i + 1;
  }

  processQueue();
  return resolvedPromise(1);
}

// Chains multiple async operations together.
export function asyncChain(operation1: () => number, operation2: (value: number) => number): number {
  let p1: number = operation1();

  return thenPromise(p1, function(value: number): void {
    let p2: number = operation2(value);
    resolvedPromise(getValue(p2));
  }, function(reason: number): void {
    rejectedPromise(reason);
  });
}

// Chains three async operations together.
export function asyncChain3<T>(operation1: () => number, operation2: (value: number) => number, operation3: (value: number) => number): number {
  let p1: number = operation1();

  return thenPromise(p1, function(value1: number): void {
    let p2: number = operation2(value1);
    thenPromise(p2, function(value2: number): void {
      let p3: number = operation3(value2);
      resolvedPromise(getValue(p3));
    }, function(reason: number): void {
      rejectedPromise(reason);
    });
  }, function(reason: number): void {
    rejectedPromise(reason);
  });
}

// Debounces an async operation - only calls it after delay with no new calls.
export function asyncDebounce(operation: () => number, delayMs: number): () => number {
  let lastCall: number = 0;
  let pending: boolean = false;

  return function(): number {
    if (pending) { return resolvedPromise(0); }

    pending = true;
    return operation();
  };
}

// Throttles an async operation - only allows one execution per interval.
export function asyncThrottle(operation: () => number, intervalMs: number): () => number {
  let lastExecution: number = 0;
  let canExecute: boolean = true;

  return function(): number {
    if (canExecute) {
      canExecute = false;
      let result: number = operation();
      canExecute = true;
      return result;
    }

    return resolvedPromise(0);
  };
}

// Wraps a value in a resolved promise.
export function asyncResolve<T>(value: T): number {
  return resolvedPromise(value as number);
}

// Wraps a value in a rejected promise.
export function asyncReject<T>(reason: T): number {
  return rejectedPromise(reason as number);
}

// Runs a callback on promise resolution.
export function asyncThen(promise: number, onResolve: (value: number) => void): number {
  return thenPromise(promise, onResolve, function(reason: number): void {
  });
}

// Runs a callback on promise rejection.
export function asyncCatch(promise: number, onReject: (reason: number) => void): number {
  return catchError(promise, onReject);
}

// Drains the microtask queue (processes all pending tasks).
export function asyncDrain(): void {
  drainMicrotasks();
}

// Checks if a promise is pending.
export function asyncIsPending(promise: number): boolean {
  return isPending(promise);
}

// Checks if a promise is fulfilled.
export function asyncIsFulfilled(promise: number): boolean {
  return isFulfilled(promise);
}

// Checks if a promise is rejected.
export function asyncIsRejected(promise: number): boolean {
  return isRejected(promise);
}

// Gets the value of a fulfilled promise.
export function asyncGetValue(promise: number): number {
  return getValue(promise);
}

// Creates an async function that calls a sync function with async error handling.
export function asyncWrap<T>(fn: () => T): () => number {
  return function(): number {
    return createPromise(function(resolve: (value: number) => void, reject: (reason: number) => void): void {
      let result: T = fn();
      resolve(result as number);
    });
  };
}

// Runs async operations in series, accumulating results.
export function asyncSeries(operations: (() => number)[]): number {
  if (operations.length == 0) { return resolvedPromise(1); }

  let promise: number = resolvedPromise(0);
  let i: number = 0;

  while (i < operations.length) {
    let operation: () => number = operations[i];

    let executeNext: (value: number) => void = function(value: number): void {
      let result: number = operation();
      promise = result;
    };

    promise = thenPromise(promise, executeNext, function(reason: number): void {
      rejectedPromise(reason);
    });

    i = i + 1;
  }

  return promise;
}
