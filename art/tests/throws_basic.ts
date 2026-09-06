// A simple throw/catch inside one function - the catch variable's
// value is exactly what was thrown.

function assertEq(actual: number, expected: number, failCount: number): number {
  if (actual != expected) { return failCount + 1; }
  return failCount;
}

function main(): number {
  let fails: number = 0;

  let caught: string = "";
  try {
    throw "boom";
  } catch (e: string) {
    caught = e;
  }
  if (caught != "boom") { fails = fails + 1; }

  // Not caught if nothing was thrown.
  let ran: boolean = false;
  try {
    ran = true;
  } catch (e: string) {
    ran = false;
  }
  if (!ran) { fails = fails + 1; }

  return fails;
}
