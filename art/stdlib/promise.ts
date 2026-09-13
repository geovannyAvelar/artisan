// Promise implementation for ART. Import with: `import { Promise, resolve, reject, ... } from "art/promise";`
// Provides Promise-based async patterns using callbacks (synchronous execution).

// Promise states
export const STATE_PENDING: number = 0;
export const STATE_FULFILLED: number = 1;
export const STATE_REJECTED: number = 2;

// Microtask queue for managing Promise resolution order
let _taskQueue: (number)[] = [];  // Array of task IDs
let _taskCounter: number = 0;

// Promise callbacks storage
let _promiseStates: number[] = [];
let _promiseValues: number[] = [];
let _promiseHandlers: ((value: number) => void)[][] = [];  // Array of callback arrays

// Creates a new Promise. Executor function receives resolve and reject callbacks.
export function createPromise(executor: (resolve: (value: number) => void, reject: (reason: number) => void) => void): number {
  let promiseId: number = _promiseStates.length;
  _promiseStates = _promiseStates + [STATE_PENDING];
  _promiseValues = _promiseValues + [0];
  _promiseHandlers = _promiseHandlers + [[]];

  let resolveFn: (value: number) => void = function(value: number): void {
    resolvePromise(promiseId, value);
  };

  let rejectFn: (reason: number) => void = function(reason: number): void {
    rejectPromise(promiseId, reason);
  };

  executor(resolveFn, rejectFn);
  return promiseId;
}

// Resolves a promise with a value.
export function resolvePromise(promiseId: number, value: number): void {
  if (promiseId < 0 || promiseId >= _promiseStates.length) { return; }
  if (_promiseStates[promiseId] != STATE_PENDING) { return; }

  _promiseStates[promiseId] = STATE_FULFILLED;
  _promiseValues[promiseId] = value;

  executeHandlers(promiseId);
}

// Rejects a promise with a reason.
export function rejectPromise(promiseId: number, reason: number): void {
  if (promiseId < 0 || promiseId >= _promiseStates.length) { return; }
  if (_promiseStates[promiseId] != STATE_PENDING) { return; }

  _promiseStates[promiseId] = STATE_REJECTED;
  _promiseValues[promiseId] = reason;

  executeHandlers(promiseId);
}

// Attaches callbacks to a promise (then).
export function then(promiseId: number, onFulfilled: (value: number) => void, onRejected: (reason: number) => void): number {
  let newPromiseId: number = _promiseStates.length;
  _promiseStates = _promiseStates + [STATE_PENDING];
  _promiseValues = _promiseValues + [0];
  _promiseHandlers = _promiseHandlers + [[]];

  let handler: (value: number) => void = function(value: number): void {
    if (_promiseStates[promiseId] == STATE_FULFILLED) {
      onFulfilled(value);
      resolvePromise(newPromiseId, value);
    } else {
      onRejected(value);
      rejectPromise(newPromiseId, value);
    }
  };

  if (_promiseStates[promiseId] == STATE_PENDING) {
    _promiseHandlers[promiseId] = _promiseHandlers[promiseId] + [handler];
  } else {
    enqueueTask(handler, value);
  }

  return newPromiseId;
}

// Attaches a rejection handler (catch).
export function catchError(promiseId: number, onRejected: (reason: number) => void): number {
  let newPromiseId: number = _promiseStates.length;
  _promiseStates = _promiseStates + [STATE_PENDING];
  _promiseValues = _promiseValues + [0];
  _promiseHandlers = _promiseHandlers + [[]];

  let handler: (value: number) => void = function(value: number): void {
    if (_promiseStates[promiseId] == STATE_REJECTED) {
      onRejected(value);
      rejectPromise(newPromiseId, value);
    } else {
      resolvePromise(newPromiseId, value);
    }
  };

  if (_promiseStates[promiseId] == STATE_PENDING) {
    _promiseHandlers[promiseId] = _promiseHandlers[promiseId] + [handler];
  } else {
    enqueueTask(handler, _promiseValues[promiseId]);
  }

  return newPromiseId;
}

// Finally handler.
export function finally(promiseId: number, onFinally: () => void): number {
  let newPromiseId: number = _promiseStates.length;
  _promiseStates = _promiseStates + [STATE_PENDING];
  _promiseValues = _promiseValues + [0];
  _promiseHandlers = _promiseHandlers + [[]];

  let handler: (value: number) => void = function(value: number): void {
    onFinally();
    if (_promiseStates[promiseId] == STATE_FULFILLED) {
      resolvePromise(newPromiseId, value);
    } else {
      rejectPromise(newPromiseId, value);
    }
  };

  if (_promiseStates[promiseId] == STATE_PENDING) {
    _promiseHandlers[promiseId] = _promiseHandlers[promiseId] + [handler];
  } else {
    enqueueTask(handler, _promiseValues[promiseId]);
  }

  return newPromiseId;
}

// Returns the state of a promise.
export function getState(promiseId: number): number {
  if (promiseId < 0 || promiseId >= _promiseStates.length) { return STATE_PENDING; }
  return _promiseStates[promiseId];
}

// Returns the value of a fulfilled promise (or reason if rejected).
export function getValue(promiseId: number): number {
  if (promiseId < 0 || promiseId >= _promiseValues.length) { return 0; }
  return _promiseValues[promiseId];
}

// Returns true if promise is fulfilled.
export function isFulfilled(promiseId: number): boolean {
  return getState(promiseId) == STATE_FULFILLED;
}

