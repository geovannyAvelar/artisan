function assertEq(actual: number, expected: number, failCount: number): number {
  if (actual != expected) { return failCount + 1; }
  return failCount;
}

// if (x != null) { ... x usable as plain T ... }
function addOne(x: number | null): number {
  if (x != null) {
    return x + 1; // x used as plain number here - no unboxing syntax needed
  }
  return 0;
}

// if (x == null) { <exits> } ... x usable as plain T after ...
function addOneEarlyExit(x: number | null): number {
  if (x == null) { return 0; }
  return x + 1; // narrowed for the rest of the block
}

interface Point { x: number; y: number; }

function describePoint(p: Point | null): string {
  if (p == null) { return "no point"; }
  return "(" + numberToString(p.x) + ", " + numberToString(p.y) + ")";
}

function main(): number {
  let fails: number = 0;

  fails = assertEq(addOne(notNull::<number>(5)), 6, fails);
  fails = assertEq(addOne(null), 0, fails);

  fails = assertEq(addOneEarlyExit(notNull::<number>(10)), 11, fails);
  fails = assertEq(addOneEarlyExit(null), 0, fails);

  if (describePoint(notNull::<Point>({ x: 1, y: 2 })) != "(1, 2)") { fails = fails + 1; }
  if (describePoint(null) != "no point") { fails = fails + 1; }

  // narrowing in a loop, with continue
  let sum: number = 0;
  for (let i: number = 0; i < 3; i = i + 1) {
    let v: number | null = maybeValue(i);
    if (v == null) { continue; }
    sum = sum + v;
  }
  fails = assertEq(sum, 40, fails);

  return fails;
}

function maybeValue(i: number): number | null {
  if (i == 1) { return null; }
  return notNull::<number>((i + 1) * 10);
}
