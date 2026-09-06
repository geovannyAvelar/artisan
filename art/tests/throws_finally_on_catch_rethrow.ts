// `finally` runs even when the catch block itself throws a NEW
// exception - a real bug this session's own first pass at `finally`
// had (the try's own frame is already popped by the time catch runs,
// so a throw from inside catch would otherwise skip straight past its
// finally). The new exception still correctly reaches an outer
// handler afterward.

function assertEq(actual: number, expected: number, failCount: number): number {
  if (actual != expected) { return failCount + 1; }
  return failCount;
}

let innerFinallyRuns: number = 0;

function inner(): void throws string {
  try {
    throw "first";
  } catch (e: string) {
    throw "second"; // a NEW exception, from inside the catch block
  } finally {
    innerFinallyRuns = innerFinallyRuns + 1;
  }
}

function main(): number {
  let fails: number = 0;

  let caught: string = "";
  try {
    inner();
  } catch (e: string) {
    caught = e;
  }

  if (caught != "second") { fails = fails + 1; } // the NEW exception, not the original
  fails = assertEq(innerFinallyRuns, 1, fails);

  return fails;
}
