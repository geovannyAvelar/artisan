import { createPromise, resolvePromise, rejectPromise, then, catchError, finally, getState, getValue, isFulfilled, isRejected, isPending, resolvedPromise, rejectedPromise, all, race, allSettled, drainMicrotasks, clearAllPromises, STATE_PENDING, STATE_FULFILLED, STATE_REJECTED, promiseToString } from "art/promise";

function testCreatePromiseResolve(): number {
  let resolved: boolean = false;
  let promiseId: number = createPromise(function(resolve: (value: number) => void, reject: (reason: number) => void): void {
    resolve(42);
    resolved = true;
  });

  if (!resolved) { return 1; }
  if (getValue(promiseId) != 42) { return 2; }
  if (!isFulfilled(promiseId)) { return 3; }
  return 0;
}

function testCreatePromiseReject(): number {
  let rejected: boolean = false;
  let promiseId: number = createPromise(function(resolve: (value: number) => void, reject: (reason: number) => void): void {
    reject(99);
    rejected = true;
  });

  if (!rejected) { return 1; }
  if (getValue(promiseId) != 99) { return 2; }
  if (!isRejected(promiseId)) { return 3; }
  return 0;
}

function testResolvePromise(): number {
  let promiseId: number = createPromise(function(resolve: (value: number) => void, reject: (reason: number) => void): void {
  });

  if (!isPending(promiseId)) { return 1; }
  resolvePromise(promiseId, 123);
  if (!isFulfilled(promiseId)) { return 2; }
  if (getValue(promiseId) != 123) { return 3; }
  return 0;
}

function testRejectPromise(): number {
  let promiseId: number = createPromise(function(resolve: (value: number) => void, reject: (reason: number) => void): void {
  });

  if (!isPending(promiseId)) { return 1; }
  rejectPromise(promiseId, 456);
  if (!isRejected(promiseId)) { return 2; }
  if (getValue(promiseId) != 456) { return 3; }
  return 0;
}

function testResolvedPromise(): number {
  let promiseId: number = resolvedPromise(789);

  if (!isFulfilled(promiseId)) { return 1; }
  if (getValue(promiseId) != 789) { return 2; }
  return 0;
}

function testRejectedPromise(): number {
  let promiseId: number = rejectedPromise(555);

  if (!isRejected(promiseId)) { return 1; }
  if (getValue(promiseId) != 555) { return 2; }
  return 0;
}

function testThenFulfilled(): number {
  let handlerCalled: boolean = false;
  let handlerValue: number = 0;

  let promiseId: number = resolvedPromise(42);
  let newPromiseId: number = then(promiseId, function(value: number): void {
    handlerCalled = true;
    handlerValue = value;
  }, function(reason: number): void {
  });

  if (!handlerCalled) { return 1; }
  if (handlerValue != 42) { return 2; }
  return 0;
}

function testThenRejected(): number {
  let rejectionHandlerCalled: boolean = false;
  let rejectionReason: number = 0;

  let promiseId: number = rejectedPromise(99);
  let newPromiseId: number = then(promiseId, function(value: number): void {
  }, function(reason: number): void {
    rejectionHandlerCalled = true;
    rejectionReason = reason;
  });

  if (!rejectionHandlerCalled) { return 1; }
  if (rejectionReason != 99) { return 2; }
  return 0;
}

function testCatchError(): number {
  let catchHandlerCalled: boolean = false;
  let caughtReason: number = 0;

  let promiseId: number = rejectedPromise(777);
  let newPromiseId: number = catchError(promiseId, function(reason: number): void {
    catchHandlerCalled = true;
    caughtReason = reason;
  });

  if (!catchHandlerCalled) { return 1; }
  if (caughtReason != 777) { return 2; }
  return 0;
}

function testCatchErrorDoesNotCallOnFulfilled(): number {
  let catchHandlerCalled: boolean = false;

  let promiseId: number = resolvedPromise(42);
  let newPromiseId: number = catchError(promiseId, function(reason: number): void {
    catchHandlerCalled = true;
  });

  if (catchHandlerCalled) { return 1; }
  return 0;
}

function testFinally(): number {
  let finallyHandlerCalled: boolean = false;

  let promiseId: number = resolvedPromise(42);
  let newPromiseId: number = finally(promiseId, function(): void {
    finallyHandlerCalled = true;
  });

  if (!finallyHandlerCalled) { return 1; }
  return 0;
}

function testFinallyOnRejection(): number {
  let finallyHandlerCalled: boolean = false;

  let promiseId: number = rejectedPromise(99);
  let newPromiseId: number = finally(promiseId, function(): void {
    finallyHandlerCalled = true;
  });

  if (!finallyHandlerCalled) { return 1; }
  return 0;
}

function testPromiseAll(): number {
  let p1: number = resolvedPromise(1);
  let p2: number = resolvedPromise(2);
  let p3: number = resolvedPromise(3);

  let allPromise: number = all([p1, p2, p3]);
  if (!isFulfilled(allPromise)) { return 1; }
  return 0;
}

function testPromiseAllWithRejection(): number {
  let p1: number = resolvedPromise(1);
  let p2: number = rejectedPromise(99);
  let p3: number = resolvedPromise(3);

  let allPromise: number = all([p1, p2, p3]);
  if (!isRejected(allPromise)) { return 1; }
  if (getValue(allPromise) != 99) { return 2; }
  return 0;
}

