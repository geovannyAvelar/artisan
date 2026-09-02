// Multi-file import/export: this entry point pulls in ./math, proving
// cross-file name resolution, visibility (helper() in math.ts stays
// private), and a struct type declared in one file used in another.

import { add, Point, distanceSquared } from "./math";

function main(): number {
  let fails: number = 0;

  if (add(2, 3) != 5) { fails = fails + 1; }

  let p: Point = { x: 3, y: 4 };
  if (distanceSquared(p) != 25) { fails = fails + 1; }

  return fails;
}
