// A nested try whose inner catch type doesn't match the thrown type
// lets it propagate to (and runs its own `finally` on the way past)
// an outer try that does match.

function assertEq(actual: number, expected: number, failCount: number): number {
  if (actual != expected) { return failCount + 1; }
  return failCount;
}

function main(): number {
  let fails: number = 0;

  let innerFinallyRan: boolean = false;
  let innerCatchRan: boolean = false;
  let outerCaught: string = "";

  try {
    try {
      throw "outer-bound";
    } catch (e: number) {
      innerCatchRan = true; // should never run - type mismatch (number vs string)
    } finally {
      innerFinallyRan = true; // still runs, even though this try didn't handle it
    }
  } catch (e: string) {
    outerCaught = e;
  }

  if (innerCatchRan) { fails = fails + 1; }
  if (!innerFinallyRan) { fails = fails + 1; }
  if (outerCaught != "outer-bound") { fails = fails + 1; }

  return fails;
}
