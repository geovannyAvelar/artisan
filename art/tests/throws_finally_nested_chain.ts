// A rethrow from a nested try's catch block correctly runs BOTH the
// inner try's own finally (via the explicit pending-finally chain,
// since its frame is already off the runtime stack) and the outer
// try's finally (via the ordinary longjmp/landing mechanism, since the
// outer try's frame is still live) - in the right order, innermost
// first.

function assertEq(actual: number, expected: number, failCount: number): number {
  if (actual != expected) { return failCount + 1; }
  return failCount;
}

function main(): number {
  let fails: number = 0;

  let order: string = "";
  let outerCaught: string = "";

  try {
    try {
      throw "start";
    } catch (e: string) {
      order = order + "inner-catch,";
      throw "rethrown";
    } finally {
      order = order + "inner-finally,";
    }
  } catch (e: string) {
    outerCaught = e;
    order = order + "outer-catch";
  } finally {
    order = order + ",outer-finally";
  }

  if (order != "inner-catch,inner-finally,outer-catch,outer-finally") { fails = fails + 1; }
  if (outerCaught != "rethrown") { fails = fails + 1; }

  return fails;
}
