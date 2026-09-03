// A `let` captured inside a loop BODY gets a fresh cell every
// iteration - each closure created in the loop sees its own iteration's
// value, not whatever the loop variable ended up at. Also pins down the
// one accepted gap from this: a classic `for (let k = ...; ...; k++)`
// HEADER counter does NOT get per-iteration cells (every closure shares
// the loop's own single, final-valued cell) - see art/README.md's
// "Closures" section.

function assertEq(actual: number, expected: number, failCount: number): number {
  if (actual != expected) { return failCount + 1; }
  return failCount;
}

function noop(): void {}

function main(): number {
  let fails: number = 0;

  // A `let` in the loop BODY - fresh cell per iteration.
  let results: number[] = makeArray::<number>(3, -1);
  let handlers: () => void[] = makeArray::<() => void>(3, noop);
  let i: number = 0;
  while (i < 3) {
    let idx: number = i;
    handlers[idx] = function(): void { results[idx] = idx; };
    i = i + 1;
  }
  let j: number = 0;
  while (j < 3) {
    let h: () => void = handlers[j];
    h();
    j = j + 1;
  }
  fails = assertEq(results[0], 0, fails);
  fails = assertEq(results[1], 1, fails);
  fails = assertEq(results[2], 2, fails);

  // A classic `for` HEADER counter - one shared cell for the whole loop,
  // so every closure sees whatever `k` ended up at (3) once the loop
  // exits, not its own iteration's value. Documented, accepted gap - not
  // a bug, see this file's own top comment.
  let headerResult: number = -1;
  let headerHandlers: () => void[] = makeArray::<() => void>(3, noop);
  for (let k: number = 0; k < 3; k++) {
    headerHandlers[k] = function(): void { headerResult = k; };
  }
  let m: number = 0;
  while (m < 3) {
    let hh: () => void = headerHandlers[m];
    hh();
    m = m + 1;
  }
  fails = assertEq(headerResult, 3, fails);

  return fails;
}
