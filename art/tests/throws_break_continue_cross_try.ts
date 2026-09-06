// break/continue crossing a try/finally boundary inside a loop still
// run every finally they pass on their way out - the one capability
// this design didn't originally have, backported from origin's own
// already-proven scopeStack/GenExceptionHandlerCleanup mechanism
// (reused verbatim - see the "throws" feature's own design notes).

function assertEq(actual: number, expected: number, failCount: number): number {
  if (actual != expected) { return failCount + 1; }
  return failCount;
}

function main(): number {
  let fails: number = 0;

  // break crossing one try/finally.
  let breakFinallyRuns: number = 0;
  let i: number = 0;
  while (i < 5) {
    try {
      if (i == 2) { break; }
    } catch (e: string) {
      fails = fails + 1; // never runs - nothing thrown
    } finally {
      breakFinallyRuns = breakFinallyRuns + 1;
    }
    i = i + 1;
  }
  fails = assertEq(i, 2, fails);
  fails = assertEq(breakFinallyRuns, 3, fails); // ran once per iteration 0, 1, 2 (the break itself)

  // continue crossing one try/finally. (Avoids '%' deliberately - see
  // this file's own note: modulo on numbers needs libc's fmod, which a
  // standalone `art`-linked binary doesn't pull in - a real, pre-
  // existing, unrelated gap, not something to route around silently
  // inside actual ART code, just inside this one test's own arithmetic.)
  let continueFinallyRuns: number = 0;
  let sum: number = 0;
  let isEven: boolean = true; // j == 0 is even
  let j: number = 0;
  while (j < 5) {
    j = j + 1;
    isEven = !isEven;
    try {
      if (isEven) { continue; }
    } finally {
      continueFinallyRuns = continueFinallyRuns + 1;
    }
    sum = sum + j;
  }
  fails = assertEq(sum, 9, fails); // 1 + 3 + 5 (2 and 4 skipped via continue)
  fails = assertEq(continueFinallyRuns, 5, fails); // finally still runs every iteration

  // break escaping two nested try/finally levels.
  let outerFinallyRuns: number = 0;
  let innerFinallyRuns: number = 0;
  let k: number = 0;
  while (k < 3) {
    try {
      try {
        break;
      } finally {
        innerFinallyRuns = innerFinallyRuns + 1;
      }
    } finally {
      outerFinallyRuns = outerFinallyRuns + 1;
    }
    k = k + 1;
  }
  fails = assertEq(innerFinallyRuns, 1, fails);
  fails = assertEq(outerFinallyRuns, 1, fails);

  return fails;
}
