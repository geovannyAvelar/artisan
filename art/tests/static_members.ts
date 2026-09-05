// Static class members: `static name: Type = init;` (a real, class-scoped
// value - never part of any instance) and `static function name(...) {}`
// (no implicit `this`) - both accessed as `ClassName.name`, never
// `instance.name`. A static and an instance member may freely share a
// name (separate namespaces), and a static field's initializer can
// reference an earlier-declared static field.

function assertEq(actual: number, expected: number, failCount: number): number {
  if (actual != expected) { return failCount + 1; }
  return failCount;
}

class Counter {
  static total: number = 0;
  count: number; // instance field - deliberately same shape, no name clash

  function increment(): void {
    this.count = this.count + 1;
    Counter.total = Counter.total + 1;
  }

  static function reset(): void {
    Counter.total = 0;
  }

  static function make(): Counter {
    return { count: 0 };
  }
}

class MathConsts {
  static base: number = 10;
  static doubled: number = MathConsts.base * 2; // references an earlier static field

  static function square(x: number): number {
    return x * x;
  }
}

// A static member and an instance member sharing the same bare name -
// separate namespaces, no collision at all.
class Widget {
  static label: string = "static-label";
  label: string;

  function ownLabel(): string {
    return this.label;
  }
}

function main(): number {
  let fails: number = 0;

  fails = assertEq(Counter.total, 0, fails);

  let a: Counter = Counter.make();
  let b: Counter = Counter.make();
  a.increment();
  a.increment();
  b.increment();
  fails = assertEq(a.count, 2, fails);
  fails = assertEq(b.count, 1, fails);
  fails = assertEq(Counter.total, 3, fails); // shared across every instance

  Counter.reset();
  fails = assertEq(Counter.total, 0, fails);

  fails = assertEq(MathConsts.base, 10, fails);
  fails = assertEq(MathConsts.doubled, 20, fails);
  fails = assertEq(MathConsts.square(5), 25, fails);

  // mutate a static field directly from outside any method
  MathConsts.base = 7;
  fails = assertEq(MathConsts.base, 7, fails);

  let w: Widget = { label: "instance-label" };
  if (w.ownLabel() != "instance-label") { fails = fails + 1; }
  if (Widget.label != "static-label") { fails = fails + 1; }

  return fails;
}
