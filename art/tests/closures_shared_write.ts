// Two independent closures sharing one captured variable observe each
// other's writes - the core proof of capture-by-reference (as opposed
// to each closure getting its own snapshot copy).

function assertEq(actual: number, expected: number, failCount: number): number {
  if (actual != expected) { return failCount + 1; }
  return failCount;
}

function main(): number {
  let fails: number = 0;

  let count: number = 0;
  let lastSeen: number = -1;

  let increment: () => void = function(): void { count = count + 1; };
  let record: () => void = function(): void { lastSeen = count; };

  increment();
  increment();
  record();
  fails = assertEq(lastSeen, 2, fails);

  increment();
  record();
  fails = assertEq(lastSeen, 3, fails);

  return fails;
}
