// Expected error: a class can't extend a 'declare class' - it has no
// accessible fields to inherit.
declare class Base {
  function ping(): void {}
}

class Derived extends Base {
  x: number;
}

function main(): number {
  return 0;
}
