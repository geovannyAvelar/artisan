// A generic function with its own `throws T` clause, instantiated
// concretely and called inside a matching try/catch.

function assertEq(actual: number, expected: number, failCount: number): number {
  if (actual != expected) { return failCount + 1; }
  return failCount;
}

function requirePositive<T>(x: number, value: T): T throws string {
  if (x < 0) { throw "must be positive"; }
  return value;
}

function main(): number {
  let fails: number = 0;

  let ok: number = 0;
  try {
    ok = requirePositive::<number>(5, 42);
  } catch (e: string) {
    fails = fails + 1; // should never run
  }
  fails = assertEq(ok, 42, fails);

  let caught: string = "";
  try {
    ok = requirePositive::<number>(-1, 42);
  } catch (e: string) {
    caught = e;
  }
  if (caught != "must be positive") { fails = fails + 1; }

  return fails;
}
