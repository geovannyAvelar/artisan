// `finally` runs exactly once on both the throwing and the
// non-throwing path.

function assertEq(actual: number, expected: number, failCount: number): number {
  if (actual != expected) { return failCount + 1; }
  return failCount;
}

let finallyRuns: number = 0;

function maybeThrow(shouldThrow: boolean): void throws string {
  if (shouldThrow) { throw "boom"; }
}

function main(): number {
  let fails: number = 0;

  // Non-throwing path.
  try {
    maybeThrow(false);
  } catch (e: string) {
    fails = fails + 1; // should never run
  } finally {
    finallyRuns = finallyRuns + 1;
  }
  fails = assertEq(finallyRuns, 1, fails);

  // Throwing path.
  let caught: boolean = false;
  try {
    maybeThrow(true);
  } catch (e: string) {
    caught = true;
  } finally {
    finallyRuns = finallyRuns + 1;
  }
  if (!caught) { fails = fails + 1; }
  fails = assertEq(finallyRuns, 2, fails);

  return fails;
}
