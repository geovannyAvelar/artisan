// Expected error: 'Counter' has no static member 'missing'.
class Counter {
  static total: number = 0;
}

function main(): number {
  return Counter.missing;
}
