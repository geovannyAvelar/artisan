function assertEq(actual: number, expected: number, failCount: number): number {
  if (actual != expected) { return failCount + 1; }
  return failCount;
}

// typeof reports the right tag for every boxable kind.
function typeOfIt(x: any): string {
  return typeof x;
}

interface Point { x: number; y: number; }

function main(): number {
  let fails: number = 0;

  // Implicit widening into `any` - no ceremony needed.
  let n: any = 5;
  let b: any = true;
  let s: any = "hello";
  let point: Point = { x: 1, y: 2 };
  let p: any = point;
  let nums: number[] = [1, 2, 3];
  let arr: any = nums;
  let fn: any = function (): void {};
  let nul: any = null;

  if (typeOfIt(n) != "number") { fails = fails + 1; }
  if (typeOfIt(b) != "boolean") { fails = fails + 1; }
  if (typeOfIt(s) != "string") { fails = fails + 1; }
  if (typeOfIt(p) != "object") { fails = fails + 1; }
  if (typeOfIt(arr) != "object") { fails = fails + 1; }
  if (typeOfIt(fn) != "function") { fails = fails + 1; }
  if (typeOfIt(nul) != "object") { fails = fails + 1; } // real JS quirk, deliberately preserved

  // Narrowing pattern 1: `if (typeof x === "...") { narrowed here }`.
  let value: any = 42;
  if (typeof value == "number") {
    fails = assertEq(value + 8, 50, fails);
  } else {
    fails = fails + 1; // should not be reached
  }

  let text: any = "world";
  if (typeof text == "string") {
    if ("hello, " + text != "hello, world") { fails = fails + 1; }
  } else {
    fails = fails + 1;
  }

  let flag: any = true;
  if (typeof flag == "boolean") {
    if (flag) {
      // ok - flag is usable as a plain boolean here
    } else {
      fails = fails + 1;
    }
  } else {
    fails = fails + 1;
  }

  // Narrowing pattern 2: `if (typeof x !== "...") { <exits> } use x after`.
  fails = assertEq(doubleIfNumber(notAny(21)), 42, fails);
  fails = assertEq(doubleIfNumber(notAny("skip")), -1, fails);

  return fails;
}

function notAny(v: any): any {
  return v;
}

function doubleIfNumber(x: any): number {
  if (typeof x != "number") { return -1; }
  return x * 2; // narrowed for the rest of the function body
}
