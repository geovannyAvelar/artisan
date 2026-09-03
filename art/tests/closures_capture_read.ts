// A closure reading an outer local - and, since capture is by
// REFERENCE (not a snapshot), seeing a later write to it too.

function assertEq(actual: number, expected: number, failCount: number): number {
  if (actual != expected) { return failCount + 1; }
  return failCount;
}

function main(): number {
  let fails: number = 0;

  let x: number = 42;
  let observed: number = 0;
  let readX: () => void = function(): void {
    observed = x;
  };

  readX();
  fails = assertEq(observed, 42, fails);

  x = 100; // by-reference: readX sees this, not a stale copy of 42
  readX();
  fails = assertEq(observed, 100, fails);

  return fails;
}
