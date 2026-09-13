import { setTimeout, setInterval, clearTimeout, clearInterval, advanceTime, now, delay, timeout, activeTimers, resetTimers, runAllTimers, timeUntilNextTimer, advanceToNextTimer, repeatEvery, debounceTimer, throttleTimer, wait, TIMER_PENDING, TIMER_EXECUTED, TIMER_CLEARED } from "art/timers";

function testSetTimeout(): number {
  resetTimers();

  let called: boolean = false;
  let timerId: number = setTimeout(function(): void {
    called = true;
  }, 1000);

  if (timerId < 0) { return 1; }
  if (called) { return 2; }
  return 0;
}

function testSetTimeoutExecute(): number {
  resetTimers();

  let called: boolean = false;
  setTimeout(function(): void {
    called = true;
  }, 1000);

  advanceTime(1000);
  if (!called) { return 1; }
  return 0;
}

function testSetInterval(): number {
  resetTimers();

  let callCount: number = 0;
  let timerId: number = setInterval(function(): void {
    callCount = callCount + 1;
  }, 500);

  if (timerId < 0) { return 1; }
  if (callCount != 0) { return 2; }
  return 0;
}

function testSetIntervalExecute(): number {
  resetTimers();

  let callCount: number = 0;
  setInterval(function(): void {
    callCount = callCount + 1;
  }, 500);

  advanceTime(500);
  if (callCount != 1) { return 1; }

  advanceTime(500);
  if (callCount != 2) { return 2; }
  return 0;
}

function testClearTimeout(): number {
  resetTimers();

  let called: boolean = false;
  let timerId: number = setTimeout(function(): void {
    called = true;
  }, 1000);

  clearTimeout(timerId);
  advanceTime(1000);

  if (called) { return 1; }
  return 0;
}

function testClearInterval(): number {
  resetTimers();

  let callCount: number = 0;
  let timerId: number = setInterval(function(): void {
    callCount = callCount + 1;
  }, 500);

  advanceTime(500);
  clearInterval(timerId);
  advanceTime(500);

  if (callCount != 1) { return 1; }
  return 0;
}

function testNow(): number {
  resetTimers();

  if (now() != 0) { return 1; }
  advanceTime(1000);
  if (now() != 1000) { return 2; }
  return 0;
}

function testAdvanceTime(): number {
  resetTimers();

  if (now() != 0) { return 1; }
  advanceTime(500);
  if (now() != 500) { return 2; }
  advanceTime(300);
  if (now() != 800) { return 3; }
  return 0;
}

function testActiveTimers(): number {
  resetTimers();

  if (activeTimers() != 0) { return 1; }

  setTimeout(function(): void {
  }, 1000);

  if (activeTimers() != 1) { return 2; }

  advanceTime(1000);
  if (activeTimers() != 0) { return 3; }
  return 0;
}

function testActiveTimersMultiple(): number {
  resetTimers();

  setTimeout(function(): void {
  }, 1000);
  setTimeout(function(): void {
  }, 2000);
  setInterval(function(): void {
  }, 500);

  if (activeTimers() != 3) { return 1; }
  return 0;
}

function testTimeUntilNextTimer(): number {
  resetTimers();

  setTimeout(function(): void {
  }, 1500);
  setTimeout(function(): void {
  }, 1000);

  let time: number = timeUntilNextTimer();
  if (time != 1000) { return 1; }
  return 0;
}

function testAdvanceToNextTimer(): number {
  resetTimers();

  let callCount: number = 0;
  setTimeout(function(): void {
    callCount = callCount + 1;
  }, 1500);
  setTimeout(function(): void {
    callCount = callCount + 1;
  }, 1000);

  advanceToNextTimer();
  if (callCount != 1) { return 1; }

  advanceToNextTimer();
  if (callCount != 2) { return 2; }
  return 0;
}

function testMultipleTimeouts(): number {
  resetTimers();

  let results: number[] = [];
  setTimeout(function(): void {
    results = results + [1];
  }, 1000);
  setTimeout(function(): void {
    results = results + [2];
  }, 2000);
  setTimeout(function(): void {
    results = results + [3];
  }, 1500);

  advanceTime(1500);
  if (results.length != 2) { return 1; }
  if (results[0] != 1) { return 2; }
  if (results[1] != 3) { return 3; }
  return 0;
}

function testRepeatEvery(): number {
  resetTimers();

  let callCount: number = 0;
  repeatEvery(function(): void {
    callCount = callCount + 1;
  }, 500, 2000);

  advanceTime(2500);
  if (callCount < 4) { return 1; }
  return 0;
}

function testDebounceTimer(): number {
  resetTimers();

  let callCount: number = 0;
  let debouncedFn: () => void = debounceTimer(function(): void {
    callCount = callCount + 1;
  }, 500);

  debouncedFn();
  debouncedFn();
  debouncedFn();

  advanceTime(500);
  if (callCount != 1) { return 1; }
  return 0;
}

function testThrottleTimer(): number {
  resetTimers();

  let callCount: number = 0;
  let throttledFn: () => void = throttleTimer(function(): void {
    callCount = callCount + 1;
  }, 500);

  throttledFn();
  if (callCount != 1) { return 1; }

  throttledFn();
  if (callCount != 1) { return 2; }

  advanceTime(500);
  throttledFn();
  if (callCount != 2) { return 3; }
  return 0;
}

function testWait(): number {
  resetTimers();

  let startTime: number = now();
  wait(1000);

  if (now() - startTime != 1000) { return 1; }
  return 0;
}

function testNestedTimeouts(): number {
  resetTimers();

  let order: number[] = [];

  setTimeout(function(): void {
    order = order + [1];
    setTimeout(function(): void {
      order = order + [2];
    }, 1000);
  }, 1000);

  advanceTime(1000);
  if (order.length != 1) { return 1; }

  advanceTime(1000);
  if (order.length != 2) { return 2; }
  return 0;
}

function testResetTimers(): number {
  resetTimers();

  setTimeout(function(): void {
  }, 1000);
  setInterval(function(): void {
  }, 500);

  if (activeTimers() != 2) { return 1; }

  resetTimers();
  if (activeTimers() != 0) { return 2; }
  if (now() != 0) { return 3; }
  return 0;
}
