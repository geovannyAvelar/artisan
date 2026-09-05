// try/catch/throw: real non-local control flow via setjmp/longjmp (see
// codegen.h's own note on why that's the right mechanism for ART
// specifically - no destructors, since everything is GC'd, so none of
// the usual reasons C++-style unwinding needs to be this complicated
// apply here). `Error` (`{ message: string }`) is the one throwable/
// catchable type for now - see StmtKind::Try's own doc comment.

function assertEq(actual: number, expected: number, failCount: number): number {
  if (actual != expected) { return failCount + 1; }
  return failCount;
}

function mightThrow(shouldThrow: boolean): number {
  if (shouldThrow) { throw { message: "boom" }; }
  return 42;
}

function level3(): number { throw { message: "deep" }; }
function level2(): number { return level3(); }
function level1(): number { return level2(); }

function recursiveCatcher(n: number): number {
  if (n == 0) { return 0; }
  try {
    if (n == 2) { throw { message: "skip" }; }
    return n + recursiveCatcher(n - 1);
  } catch (e: Error) {
    return n + recursiveCatcher(n - 1);
  }
}

function returnsFromInsideTry(x: number): number {
  try {
    if (x > 0) { return 100; } // early return - must pop this try's own handler frame
    throw { message: "neg" };
  } catch (e: Error) {
    return -1;
  }
}

function main(): number {
  let fails: number = 0;

  // basic catch, and the no-exception path leaving the value untouched
  fails = assertEq(mightThrow(false), 42, fails);
  try {
    mightThrow(true);
    fails = fails + 1; // unreachable - the throw above skips this
  } catch (e: Error) {
    if (e.message != "boom") { fails = fails + 1; }
  }

  // propagates up through several function-call levels
  try {
    level1();
    fails = fails + 1;
  } catch (e: Error) {
    if (e.message != "deep") { fails = fails + 1; }
  }

  // nested try/catch - inner handles it, outer never sees it
  try {
    try {
      throw { message: "inner" };
    } catch (e: Error) {
      if (e.message != "inner") { fails = fails + 1; }
    }
  } catch (e: Error) {
    fails = fails + 1;
  }

  // rethrow from inside a catch - the outer try catches the new one
  try {
    try {
      throw { message: "first" };
    } catch (e: Error) {
      throw { message: "rethrown: " + e.message };
    }
  } catch (e: Error) {
    if (e.message != "rethrown: first") { fails = fails + 1; }
  }

  // a fresh try/catch inside each loop iteration
  let caughtCount: number = 0;
  for (let i: number = 0; i < 5; i = i + 1) {
    try {
      if (i % 2 == 0) { throw { message: "even" }; }
    } catch (e: Error) {
      caughtCount = caughtCount + 1;
    }
  }
  fails = assertEq(caughtCount, 3, fails); // i = 0, 2, 4

  // recursion with a try/catch at every level - each activation's own
  // handler frame must be genuinely independent
  fails = assertEq(recursiveCatcher(4), 10, fails); // 4+3+2+1+0

  // early return from inside a try (with and without an actual throw)
  fails = assertEq(returnsFromInsideTry(5), 100, fails);
  fails = assertEq(returnsFromInsideTry(-5), -1, fails);

  // break/continue crossing a try boundary - the handler stack must be
  // correctly restored, or a LATER unrelated throw could corrupt memory
  let sum: number = 0;
  for (let i: number = 0; i < 10; i = i + 1) {
    try {
      if (i == 3) { break; }
      sum = sum + i;
    } catch (e: Error) {
      fails = fails + 1;
    }
  }
  fails = assertEq(sum, 3, fails); // 0+1+2

  let sum2: number = 0;
  for (let i: number = 0; i < 5; i = i + 1) {
    try {
      if (i == 2) { continue; }
      sum2 = sum2 + i;
    } catch (e: Error) {
      fails = fails + 1;
    }
  }
  fails = assertEq(sum2, 8, fails); // 0+1+3+4

  // break escaping TWO nested try levels at once inside a loop
  let outerRuns: number = 0;
  for (let i: number = 0; i < 3; i = i + 1) {
    outerRuns = outerRuns + 1;
    try {
      try {
        if (i == 1) { break; }
      } catch (e: Error) {
        fails = fails + 1;
      }
    } catch (e: Error) {
      fails = fails + 1;
    }
  }
  fails = assertEq(outerRuns, 2, fails);

  // the critical check: after every early exit above popped its own
  // handler frame(s), a completely fresh try/throw/catch still has to
  // work - this is where a stale @currentHandler pointing at reused
  // stack memory would actually misbehave.
  try {
    throw { message: "still sane" };
  } catch (e: Error) {
    if (e.message != "still sane") { fails = fails + 1; }
  }

  // a closure that throws, called from inside a try
  let thrower: () => void = function(): void { throw { message: "from closure" }; };
  try {
    thrower();
    fails = fails + 1;
  } catch (e: Error) {
    if (e.message != "from closure") { fails = fails + 1; }
  }

  // the caught exception variable, captured by a closure created inside
  // the catch block - must be boxed correctly
  let savedMessage: string = "";
  let saveIt: () => void = function(): void {};
  try {
    throw { message: "captured" };
  } catch (e: Error) {
    saveIt = function(): void { savedMessage = e.message; };
  }
  saveIt();
  if (savedMessage != "captured") { fails = fails + 1; }

  return fails;
}
