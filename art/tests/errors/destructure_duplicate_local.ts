// Expected error: 'x' is already bound by this destructuring pattern
// (both fields renamed to the same local name).
interface Point {
  x: number;
  y: number;
}

function main(): number {
  let p: Point = { x: 1, y: 2 };
  let { x: dup, y: dup } = p;
  return 0;
}
