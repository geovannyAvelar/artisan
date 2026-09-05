// readonly: a field is still set the normal way (a `{}` object literal
// at construction) and freely READ afterward - only assignment/
// increment-decrement afterward is rejected, on both a plain interface
// and a class field.

interface Point {
  readonly x: number;
  readonly y: number;
  label: string; // an ordinary, still-mutable field alongside a readonly one
}

class Account {
  readonly id: number;
  balance: number;

  function deposit(amount: number): void {
    this.balance = this.balance + amount; // balance is NOT readonly - fine
  }
}

function assertEq(actual: number, expected: number, failCount: number): number {
  if (actual != expected) { return failCount + 1; }
  return failCount;
}

function main(): number {
  let fails: number = 0;

  let p: Point = { x: 1, y: 2, label: "origin" };
  fails = assertEq(p.x, 1, fails);
  fails = assertEq(p.y, 2, fails);
  p.label = "moved"; // ordinary field - still freely assignable
  if (p.label != "moved") { fails = fails + 1; }

  let a: Account = { id: 42, balance: 100 };
  a.deposit(50);
  fails = assertEq(a.balance, 150, fails);
  fails = assertEq(a.id, 42, fails);

  return fails;
}
