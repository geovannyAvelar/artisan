enum Color { Red, Green, Blue }

class Animal {
  name: string;
  function speak(): string { return this.name + " makes a sound"; }
}

class Dog extends Animal {
  function speak(): string { return this.name + " barks"; }
}

function assertEq(actual: number, expected: number, failCount: number): number {
  if (actual != expected) { return failCount + 1; }
  return failCount;
}

function main(): number {
  let fails: number = 0;

  // any[] and a generic instantiation with `any`.
  let bag: any[] = makeArray::<any>(3, 0);
  bag[0] = 1;
  bag[1] = "two";
  bag[2] = true;
  if (typeof bag[0] != "number") { fails = fails + 1; }
  if (typeof bag[1] != "string") { fails = fails + 1; }
  if (typeof bag[2] != "boolean") { fails = fails + 1; }

  // Enum boxed into `any` reports as "number" (its real runtime shape).
  let c: any = Color.Green;
  if (typeof c != "number") { fails = fails + 1; }

  // A class instance (with virtual dispatch) boxes/typeofs fine even
  // though it's never narrowed back out.
  let d: Dog = { name: "Rex" };
  let anyDog: any = d;
  if (typeof anyDog != "object") { fails = fails + 1; }

  return fails;
}
