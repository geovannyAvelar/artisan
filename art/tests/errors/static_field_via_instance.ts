// Expected error: a static field isn't part of any instance - it can
// only be reached as `ClassName.total`, never `instance.total`.
class Counter {
  static total: number = 0;
  count: number;
}

function main(): number {
  let c: Counter = { count: 0 };
  return c.total;
}
