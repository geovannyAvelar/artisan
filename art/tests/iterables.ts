import { range, repeat, filter, map, take, skip, findAll, partition, pairs, group, interleave, concat, flatten, cycle, reverse, zipMany, iterate, generate, any, all, find, countWhere } from "art/iterables";

function testRange(): number {
  let r: number[] = range(0, 5, 1);
  if (r.length != 5) { return 1; }
  if (r[0] != 0) { return 2; }
  if (r[4] != 4) { return 3; }
  return 0;
}

function testRangeWithStep(): number {
  let r: number[] = range(0, 10, 2);
  if (r.length != 5) { return 1; }
  if (r[0] != 0) { return 2; }
  if (r[4] != 8) { return 3; }
  return 0;
}

function testRepeat(): number {
  let r: number[] = repeat::<number>(42, 3);
  if (r.length != 3) { return 1; }
  if (r[0] != 42) { return 2; }
  if (r[2] != 42) { return 3; }
  return 0;
}

function testFilter(): number {
  let arr: number[] = [1, 2, 3, 4, 5];
  let evens: number[] = filter::<number>(arr, function(x: number): boolean {
    return x % 2 == 0;
  });

  if (evens.length != 2) { return 1; }
  if (evens[0] != 2) { return 2; }
  if (evens[1] != 4) { return 3; }
  return 0;
}

function testMap(): number {
  let arr: number[] = [1, 2, 3];
  let doubled: number[] = map::<number, number>(arr, function(x: number): number {
    return x * 2;
  });

  if (doubled.length != 3) { return 1; }
  if (doubled[0] != 2) { return 2; }
  if (doubled[2] != 6) { return 3; }
  return 0;
}

function testTake(): number {
  let arr: number[] = [1, 2, 3, 4, 5];
  let taken: number[] = take::<number>(arr, 3);

  if (taken.length != 3) { return 1; }
  if (taken[2] != 3) { return 2; }
  return 0;
}

function testSkip(): number {
  let arr: number[] = [1, 2, 3, 4, 5];
  let skipped: number[] = skip::<number>(arr, 2);

  if (skipped.length != 3) { return 1; }
  if (skipped[0] != 3) { return 2; }
  return 0;
}

function testFindAll(): number {
  let arr: number[] = [1, 2, 3, 4, 5, 6];
  let evens: number[] = findAll::<number>(arr, function(x: number): boolean {
    return x % 2 == 0;
  });

  if (evens.length != 3) { return 1; }
  return 0;
}

function testPartition(): number {
  let arr: number[] = [1, 2, 3, 4, 5];
  let parts: [number[], number[]] = partition::<number>(arr, function(x: number): boolean {
    return x % 2 == 0;
  });

  if (parts[0].length != 2) { return 1; }
  if (parts[1].length != 3) { return 2; }
  return 0;
}

function testPairs(): number {
  let arr: number[] = [1, 2, 3, 4];
  let p: [number, number][] = pairs::<number>(arr);

  if (p.length != 3) { return 1; }
  if (p[0][0] != 1 || p[0][1] != 2) { return 2; }
  if (p[2][0] != 3 || p[2][1] != 4) { return 3; }
  return 0;
}

function testGroup(): number {
  let arr: number[] = [1, 1, 2, 2, 2, 3, 1];
  let groups: number[][] = group::<number>(arr, function(a: number, b: number): boolean {
    return a == b;
  });

  if (groups.length != 4) { return 1; }
  if (groups[0].length != 2) { return 2; }
  if (groups[1].length != 3) { return 3; }
  return 0;
}

function testInterleave(): number {
  let arr1: number[] = [1, 2, 3];
  let arr2: number[] = [10, 20, 30];
  let result: number[] = interleave::<number>(arr1, arr2);

  if (result.length != 6) { return 1; }
  if (result[0] != 1) { return 2; }
  if (result[1] != 10) { return 3; }
  return 0;
}

function testConcat(): number {
  let arrays: number[][] = [[1, 2], [3, 4], [5]];
  let result: number[] = concat::<number>(arrays);

  if (result.length != 5) { return 1; }
  if (result[2] != 3) { return 2; }
  return 0;
}

