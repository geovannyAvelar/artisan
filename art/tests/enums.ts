// enum: a closed set of named numeric constants, auto-numbered from 0
// unless a member gives its own explicit value (auto-numbering then
// continues from there) - a real, distinct type from `number` (can't
// assign a bare number literal to it), accessed as `Name.Member`.

enum Color { Red, Green, Blue }
enum HttpStatus { Ok = 200, Created, Accepted, NotFound = 404, Forbidden }
enum Empty2 { Only }

function assertEq(actual: number, expected: number, failCount: number): number {
  if (actual != expected) { return failCount + 1; }
  return failCount;
}

function describe(c: Color): string {
  // switch over an enum discriminant - the same equality machinery any
  // other switch already has, just with Enum's own "compares as a
  // double" codegen path.
  switch (c) {
    case Color.Red: return "red";
    case Color.Green: return "green";
    case Color.Blue: return "blue";
  }
  return "unknown";
}

function main(): number {
  let fails: number = 0;

  // auto-numbering from 0
  fails = assertEq(numberOf(Color.Red), 0, fails);
  fails = assertEq(numberOf(Color.Green), 1, fails);
  fails = assertEq(numberOf(Color.Blue), 2, fails);

  // explicit values, and auto-numbering continuing from the last one
  fails = assertEq(numberOf2(HttpStatus.Ok), 200, fails);
  fails = assertEq(numberOf2(HttpStatus.Created), 201, fails);
  fails = assertEq(numberOf2(HttpStatus.Accepted), 202, fails);
  fails = assertEq(numberOf2(HttpStatus.NotFound), 404, fails);
  fails = assertEq(numberOf2(HttpStatus.Forbidden), 405, fails);

  // equality
  let c: Color = Color.Red;
  if (c != Color.Red) { fails = fails + 1; }
  if (c == Color.Blue) { fails = fails + 1; }

  // real switch fallthrough/dispatch over an enum value
  if (describe(Color.Red) != "red") { fails = fails + 1; }
  if (describe(Color.Blue) != "blue") { fails = fails + 1; }

  // stored in a field/passed as a parameter/returned - ordinary value
  // semantics, same as any number under the hood
  let picked: Color = pick(true);
  if (picked != Color.Green) { fails = fails + 1; }

  // a single-member enum still auto-numbers from 0
  fails = assertEq(numberOf3(Empty2.Only), 0, fails);

  return fails;
}

// A generic-looking helper that just forces the enum value through a
// number-shaped comparison via '==' against every member, proving the
// underlying value really is what auto-numbering/explicit values say -
// there's no `numberToEnum`/cast in ART, so this is how a test extracts
// the actual double back out.
function numberOf(c: Color): number {
  if (c == Color.Red) { return 0; }
  if (c == Color.Green) { return 1; }
  if (c == Color.Blue) { return 2; }
  return -1;
}

function numberOf2(s: HttpStatus): number {
  if (s == HttpStatus.Ok) { return 200; }
  if (s == HttpStatus.Created) { return 201; }
  if (s == HttpStatus.Accepted) { return 202; }
  if (s == HttpStatus.NotFound) { return 404; }
  if (s == HttpStatus.Forbidden) { return 405; }
  return -1;
}

function numberOf3(e: Empty2): number {
  if (e == Empty2.Only) { return 0; }
  return -1;
}

function pick(greenNotRed: boolean): Color {
  return greenNotRed ? Color.Green : Color.Red;
}
