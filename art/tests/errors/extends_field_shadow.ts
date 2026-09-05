// Expected error: 'name' is already a field of 'Animal' - 'Dog' can't
// redeclare it (no field shadowing).
class Animal {
  name: string;
}

class Dog extends Animal {
  name: string;
}

function main(): number {
  return 0;
}