function testFlatten(): number {
  let arr: number[][] = [[1, 2], [3, 4], [5]];
  let result: number[] = flatten::<number>(arr);

  if (result.length != 5) { return 1; }
  if (result[0] != 1) { return 2; }
  if (result[4] != 5) { return 3; }
  return 0;
}

function testCycle(): number {
  let arr: number[] = [1, 2];
  let result: number[] = cycle::<number>(arr, 3);

  if (result.length != 6) { return 1; }
  if (result[0] != 1) { return 2; }
  if (result[5] != 2) { return 3; }
  return 0;
}

function testReverse(): number {
  let arr: number[] = [1, 2, 3, 4, 5];
  let result: number[] = reverse::<number>(arr);

  if (result.length != 5) { return 1; }
  if (result[0] != 5) { return 2; }
  if (result[4] != 1) { return 3; }
  return 0;
}

function testZipMany(): number {
  let arrays: number[][] = [[1, 2, 3], [10, 20, 30], [100, 200, 300]];
  let result: number[][] = zipMany::<number>(arrays);

  if (result.length != 3) { return 1; }
  if (result[0].length != 3) { return 2; }
  return 0;
}

function testIterate(): number {
  let result: number[] = iterate(1, function(x: number): boolean { return x <= 5; }, function(x: number): number { return x + 1; });

  if (result.length != 5) { return 1; }
  if (result[0] != 1) { return 2; }
  if (result[4] != 5) { return 3; }
  return 0;
}

function testGenerate(): number {
  let result: number[] = generate(2, function(x: number): number { return x * 2; }, 4);

  if (result.length != 4) { return 1; }
  if (result[0] != 2) { return 2; }
  if (result[3] != 16) { return 3; }
  return 0;
}

function testAny(): number {
  let arr: number[] = [1, 2, 3, 4, 5];
  let hasEven: boolean = any::<number>(arr, function(x: number): boolean { return x % 2 == 0; });

  if (!hasEven) { return 1; }

  let hasNegative: boolean = any::<number>(arr, function(x: number): boolean { return x < 0; });
  if (hasNegative) { return 2; }
  return 0;
}

function testAll(): number {
  let arr: number[] = [2, 4, 6, 8];
  let allEven: boolean = all::<number>(arr, function(x: number): boolean { return x % 2 == 0; });

  if (!allEven) { return 1; }

  let arr2: number[] = [1, 2, 3, 4];
  let all2Even: boolean = all::<number>(arr2, function(x: number): boolean { return x % 2 == 0; });
  if (all2Even) { return 2; }
  return 0;
}

function testFind(): number {
  let arr: number[] = [1, 2, 3, 4, 5];
  let found: number = find::<number>(arr, function(x: number): boolean { return x > 3; });

  if (found != 4) { return 1; }
  return 0;
}

function testCountWhere(): number {
  let arr: number[] = [1, 2, 3, 4, 5, 6];
  let count: number = countWhere::<number>(arr, function(x: number): boolean { return x % 2 == 0; });

  if (count != 3) { return 1; }
  return 0;
}

function testForOfWithIterable(): number {
  let arr: number[] = range(1, 4, 1);
  let sum: number = 0;

  for (let x of arr) {
    sum = sum + x;
  }

  if (sum != 6) { return 1; }
  return 0;
}

function testForOfWithFiltered(): number {
  let arr: number[] = [1, 2, 3, 4, 5];
  let evens: number[] = filter::<number>(arr, function(x: number): boolean { return x % 2 == 0; });
  let sum: number = 0;

  for (let x of evens) {
    sum = sum + x;
  }

  if (sum != 6) { return 1; }
  return 0;
}

function testForOfWithMapped(): number {
  let arr: number[] = [1, 2, 3];
  let doubled: number[] = map::<number, number>(arr, function(x: number): number { return x * 2; });
  let sum: number = 0;

  for (let x of doubled) {
    sum = sum + x;
  }

  if (sum != 12) { return 1; }
  return 0;
}
