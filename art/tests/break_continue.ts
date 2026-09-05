// break/continue: early loop exit and skip-to-next-iteration, across
// while/do-while/for/for...of, plus break's own switch-only shape and
// the accepted "can't cross a function boundary" rule.

function assertEq(actual: number, expected: number, failCount: number): number {
  if (actual != expected) { return failCount + 1; }
  return failCount;
}

function main(): number {
  let fails: number = 0;

  // break in a while loop
  let i: number = 0;
  while (i < 100) {
    if (i == 5) { break; }
    i = i + 1;
  }
  fails = assertEq(i, 5, fails);

  // continue in a while loop - skip even numbers, sum the odd ones under 10
  let n: number = 0;
  let sumOdds: number = 0;
  while (n < 10) {
    n = n + 1;
    if (n % 2 == 0) { continue; }
    sumOdds = sumOdds + n;
  }
  fails = assertEq(sumOdds, 25, fails); // 1+3+5+7+9

  // break in a do-while loop
  let j: number = 0;
  do {
    if (j == 3) { break; }
    j = j + 1;
  } while (j < 100);
  fails = assertEq(j, 3, fails);

  // continue in a for loop
  let sumSkip3: number = 0;
  for (let k: number = 0; k < 10; k = k + 1) {
    if (k == 3) { continue; }
    sumSkip3 = sumSkip3 + k;
  }
  fails = assertEq(sumSkip3, 42, fails); // 0+1+2+4+5+6+7+8+9

  // break in a for...of loop
  let xs: number[] = [10, 20, 30, 40, 50];
  let found: number = -1;
  for (let x of xs) {
    if (x == 30) { found = x; break; }
  }
  fails = assertEq(found, 30, fails);

  // continue in a for...of loop
  let sumNonNeg: number = 0;
  let ys: number[] = [1, -1, 2, -2, 3];
  for (let y of ys) {
    if (y < 0) { continue; }
    sumNonNeg = sumNonNeg + y;
  }
  fails = assertEq(sumNonNeg, 6, fails);

  // break in a nested loop only exits the INNERMOST loop
  let outerRuns: number = 0;
  for (let a: number = 0; a < 3; a = a + 1) {
    outerRuns = outerRuns + 1;
    for (let b: number = 0; b < 10; b = b + 1) {
      if (b == 2) { break; }
    }
  }
  fails = assertEq(outerRuns, 3, fails);

  // a closure created inside a loop body has its OWN loop (break/continue
  // inside it never touches the outer loop it's textually nested in) -
  // this closure returns void, so it "escapes" its own trivial one-shot
  // while loop with break and the outer loop keeps running normally.
  let closureRuns: number = 0;
  for (let c: number = 0; c < 3; c = c + 1) {
    closureRuns = closureRuns + 1;
    let innerFn: () => void = function(): void {
      let m: number = 0;
      while (m < 100) {
        if (m == 1) { break; }
        m = m + 1;
      }
    };
    innerFn();
  }
  fails = assertEq(closureRuns, 3, fails);

  return fails;
}
