import { forEach, some, every, find, findIndex, includes, indexOf, lastIndexOf, reduce, join } from "art/arrays";

function testForEach(): number {
  let count: number = 0;
  let sum: number = 0;
  forEach::<number>([1, 2, 3], function(x: number): void {
    count = count + 1;
    sum = sum + x;
  });
  if (count != 3) { return 1; }
  if (sum != 6) { return 2; }
  return 0;
}

function testSome(): number {
  let arr: number[] = [1, 2, 3, 4, 5];
  let hasEven: boolean = some::<number>(arr, function(x: number): boolean { return x % 2 == 0; });
  if (!hasEven) { return 1; }
  let hasNegative: boolean = some::<number>(arr, function(x: number): boolean { return x < 0; });
  if (hasNegative) { return 2; }
  return 0;
}

function testEvery(): number {
  let arr: number[] = [2, 4, 6, 8];
  let allEven: boolean = every::<number>(arr, function(x: number): boolean { return x % 2 == 0; });
  if (!allEven) { return 1; }
  let allPositive: boolean = every::<number>([1, 2, 3, -4], function(x: number): boolean { return x > 0; });
  if (allPositive) { return 2; }
  return 0;
}

function testFind(): number {
  let arr: number[] = [1, 2, 3, 4, 5];
  let result: number | null = find::<number>(arr, function(x: number): boolean { return x > 3; });
  if (result == null) { return 1; }
  if (result != 4) { return 2; }

  let notFound: number | null = find::<number>(arr, function(x: number): boolean { return x > 10; });
  if (notFound != null) { return 3; }
  return 0;
}

function testFindIndex(): number {
  let arr: number[] = [10, 20, 30, 40];
  let idx: number = findIndex::<number>(arr, function(x: number): boolean { return x > 25; });
  if (idx != 2) { return 1; }

  let notFound: number = findIndex::<number>(arr, function(x: number): boolean { return x > 100; });
  if (notFound != -1) { return 2; }
  return 0;
}

function testIncludes(): number {
  let arr: number[] = [1, 2, 3, 4, 5];
  if (!includes::<number>(arr, 3)) { return 1; }
  if (includes::<number>(arr, 10)) { return 2; }
  return 0;
}

function testIndexOf(): number {
  let arr: number[] = [5, 10, 5, 20, 5];
  if (indexOf::<number>(arr, 5) != 0) { return 1; }
  if (indexOf::<number>(arr, 20) != 3) { return 2; }
  if (indexOf::<number>(arr, 99) != -1) { return 3; }
  return 0;
}

function testLastIndexOf(): number {
  let arr: number[] = [5, 10, 5, 20, 5];
  if (lastIndexOf::<number>(arr, 5) != 4) { return 1; }
  if (lastIndexOf::<number>(arr, 10) != 1) { return 2; }
  if (lastIndexOf::<number>(arr, 99) != -1) { return 3; }
  return 0;
}

function testReduce(): number {
  let arr: number[] = [1, 2, 3, 4];
  let sum: number = reduce::<number, number>(arr, function(acc: number, x: number): number {
    return acc + x;
  }, 0);
  if (sum != 10) { return 1; }

  let product: number = reduce::<number, number>(arr, function(acc: number, x: number): number {
    return acc * x;
  }, 1);
  if (product != 24) { return 2; }
  return 0;
}

function testJoin(): number {
  let arr: number[] = [1, 2, 3, 4];
  let result: string = join(arr, "-");
  if (result != "1-2-3-4") { return 1; }

  let empty: string = join(makeArray::<number>(0, 0), ",");
  if (empty != "") { return 2; }
  return 0;
}

function main(): number {
  if (testForEach() != 0) { return 1; }
  if (testSome() != 0) { return 2; }
  if (testEvery() != 0) { return 3; }
  if (testFind() != 0) { return 4; }
  if (testFindIndex() != 0) { return 5; }
  if (testIncludes() != 0) { return 6; }
  if (testIndexOf() != 0) { return 7; }
  if (testLastIndexOf() != 0) { return 8; }
  if (testReduce() != 0) { return 9; }
  if (testJoin() != 0) { return 10; }
  return 0;
}
