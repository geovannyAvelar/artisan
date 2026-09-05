// Expected error: '++'/'--' on a readonly field is still an assignment
// under the hood - rejected the same way a plain '=' is.
interface Counter {
  readonly count: number;
}

function main(): number {
  let c: Counter = { count: 0 };
  c.count++;
  return 0;
}
