// A closure nested inside another closure, capturing from TWO frames
// out (main -> outer -> inner) - the intermediate closure (`outer`)
// never itself reads `x`, but still has to thread its cell pointer
// through its own env for `inner` to find.

function assertEq(actual: number, expected: number, failCount: number): number {
  if (actual != expected) { return failCount + 1; }
  return failCount;
}

function main(): number {
  let fails: number = 0;

  let x: number = 10;
  let result: number = 0;

  let outer: () => void = function(): void {
    let inner: () => void = function(): void {
      result = x;
    };
    inner();
  };

  outer();
  fails = assertEq(result, 10, fails);

  x = 99;
  outer();
  fails = assertEq(result, 99, fails);

  return fails;
}
