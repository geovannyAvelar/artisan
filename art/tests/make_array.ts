// makeArray<T>: runtime-sized array allocation, for number/string/struct
// element types (each a real, separately-generated instantiation).

interface Point { x: number; y: number; }

function main(): number {
  let fails: number = 0;

  let zeros: number[] = makeArray::<number>(5, 0);
  if (zeros.length != 5) { fails = fails + 1; }
  let sum: number = 0;
  for (let x of zeros) { sum = sum + x; }
  if (sum != 0) { fails = fails + 1; }

  let empty: number[] = makeArray::<number>(0, 0);
  if (empty.length != 0) { fails = fails + 1; }

  let filled: string[] = makeArray::<string>(3, "x");
  if (filled.length != 3 || filled[0] != "x" || filled[2] != "x") { fails = fails + 1; }

  let points: Point[] = makeArray::<Point>(2, { x: 1, y: 2 });
  points[0] = { x: 10, y: 20 };
  if (points[0].x != 10 || points[1].x != 1) { fails = fails + 1; }

  return fails;
}
