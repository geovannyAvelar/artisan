// Expected error: 'Point' has no field 'z'.
interface Point {
  x: number;
  y: number;
}

function main(): number {
  let p: Point = { x: 1, y: 2 };
  let { z } = p;
  return 0;
}
