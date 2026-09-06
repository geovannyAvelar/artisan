// `finally` runs even when the try or catch block exits early via
// `return` - and the returned value is computed before `finally` runs
// (so `finally`'s own side effects can't retroactively change it).

function assertEq(actual: number, expected: number, failCount: number): number {
  if (actual != expected) { return failCount + 1; }
  return failCount;
}

let finallyRuns: number = 0;

function returnFromTry(): number {
  try {
    return 1;
  } catch (e: string) {
    return -1; // should never run - nothing thrown
  } finally {
    finallyRuns = finallyRuns + 1;
  }
}

function returnFromCatch(): number throws string {
  try {
    throw "boom";
  } catch (e: string) {
    return 2;
  } finally {
    finallyRuns = finallyRuns + 1;
  }
}

function returnValueSnapshot(): number {
  let x: number = 10;
  try {
    return x;
  } catch (e: string) {
    return -1; // should never run - nothing thrown
  } finally {
    x = 99; // must not affect the already-returned value
  }
}

function main(): number {
  let fails: number = 0;

  fails = assertEq(returnFromTry(), 1, fails);
  fails = assertEq(finallyRuns, 1, fails);

  let caught: number = 0;
  try {
    caught = returnFromCatch();
  } catch (e: string) {
    fails = fails + 1; // should never run - returnFromCatch handles it internally
  }
  fails = assertEq(caught, 2, fails);
  fails = assertEq(finallyRuns, 2, fails);

  fails = assertEq(returnValueSnapshot(), 10, fails);

  return fails;
}
