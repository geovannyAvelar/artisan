// switch: real fallthrough (no implicit break, matching real TS/JS),
// break ending an arm early, default (in the middle AND at the end),
// and switch over each of number/string/boolean.

function assertEq(actual: number, expected: number, failCount: number): number {
  if (actual != expected) { return failCount + 1; }
  return failCount;
}

function classify(n: number): number {
  let result: number = 0;
  switch (n) {
    case 1:
      result = 100;
      break;
    case 2:
    case 3:
      // no value of its own - falls through from `case 2:` above, so
      // both 2 and 3 land here (the classic "grouped case" pattern).
      result = 200;
      break;
    default:
      result = -1;
  }
  return result;
}

function fallthroughSum(n: number): number {
  // Deliberately NO break anywhere - every arm from the matched case
  // onward runs, real fallthrough.
  let total: number = 0;
  switch (n) {
    case 1:
      total = total + 1;
    case 2:
      total = total + 2;
    case 3:
      total = total + 3;
  }
  return total;
}

function stringSwitch(s: string): number {
  switch (s) {
    case "a": return 1;
    case "b": return 2;
    default: return -1;
  }
}

function boolSwitch(b: boolean): number {
  switch (b) {
    case true: return 1;
    case false: return 0;
  }
  return -1; // unreachable given only two boolean values, but a real path
}

function defaultInMiddle(n: number): number {
  // `default:` isn't required to be last - falling through from it
  // reaches whatever textually follows it, same as any other arm.
  let result: number = 0;
  switch (n) {
    case 1:
      result = 1;
      break;
    default:
      result = 999;
    case 2:
      result = result + 2;
      break;
  }
  return result;
}

function main(): number {
  let fails: number = 0;

  fails = assertEq(classify(1), 100, fails);
  fails = assertEq(classify(2), 200, fails);
  fails = assertEq(classify(3), 200, fails);
  fails = assertEq(classify(4), -1, fails);

  fails = assertEq(fallthroughSum(1), 6, fails); // 1+2+3
  fails = assertEq(fallthroughSum(2), 5, fails); // 2+3
  fails = assertEq(fallthroughSum(3), 3, fails); // 3
  fails = assertEq(fallthroughSum(4), 0, fails); // no match, no default

  fails = assertEq(stringSwitch("a"), 1, fails);
  fails = assertEq(stringSwitch("b"), 2, fails);
  fails = assertEq(stringSwitch("z"), -1, fails);

  fails = assertEq(boolSwitch(true), 1, fails);
  fails = assertEq(boolSwitch(false), 0, fails);

  // n=99: matches neither case 1 nor case 2, falls to default (result=999),
  // then falls through into case 2's own body (result = 999 + 2 = 1001).
  fails = assertEq(defaultInMiddle(99), 1001, fails);
  fails = assertEq(defaultInMiddle(1), 1, fails);
  fails = assertEq(defaultInMiddle(2), 2, fails);

  // A switch inside a loop: break exits only the SWITCH, not the loop -
  // continue (if reached from inside a switch) still targets the loop.
  let loopIters: number = 0;
  let switchHits: number = 0;
  for (let i: number = 0; i < 5; i = i + 1) {
    loopIters = loopIters + 1;
    switch (i) {
      case 2:
        switchHits = switchHits + 1;
        break; // exits the switch only - the for loop keeps going
      default:
        break;
    }
  }
  fails = assertEq(loopIters, 5, fails);
  fails = assertEq(switchHits, 1, fails);

  // continue inside a switch inside a loop skips past the switch to the
  // next loop iteration, not just the rest of the switch.
  let sumSkip3: number = 0;
  for (let j: number = 0; j < 6; j = j + 1) {
    switch (j) {
      case 3:
        continue; // continues the for loop, never reaches the line below
    }
    sumSkip3 = sumSkip3 + j;
  }
  fails = assertEq(sumSkip3, 12, fails); // 0+1+2+4+5

  return fails;
}