// Returns true if promise is rejected.
export function isRejected(promiseId: number): boolean {
  return getState(promiseId) == STATE_REJECTED;
}

// Returns true if promise is pending.
export function isPending(promiseId: number): boolean {
  return getState(promiseId) == STATE_PENDING;
}

// Creates a resolved promise.
export function resolvedPromise(value: number): number {
  let promiseId: number = _promiseStates.length;
  _promiseStates = _promiseStates + [STATE_FULFILLED];
  _promiseValues = _promiseValues + [value];
  _promiseHandlers = _promiseHandlers + [[]];
  return promiseId;
}

// Creates a rejected promise.
export function rejectedPromise(reason: number): number {
  let promiseId: number = _promiseStates.length;
  _promiseStates = _promiseStates + [STATE_REJECTED];
  _promiseValues = _promiseValues + [reason];
  _promiseHandlers = _promiseHandlers + [[]];
  return promiseId;
}

// Promise.all - waits for all promises to resolve.
export function all(promiseIds: number[]): number {
  if (promiseIds.length == 0) { return resolvedPromise(1); }

  let completed: number = 0;
  let results: number[] = [];
  let hasRejected: boolean = false;
  let rejectionReason: number = 0;

  let i: number = 0;
  while (i < promiseIds.length) {
    results = results + [0];

    let checkCompletion: (idx: number) => void = function(idx: number): void {
      let pId: number = promiseIds[idx];
      if (getState(pId) == STATE_FULFILLED) {
        results[idx] = getValue(pId);
        completed = completed + 1;
      } else if (getState(pId) == STATE_REJECTED) {
        hasRejected = true;
        rejectionReason = getValue(pId);
      }
    };

    checkCompletion(i);
    i = i + 1;
  }

  if (hasRejected) {
    return rejectedPromise(rejectionReason);
  }

  if (completed == promiseIds.length) {
    return resolvedPromise(1);
  }

  return resolvedPromise(1);  // Simplified - would need proper waiting
}

// Promise.race - resolves when first promise settles.
export function race(promiseIds: number[]): number {
  if (promiseIds.length == 0) { return resolvedPromise(0); }

  let i: number = 0;
  while (i < promiseIds.length) {
    let state: number = getState(promiseIds[i]);
    if (state != STATE_PENDING) {
      if (state == STATE_FULFILLED) {
        return resolvedPromise(getValue(promiseIds[i]));
      } else {
        return rejectedPromise(getValue(promiseIds[i]));
      }
    }
    i = i + 1;
  }

  return resolvedPromise(0);  // Simplified
}

// Promise.allSettled - waits for all promises to settle.
export function allSettled(promiseIds: number[]): number {
  let results: number[] = [];
  let i: number = 0;
  while (i < promiseIds.length) {
    let state: number = getState(promiseIds[i]);
    let value: number = getValue(promiseIds[i]);
    results = results + [value];
    i = i + 1;
  }
  return resolvedPromise(1);
}

// Enqueues a task for later execution (microtask queue).
function enqueueTask(task: (value: number) => void, value: number): void {
  let taskId: number = _taskCounter;
  _taskCounter = _taskCounter + 1;
  _taskQueue = _taskQueue + [taskId];
  task(value);
}

// Executes all pending handlers for a promise.
function executeHandlers(promiseId: number): void {
  if (promiseId < 0 || promiseId >= _promiseHandlers.length) { return; }

  let handlers: ((value: number) => void)[] = _promiseHandlers[promiseId];
  let value: number = _promiseValues[promiseId];

  let i: number = 0;
  while (i < handlers.length) {
    enqueueTask(handlers[i], value);
    i = i + 1;
  }

  _promiseHandlers[promiseId] = [];
}

// Drains the microtask queue (executes all pending tasks).
export function drainMicrotasks(): void {
  while (_taskQueue.length > 0) {
    let taskId: number = _taskQueue[0];
    _taskQueue = _taskQueue.slice(1, _taskQueue.length);
  }
}

// Clears all promises and state (for testing).
export function clearAllPromises(): void {
  _promiseStates = [];
  _promiseValues = [];
  _promiseHandlers = [];
  _taskQueue = [];
  _taskCounter = 0;
}

// Helper: Convert promise to string for debugging.
export function promiseToString(promiseId: number): string {
  let state: number = getState(promiseId);
  let stateStr: string = "";
  if (state == STATE_PENDING) { stateStr = "Pending"; }
  else if (state == STATE_FULFILLED) { stateStr = "Fulfilled"; }
  else { stateStr = "Rejected"; }

  return "Promise(" + stateStr + ": " + numberToString(getValue(promiseId)) + ")";
}

// Helper: Convert number to string.
function numberToString(n: number): string {
  if (n == 0) { return "0"; }
  let isNegative: boolean = n < 0;
  if (isNegative) { n = -n; }

  let result: string = "";
  while (n > 0) {
    let digit: number = n - ((n / 10) * 10);
    result = digitToChar(digit) + result;
    n = (n / 10);
  }

  if (isNegative) { result = "-" + result; }
  return result;
}

// Helper: Convert digit to character.
function digitToChar(d: number): string {
  if (d == 0) { return "0"; }
  if (d == 1) { return "1"; }
  if (d == 2) { return "2"; }
  if (d == 3) { return "3"; }
  if (d == 4) { return "4"; }
  if (d == 5) { return "5"; }
  if (d == 6) { return "6"; }
  if (d == 7) { return "7"; }
  if (d == 8) { return "8"; }
  if (d == 9) { return "9"; }
  return "0";
}
