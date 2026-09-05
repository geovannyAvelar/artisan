// Rest parameters: `function f(...args: T[]): void` - collects every
// trailing call-site argument into a real array. Only supported on a
// plain, non-generic, top-level function in this pass (not a method,
// closure, or `declare function` - see Param::isRest's own doc comment),
// and only as individual call-site arguments (no spread-in-calls yet -
// an existing array can't be forwarded directly into one).

function assertEq(actual: number, expected: number, failCount: number): number {
  if (actual != expected) { return failCount + 1; }
  return failCount;
}

function sum(...nums: number[]): number {
  let total: number = 0;
  for (let n of nums) { total = total + n; }
  return total;
}

// leading fixed parameters alongside a trailing rest one
function joinWithPrefix(prefix: string, ...parts: string[]): string {
  let out: string = prefix;
  for (let p of parts) { out = out + p; }
  return out;
}

function countArgs(...xs: number[]): number {
  return xs.length;
}

function main(): number {
  let fails: number = 0;

  fails = assertEq(sum(1, 2, 3), 6, fails);
  fails = assertEq(sum(10), 10, fails);
  fails = assertEq(sum(), 0, fails); // zero rest arguments - a real, empty array

  fails = assertEq(countArgs(), 0, fails);
  fails = assertEq(countArgs(1, 2, 3, 4, 5), 5, fails);

  if (joinWithPrefix("x: ", "a", "b", "c") != "x: abc") { fails = fails + 1; }
  if (joinWithPrefix("empty: ") != "empty: ") { fails = fails + 1; }

  // the rest parameter is a completely ordinary array inside the body -
  // indexing, .length, for...of all just work, nothing special about it
  // beyond how it was populated at the call site
  fails = assertEq(firstOrZero(7, 8, 9), 7, fails);
  fails = assertEq(firstOrZero(), 0, fails);

  return fails;
}

function firstOrZero(...xs: number[]): number {
  if (xs.length == 0) { return 0; }
  return xs[0];
}
