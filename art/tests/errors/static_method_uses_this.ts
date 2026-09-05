// Expected error: a static method has no implicit `this` receiver at
// all - referencing `this` inside one is just an undefined identifier.
class Counter {
  count: number;

  static function bad(): number {
    return this.count;
  }
}

function main(): number {
  return 0;
}
