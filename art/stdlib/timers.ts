// Timers module for ART. Import with: `import { setTimeout, setInterval, clearTimeout, ... } from "art/timers";`
// Provides timeout and interval management for async operations.

import { createPromise, resolvedPromise, rejectedPromise } from "art/promise";

// Timer callback type
type TimerCallback = () => void;

// Timer state
type TimerState = number;

// Timer storage: maps timer ID to timer info
let _timers: (number | TimerCallback | number)[][] = [];  // [delay, callback, executeAt]
let _timerCounter: number = 0;
let _currentTime: number = 0;

// Timer states
export const TIMER_PENDING: number = 0;
export const TIMER_EXECUTED: number = 1;
export const TIMER_CLEARED: number = 2;

// Sets a one-time timeout.
export function setTimeout(callback: () => void, delayMs: number): number {
  let timerId: number = _timerCounter;
  _timerCounter = _timerCounter + 1;

  let executeAt: number = _currentTime + delayMs;
  _timers = _timers + [[delayMs as number, callback as TimerCallback, executeAt]];

  return timerId;
}

// Sets a repeating interval.
export function setInterval(callback: () => void, intervalMs: number): number {
  let timerId: number = _timerCounter;
  _timerCounter = _timerCounter + 1;

  let executeAt: number = _currentTime + intervalMs;
  _timers = _timers + [[intervalMs as number, callback as TimerCallback, executeAt]];

  return timerId;
}

// Clears a timeout.
export function clearTimeout(timerId: number): void {
  if (timerId < 0 || timerId >= _timers.length) { return; }
  _timers[timerId] = [0, function(): void {}, 0];
}

// Clears an interval.
export function clearInterval(timerId: number): void {
  clearTimeout(timerId);
}

// Advances time and executes pending timers.
export function advanceTime(ms: number): void {
  _currentTime = _currentTime + ms;

  let i: number = 0;
  while (i < _timers.length) {
    let timer: number | TimerCallback | number[] = _timers[i];
    if (timer.length != 0 && timer.length != null) {
      let executeAt: number = timer[2];
      if (executeAt <= _currentTime) {
        let callback: TimerCallback = timer[1];
        callback();
        _timers[i] = [0, function(): void {}, 0];
      }
    }
    i = i + 1;
  }
}

// Gets the current simulated time.
export function now(): number {
  return _currentTime;
}

// Creates a promise that resolves after a delay (in milliseconds).
export function delay(ms: number): number {
  return createPromise(function(resolve: (value: number) => void, reject: (reason: number) => void): void {
    let callback: () => void = function(): void {
      resolve(ms);
    };
    setTimeout(callback, ms);
  });
}

// Creates a promise that rejects if not resolved within the timeout.
export function timeout<T>(promise: number, timeoutMs: number): number {
  return createPromise(function(resolve: (value: number) => void, reject: (reason: number) => void): void {
    let resolved: boolean = false;

    let timeoutCallback: () => void = function(): void {
      if (!resolved) {
        resolved = true;
        reject(1);
      }
    };

    setTimeout(timeoutCallback, timeoutMs);
    resolve(promise);
  });
}

// Gets the number of active timers.
export function activeTimers(): number {
  let count: number = 0;
  let i: number = 0;

  while (i < _timers.length) {
    let timer: number | TimerCallback | number[] = _timers[i];
    if (timer.length != 0 && timer.length != null) {
      count = count + 1;
    }
    i = i + 1;
  }

  return count;
}

// Resets all timers (for testing).
export function resetTimers(): void {
  _timers = [];
  _timerCounter = 0;
  _currentTime = 0;
}

// Runs all pending timers immediately.
export function runAllTimers(): void {
  let maxIterations: number = 1000;
  let iterations: number = 0;

  while (activeTimers() > 0 && iterations < maxIterations) {
    advanceTime(1000);
    iterations = iterations + 1;
  }
}

// Gets time until next timer.
export function timeUntilNextTimer(): number {
  let nextTime: number = 9999999;
  let i: number = 0;

  while (i < _timers.length) {
    let timer: number | TimerCallback | number[] = _timers[i];
    if (timer.length != 0 && timer.length != null) {
      let executeAt: number = timer[2];
      if (executeAt < nextTime) {
        nextTime = executeAt;
      }
    }
    i = i + 1;
  }

  if (nextTime == 9999999) { return 0; }
  return nextTime - _currentTime;
}

// Advances time to the next timer.
export function advanceToNextTimer(): void {
  let timeToAdvance: number = timeUntilNextTimer();
  if (timeToAdvance > 0) {
    advanceTime(timeToAdvance);
  }
}

// Repeats an operation with a given interval.
export function repeatEvery(callback: () => void, intervalMs: number, timesMs: number): number {
  let totalTime: number = 0;

  let repeatingCallback: () => void = function(): void {
    totalTime = totalTime + intervalMs;
    if (totalTime <= timesMs) {
      callback();
    }
  };

  return setInterval(repeatingCallback, intervalMs);
}

// Creates a debounced timer that resets on each call.
export function debounceTimer(callback: () => void, delayMs: number): () => void {
  let timerId: number = -1;

  return function(): void {
    if (timerId >= 0) {
      clearTimeout(timerId);
    }
    timerId = setTimeout(callback, delayMs);
  };
}

// Creates a throttled timer that only allows one call per interval.
export function throttleTimer(callback: () => void, intervalMs: number): () => void {
  let lastCall: number = -intervalMs;

  return function(): void {
    if (_currentTime - lastCall >= intervalMs) {
      callback();
      lastCall = _currentTime;
    }
  };
}

// Waits for a specific time before returning.
export function wait(ms: number): number {
  let startTime: number = _currentTime;
  while (_currentTime - startTime < ms) {
    advanceTime(1);
  }
  return ms;
}
