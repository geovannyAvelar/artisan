// Object destructuring: `let { a, b: renamed } = expr;` - reads plain
// fields off a struct into new locals, with optional renaming. Scoped
// to interface/class values only (no array destructuring, no defaults -
// see StmtKind::Destructure's own doc comment for why).

interface Point {
  x: number;
  y: number;
}

class Person {
  name: string;
  age: number;

  function greeting(): string {
    return "hi " + this.name;
  }
}

function assertEq(actual: number, expected: number, failCount: number): number {
  if (actual != expected) { return failCount + 1; }
  return failCount;
}

function main(): number {
  let fails: number = 0;

  let p: Point = { x: 3, y: 4 };
  let { x, y } = p;
  fails = assertEq(x, 3, fails);
  fails = assertEq(y, 4, fails);

  // renaming
  let { x: px, y: py } = p;
  fails = assertEq(px, 3, fails);
  fails = assertEq(py, 4, fails);

  // only some fields
  let { x: justX } = p;
  fails = assertEq(justX, 3, fails);

  // works on a class instance too (still a plain field read, not through
  // any get/set property)
  let person: Person = { name: "Ada", age: 36 };
  let { name, age } = person;
  fails = assertEq(age, 36, fails);
  if (name != "Ada") { fails = fails + 1; }

  // destructuring directly from a function's return value, not just a
  // variable already holding one
  let { x: fx, y: fy } = makePoint();
  fails = assertEq(fx, 10, fails);
  fails = assertEq(fy, 20, fails);

  // const destructuring
  const { x: cx } = p;
  fails = assertEq(cx, 3, fails);

  // a destructured local captured by a closure - correctly boxed, sees
  // the value at destructuring time (a real, independent local, not a
  // live view into the original struct - matching a plain `let`'s own
  // by-value field-read semantics)
  let { x: capturedX } = p;
  let holder: NumberHolder = { value: 0 };
  let readCapturedX: () => void = function(): void {
    holder.value = capturedX;
  };
  readCapturedX();
  fails = assertEq(holder.value, 3, fails);

  return fails;
}

interface NumberHolder {
  value: number;
}

function makePoint(): Point {
  return { x: 10, y: 20 };
}
