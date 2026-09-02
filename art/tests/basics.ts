// Core language basics: number/boolean/string/array types, operators
// (including the conditional/ternary one), and control flow.

function assertEq(actual: number, expected: number, failCount: number): number {
  if (actual != expected) { return failCount + 1; }
  return failCount;
}

function main(): number {
  let fails: number = 0;

  let a: number = 2;
  let b: number = 3;
  fails = assertEq(a + b, 5, fails);
  fails = assertEq(a * b, 6, fails);
  fails = assertEq(b - a, 1, fails);
  fails = assertEq(a > b ? a : b, 3, fails); // ternary
  fails = assertEq(a < b ? a : b, 2, fails);

  let s: string = "hello, " + "world";
  if (s != "hello, world") { fails = fails + 1; }
  if (s.length != 12) { fails = fails + 1; }
  if (s[0] != "h") { fails = fails + 1; }

  let xs: number[] = [10, 20, 30];
  let sum: number = 0;
  for (let i: number = 0; i < xs.length; i++) { sum = sum + xs[i]; }
  fails = assertEq(sum, 60, fails);

  let sum2: number = 0;
  for (let x of xs) { sum2 = sum2 + x; }
  fails = assertEq(sum2, 60, fails);

  let n: number = 0;
  while (n < 5) { n++; }
  fails = assertEq(n, 5, fails);

  let flag: boolean = a < b && b < 10;
  if (!flag) { fails = fails + 1; }

  return fails;
}
