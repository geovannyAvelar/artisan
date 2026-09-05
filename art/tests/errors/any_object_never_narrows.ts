interface Point { x: number; y: number; }

function useIt(x: any): number {
  if (typeof x == "object") {
    return x.x; // "object" never narrows (see art/README.md) - x is still `any` here, no `.x` to access
  }
  return 0;
}

function main(): number {
  let p: Point = { x: 1, y: 2 };
  return useIt(p);
}
