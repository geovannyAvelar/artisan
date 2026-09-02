// Generics: functions, interfaces, and classes, monomorphized per
// distinct type used.

function identity<T>(x: T): T { return x; }

interface Box<T> { value: T; }

class Wrapper<T> {
  raw: T;
  get value(): T { return this.raw; }
  function reset(v: T): void { this.raw = v; }
}

function main(): number {
  let fails: number = 0;

  let n: number = identity::<number>(5);
  if (n != 5) { fails = fails + 1; }
  let s: string = identity::<string>("hi");
  if (s != "hi") { fails = fails + 1; }

  let b: Box<number> = { value: 42 };
  if (b.value != 42) { fails = fails + 1; }

  let w: Wrapper<string> = { raw: "start" };
  if (w.value != "start") { fails = fails + 1; }
  w.reset("changed");
  if (w.value != "changed") { fails = fails + 1; }

  return fails;
}
