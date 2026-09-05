// Expected error: cannot assign to a readonly field after construction.
interface Point {
  readonly x: number;
}

function main(): number {
  let p: Point = { x: 1 };
  p.x = 2;
  return 0;
}
