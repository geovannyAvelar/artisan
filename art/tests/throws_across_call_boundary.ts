// An exception thrown from a CALLED function - not lexically inside
// the try at all - still reaches the caller's try/catch, proving the
// checked-exceptions call-graph propagation actually crosses a real
// function-call boundary (A calls B, B has no local try, B declares
// throws and propagates out to A's own try).

function assertEq(actual: number, expected: number, failCount: number): number {
  if (actual != expected) { return failCount + 1; }
  return failCount;
}

function risky(x: number): number throws string {
  if (x < 0) { throw "negative"; }
  return x * 2;
}

function wrapper(x: number): number throws string {
  // No local try here at all - a failure just propagates straight
  // through, since wrapper itself declares 'throws string' too.
  return risky(x) + 1;
}

function main(): number {
  let fails: number = 0;

  let result: number = 0;
  try {
    result = wrapper(5);
  } catch (e: string) {
    fails = fails + 1; // should never run
  }
  fails = assertEq(result, 11, fails);

  let caught: string = "";
  try {
    result = wrapper(-1);
  } catch (e: string) {
    caught = e;
  }
  if (caught != "negative") { fails = fails + 1; }

  return fails;
}
