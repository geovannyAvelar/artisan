// `class Dog extends Animal { ... }` - single inheritance, classes only.
// Real virtual dispatch (a vtable) for any class actually touched by an
// extends relationship - a method called through a base-typed reference
// reaches the actual runtime instance's own override, not just whatever
// was statically visible at the call site. See art/README.md's own
// "Inheritance" notes for the accepted v1 limits (no downcasting/
// instanceof, no field shadowing, accessors inherit but don't override,
// statics don't inherit).

class Animal {
  name: string;
  function speak(): string { return this.name + " makes a sound"; }
  function describe(): string { return "I am " + this.name; }
}

class Dog extends Animal {
  breed: string;
  function speak(): string { return this.name + " barks"; }
}

class Puppy extends Dog {
  age: number;
  function speak(): string { return this.name + ": yip (age " + numberToString(this.age) + ")"; }
  function isYoung(): boolean { return this.age < 1; }
}

function makeSound(a: Animal): string { return a.speak(); }
function describeIt(a: Animal): string { return a.describe(); }
function dogSound(d: Dog): string { return d.speak(); }

class WithAccessor {
  raw: number;
  get value(): number { return this.raw; }
  set value(v: number) { this.raw = v; }
}

class InheritsAccessor extends WithAccessor {
  bark: string;
}

function main(): number {
  let fails: number = 0;

  let animal: Animal = { name: "Generic" };
  let dog: Dog = { name: "Rex", breed: "Lab" };
  let puppy: Puppy = { name: "Buddy", breed: "Lab", age: 0 };

  // direct calls, no polymorphism involved
  if (dog.speak() != "Rex barks") { fails = fails + 1; }
  if (animal.speak() != "Generic makes a sound") { fails = fails + 1; }
  if (puppy.speak() != "Buddy: yip (age 0)") { fails = fails + 1; }

  // the critical case: dispatch through a base-typed parameter must
  // reach the ACTUAL instance's own override, not the parameter's
  // static type's implementation
  if (makeSound(dog) != "Rex barks") { fails = fails + 1; }
  if (makeSound(animal) != "Generic makes a sound") { fails = fails + 1; }
  if (makeSound(puppy) != "Buddy: yip (age 0)") { fails = fails + 1; } // 2 levels up
  if (dogSound(puppy) != "Buddy: yip (age 0)") { fails = fails + 1; } // 1 level up

  // a method NOT overridden anywhere in the chain still resolves
  // correctly through however many levels of base-typed reference
  if (describeIt(dog) != "I am Rex") { fails = fails + 1; }
  if (describeIt(puppy) != "I am Buddy") { fails = fails + 1; }

  // upcasting into a plain variable
  let asAnimal: Animal = puppy;
  if (asAnimal.speak() != "Buddy: yip (age 0)") { fails = fails + 1; }

  // inherited fields, through one and two levels
  if (dog.name != "Rex") { fails = fails + 1; }
  if (puppy.name != "Buddy") { fails = fails + 1; }
  if (!puppy.isYoung()) { fails = fails + 1; }

  // get/set accessors inherit (though they can't be overridden yet)
  let acc: InheritsAccessor = { raw: 0, bark: "woof" };
  acc.value = 42;
  if (acc.value != 42) { fails = fails + 1; }

  return fails;
}
