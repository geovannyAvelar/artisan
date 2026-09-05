function assertEq(actual: number, expected: number, failCount: number): number {
  if (actual != expected) { return failCount + 1; }
  return failCount;
}

let log: string = "";

function record(tag: string): void {
  log = log + tag;
}

// 1. finally always runs on normal completion, even with no catch.
function normalCompletion(): number {
  try {
    record("A");
  } finally {
    record("F");
  }
  return 0;
}

// 2. finally runs when the try body throws and is caught.
function caughtException(): number {
  try {
    record("A");
    throw { message: "boom" };
  } catch (e: Error) {
    record("C");
  } finally {
    record("F");
  }
  return 0;
}

// 3. finally runs when there's no catch and the exception propagates to
// an OUTER catch.
function propagatesThroughFinallyOnly(): number {
  try {
    try {
      record("A");
      throw { message: "boom" };
    } finally {
      record("F");
    }
  } catch (e: Error) {
    record("C");
  }
  return 0;
}

// 4. finally runs exactly once on an early return crossing it.
function earlyReturn(): number {
  try {
    record("A");
    return 1;
  } finally {
    record("F");
  }
}

// 5. finally runs exactly once on break/continue crossing it, inside a loop.
function earlyBreakContinue(): number {
  let sum: number = 0;
  for (let i: number = 0; i < 3; i = i + 1) {
    try {
      if (i == 1) { continue; }
      if (i == 2) { break; }
      sum = sum + 1;
    } finally {
      sum = sum + 10;
    }
  }
  return sum;
}

// 6. THE hard case: finally still runs even when the CATCH body itself throws.
function catchBodyThrows(): number {
  try {
    try {
      record("A");
      throw { message: "first" };
    } catch (e: Error) {
      record("C");
      throw { message: "second" };
    } finally {
      record("F");
    }
  } catch (e: Error) {
    record("O"); // outer catch sees the SECOND exception
    if (e.message != "second") { return 1; }
  }
  return 0;
}

// 7. Nested finally blocks run innermost-first on a single early exit.
function nestedFinally(): number {
  try {
    try {
      record("A");
      return 42;
    } finally {
      record("1");
    }
  } finally {
    record("2");
  }
}

// break/continue targeting a loop that's ITSELF inside the finally is legal.
function loopInsideFinally(): number {
  let total: number = 0;
  try {
    record("A");
  } finally {
    for (let i: number = 0; i < 5; i = i + 1) {
      if (i == 3) { break; }
      total = total + 1;
    }
  }
  return total;
}

function main(): number {
  let fails: number = 0;

  log = "";
  fails = assertEq(normalCompletion(), 0, fails);
  if (log != "AF") { fails = fails + 1; }

  log = "";
  fails = assertEq(caughtException(), 0, fails);
  if (log != "ACF") { fails = fails + 1; }

  log = "";
  fails = assertEq(propagatesThroughFinallyOnly(), 0, fails);
  if (log != "AFC") { fails = fails + 1; }

  log = "";
  fails = assertEq(earlyReturn(), 1, fails);
  if (log != "AF") { fails = fails + 1; }

  // i=0: sum+1=1, +10=11. i=1: continue - +10=21 (no +1). i=2: break - +10=31 (no +1).
  fails = assertEq(earlyBreakContinue(), 31, fails);

  log = "";
  fails = assertEq(catchBodyThrows(), 0, fails);
  if (log != "ACFO") { fails = fails + 1; }

  log = "";
  fails = assertEq(nestedFinally(), 42, fails);
  if (log != "A12") { fails = fails + 1; }

  fails = assertEq(loopInsideFinally(), 3, fails);

  return fails;
}
