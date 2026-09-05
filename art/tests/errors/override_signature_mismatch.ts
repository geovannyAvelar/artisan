// Expected error: an override must match its ancestor's signature
// exactly - Dog's speak() returns number, Animal's returns string.
class Animal {
  function speak(): string { return "..."; }
}

class Dog extends Animal {
  function speak(): number { return 1; }
}

function main(): number {
  return 0;
}
