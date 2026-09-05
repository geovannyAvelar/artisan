// do-while: same shape as while, but the body always runs at least once,
// even when the condition is false from the very start - that's the one
// thing that actually distinguishes it from `while`.

function assertEq(actual: number, expected: number, failCount: number): number {
  if (actual != expected) { return failCount + 1; }
  return failCount;
}

function main(): number {
  let fails: number = 0;

  // condition false from the start - body still runs exactly once
  let runs: number = 0;
  do {
    runs = runs + 1;
  } while (false);
  fails = assertEq(runs, 1, fails);

  // ordinary counting loop, same result a while loop would give
  let n: number = 0;
  do {
    n = n + 1;
  } while (n < 5);
  fails = assertEq(n, 5, fails);

  // Sema's own return-analysis understands a do-while body that always
  // returns means the whole loop does too (the body ALWAYS runs at
  // least once, unlike while/for) - see alwaysReturnsBody below, which
  // needs no fallback return after its own do-while to type-check.
  return fails;
}

function alwaysReturnsBody(): number {
  let i: number = 0;
  do {
    return i;
  } while (i < 10);
}