function testPromiseAllEmpty(): number {
  let allPromise: number = all([]);
  if (!isFulfilled(allPromise)) { return 1; }
  return 0;
}

function testPromiseRace(): number {
  let p1: number = resolvedPromise(42);
  let p2: number = resolvedPromise(99);

  let racePromise: number = race([p1, p2]);
  if (!isFulfilled(racePromise)) { return 1; }
  if (getValue(racePromise) != 42) { return 2; }
  return 0;
}

function testPromiseRaceWithRejection(): number {
  let p1: number = rejectedPromise(55);
  let p2: number = resolvedPromise(99);

  let racePromise: number = race([p1, p2]);
  if (!isRejected(racePromise)) { return 1; }
  if (getValue(racePromise) != 55) { return 2; }
  return 0;
}

function testPromiseRaceEmpty(): number {
  let racePromise: number = race([]);
  if (!isFulfilled(racePromise)) { return 1; }
  return 0;
}

function testPromiseAllSettled(): number {
  let p1: number = resolvedPromise(1);
  let p2: number = rejectedPromise(99);
  let p3: number = resolvedPromise(3);

  let settledPromise: number = allSettled([p1, p2, p3]);
  if (!isFulfilled(settledPromise)) { return 1; }
  return 0;
}

function testPromiseAllSettledEmpty(): number {
  let settledPromise: number = allSettled([]);
  if (!isFulfilled(settledPromise)) { return 1; }
  return 0;
}

function testGetState(): number {
  let p1: number = resolvedPromise(42);
  let p2: number = rejectedPromise(99);
  let p3: number = createPromise(function(resolve: (value: number) => void, reject: (reason: number) => void): void {
  });

  if (getState(p1) != STATE_FULFILLED) { return 1; }
  if (getState(p2) != STATE_REJECTED) { return 2; }
  if (getState(p3) != STATE_PENDING) { return 3; }
  return 0;
}

function testIsFulfilled(): number {
  let p1: number = resolvedPromise(42);
  if (!isFulfilled(p1)) { return 1; }

  let p2: number = rejectedPromise(99);
  if (isFulfilled(p2)) { return 2; }

  let p3: number = createPromise(function(resolve: (value: number) => void, reject: (reason: number) => void): void {
  });
  if (isFulfilled(p3)) { return 3; }
  return 0;
}

function testIsRejected(): number {
  let p1: number = resolvedPromise(42);
  if (isRejected(p1)) { return 1; }

  let p2: number = rejectedPromise(99);
  if (!isRejected(p2)) { return 2; }

  let p3: number = createPromise(function(resolve: (value: number) => void, reject: (reason: number) => void): void {
  });
  if (isRejected(p3)) { return 3; }
  return 0;
}

function testIsPending(): number {
  let p1: number = resolvedPromise(42);
  if (isPending(p1)) { return 1; }

  let p2: number = rejectedPromise(99);
  if (isPending(p2)) { return 2; }

  let p3: number = createPromise(function(resolve: (value: number) => void, reject: (reason: number) => void): void {
  });
  if (!isPending(p3)) { return 3; }
  return 0;
}

function testMultipleHandlers(): number {
  let count: number = 0;
  let promiseId: number = resolvedPromise(42);

  let h1: number = then(promiseId, function(value: number): void {
    count = count + 1;
  }, function(reason: number): void {
  });

  let h2: number = then(promiseId, function(value: number): void {
    count = count + 1;
  }, function(reason: number): void {
  });

  if (count != 2) { return 1; }
  return 0;
}

function testPromiseChaining(): number {
  let values: number[] = [];

  let p1: number = resolvedPromise(10);
  let p2: number = then(p1, function(value: number): void {
    values = values + [value];
    resolvePromise(p2, value + 5);
  }, function(reason: number): void {
  });

  let p3: number = then(p2, function(value: number): void {
    values = values + [value];
  }, function(reason: number): void {
  });

  if (values.length != 2) { return 1; }
  if (values[0] != 10) { return 2; }
  return 0;
}

function testPromiseToString(): number {
  let p1: number = resolvedPromise(42);
  let str1: string = promiseToString(p1);
  if (str1.length == 0) { return 1; }
  if (str1.indexOf("Fulfilled") == -1) { return 2; }

  let p2: number = rejectedPromise(99);
  let str2: string = promiseToString(p2);
  if (str2.indexOf("Rejected") == -1) { return 3; }

  let p3: number = createPromise(function(resolve: (value: number) => void, reject: (reason: number) => void): void {
  });
  let str3: string = promiseToString(p3);
  if (str3.indexOf("Pending") == -1) { return 4; }
  return 0;
}

function testClearAllPromises(): number {
  let p1: number = resolvedPromise(42);
  let p2: number = rejectedPromise(99);

  clearAllPromises();

  let p3: number = resolvedPromise(123);
  if (p3 != 0) { return 1; }
  return 0;
}

function testPendingPromiseInvalidId(): number {
  let state: number = getState(-1);
  if (state != STATE_PENDING) { return 1; }

  let value: number = getValue(-1);
  if (value != 0) { return 2; }
  return 0;
}

function testPromiseIdOutOfBounds(): number {
  let state: number = getState(9999);
  if (state != STATE_PENDING) { return 1; }

  let value: number = getValue(9999);
  if (value != 0) { return 2; }
  return 0;
}
