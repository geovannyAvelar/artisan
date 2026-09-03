// A plain top-level function used as a first-class Handler value (no
// closure literal involved) still needs to work - it gets its own
// lazily-generated, cached "ignore env" thunk (see
// Codegen::GetOrCreatePlainThunk) so it's callable through the exact
// same {fn, env} convention a real closure uses. Also exercises Handler
// equality for the plain-function case.

function assertEq(actual: number, expected: number, failCount: number): number {
  if (actual != expected) { return failCount + 1; }
  return failCount;
}

let calls: number = 0;

function bump(): void { calls = calls + 1; }
function bumpTwo(): void { calls = calls + 2; }

function callIt(h: () => void): void { h(); }

function main(): number {
  let fails: number = 0;

  let h: () => void = bump;
  callIt(h);
  fails = assertEq(calls, 1, fails);

  let h2: () => void = bump;
  if (h != h2) { fails = fails + 1; } // same plain function -> equal

  let h3: () => void = bumpTwo;
  if (h == h3) { fails = fails + 1; } // different function -> unequal

  return fails;
}
