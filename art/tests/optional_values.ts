// Optional<T> - ART's null/undefined-free way to say "this might not be
// there", checked via .hasValue. Not a compiler feature - see README.md's
// "Optional values" section for the full writeup of why (no null literal,
// no union types) and this exact source.

interface Optional<T> {
  hasValue: boolean;
  value: T;
}

function some<T>(v: T): Optional<T> {
  return { hasValue: true, value: v };
}

function none<T>(placeholder: T): Optional<T> {
  return { hasValue: false, value: placeholder };
}

interface Point {
  x: number;
  y: number;
}

function assertEq(actual: number, expected: number, failCount: number): number {
  if (actual != expected) { return failCount + 1; }
  return failCount;
}

function findUser(id: number): Optional<string> {
  if (id == 1) { return some::<string>("Ada"); }
  return none::<string>("");
}

function main(): number {
  let fails: number = 0;

  let a: Optional<number> = some::<number>(5);
  if (!a.hasValue) { fails = fails + 1; }
  fails = assertEq(a.value, 5, fails);

  let b: Optional<number> = none::<number>(0);
  if (b.hasValue) { fails = fails + 1; }
  fails = assertEq(b.value, 0, fails); // reads back the placeholder, not garbage

  // works for a struct type too, not just number
  let p: Optional<Point> = some::<Point>({ x: 1, y: 2 });
  if (!p.hasValue) { fails = fails + 1; }
  fails = assertEq(p.value.x, 1, fails);
  fails = assertEq(p.value.y, 2, fails);

  let noPoint: Optional<Point> = none::<Point>({ x: 0, y: 0 });
  if (noPoint.hasValue) { fails = fails + 1; }

  // a real function returning Optional<T>, checked by the caller
  let found: Optional<string> = findUser(1);
  if (!found.hasValue) { fails = fails + 1; }
  if (found.value != "Ada") { fails = fails + 1; }

  let notFound: Optional<string> = findUser(999);
  if (notFound.hasValue) { fails = fails + 1; }

  // nested Optional - Optional<Optional<T>> is an ordinary instantiation,
  // nothing special about Optional itself
  let nested: Optional<Optional<number>> = some::<Optional<number>>(some::<number>(42));
  if (!nested.hasValue) { fails = fails + 1; }
  if (!nested.value.hasValue) { fails = fails + 1; }
  fails = assertEq(nested.value.value, 42, fails);

  return fails;
}
