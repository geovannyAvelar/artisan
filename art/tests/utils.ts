import { uuid, hash, isNullish, isTruthy, coalesce, allTrue, anyTrue, when, swap, rotateLeft, rotateRight, zip, unzip, chunk, findIndex, count, groupBy, typeof, range, repeat } from "art/utils";

function testUuid(): number {
  let id1: string = uuid();
  let id2: string = uuid();

  if (id1.length != 36) { return 1; }
  if (id1 == id2) { return 2; }
  if (id1.indexOf("-") != 8) { return 3; }
  return 0;
}

function testHash(): number {
  let h1: number = hash("hello");
  let h2: number = hash("world");
  let h3: number = hash("hello");

  if (h1 == h2) { return 1; }
  if (h1 != h3) { return 2; }
  if (h1 == 0) { return 3; }
  return 0;
}

function testIsNullish(): number {
  if (!isNullish(0)) { return 1; }
  if (isNullish(1)) { return 2; }
  if (isNullish(-5)) { return 3; }
  return 0;
}

function testIsTruthy(): number {
  if (isTruthy(0)) { return 1; }
  if (!isTruthy(1)) { return 2; }
  if (!isTruthy(-5)) { return 3; }
  return 0;
}

function testCoalesce(): number {
  let result: number = coalesce([0, 0, 42, 99]);
  if (result != 42) { return 1; }

  let result2: number = coalesce([0, 0, 0]);
  if (result2 != 0) { return 2; }
  return 0;
}

function testAllTrue(): number {
  if (!allTrue([true, true, true])) { return 1; }
  if (allTrue([true, false, true])) { return 2; }
  if (allTrue([])) { return 3; }
  return 0;
}

function testAnyTrue(): number {
  if (!anyTrue([false, true, false])) { return 1; }
  if (anyTrue([false, false, false])) { return 2; }
  if (!anyTrue([true])) { return 3; }
  return 0;
}

function testWhen(): number {
  if (when(true, 42, 99) != 42) { return 1; }
  if (when(false, 42, 99) != 99) { return 2; }
  return 0;
}

function testSwap(): number {
  let arr: number[] = [1, 2, 3, 4, 5];
  swap(arr, 0, 4);

  if (arr[0] != 5) { return 1; }
  if (arr[4] != 1) { return 2; }
  return 0;
}

function testRotateLeft(): number {
  let arr: number[] = [1, 2, 3, 4, 5];
  let result: number[] = rotateLeft(arr, 2);

  if (result.length != 5) { return 1; }
  if (result[0] != 3) { return 2; }
  if (result[4] != 2) { return 3; }
  return 0;
}

function testRotateRight(): number {
  let arr: number[] = [1, 2, 3, 4, 5];
  let result: number[] = rotateRight(arr, 2);

  if (result.length != 5) { return 1; }
  if (result[0] != 4) { return 2; }
  if (result[1] != 5) { return 3; }
  return 0;
}

function testZip(): number {
  let arr1: number[] = [1, 2, 3];
  let arr2: number[] = [4, 5, 6];
  let result: [number, number][] = zip(arr1, arr2);

  if (result.length != 3) { return 1; }
  if (result[0][0] != 1 || result[0][1] != 4) { return 2; }
  if (result[2][0] != 3 || result[2][1] != 6) { return 3; }
  return 0;
}

function testZipUnequal(): number {
  let arr1: number[] = [1, 2];
  let arr2: number[] = [3, 4, 5];
  let result: [number, number][] = zip(arr1, arr2);

  if (result.length != 2) { return 1; }
  return 0;
}

function testUnzip(): number {
  let pairs: [number, number][] = [[1, 4], [2, 5], [3, 6]];
  let result: [number[], number[]] = unzip(pairs);

  if (result[0].length != 3) { return 1; }
  if (result[1].length != 3) { return 2; }
  if (result[0][0] != 1) { return 3; }
  if (result[1][2] != 6) { return 4; }
  return 0;
}

function testChunk(): number {
  let arr: number[] = [1, 2, 3, 4, 5];
  let result: number[][] = chunk(arr, 2);

  if (result.length != 3) { return 1; }
  if (result[0].length != 2) { return 2; }
  if (result[2].length != 1) { return 3; }
  if (result[2][0] != 5) { return 4; }
  return 0;
}

function testChunkZero(): number {
  let result: number[][] = chunk([1, 2, 3], 0);
  if (result.length != 0) { return 1; }
  return 0;
}

function testFindIndex(): number {
  let arr: number[] = [1, 2, 3, 4, 5];
  let idx: number = findIndex(arr, function(v: number): boolean { return v == 3; });

  if (idx != 2) { return 1; }

  let idx2: number = findIndex(arr, function(v: number): boolean { return v == 99; });
  if (idx2 != -1) { return 2; }
  return 0;
}

function testCount(): number {
  let arr: number[] = [1, 2, 3, 4, 5];
  let result: number = count(arr, function(v: number): boolean {
    return v - ((v / 2) * 2) == 0;
  });

  if (result != 2) { return 1; }
  return 0;
}

function testGroupBy(): number {
  let arr: number[] = [1, 2, 3, 4, 5];
  let groups: [key: string, values: number[]][] = groupBy(arr, function(v: number): string {
    if (v - ((v / 2) * 2) == 0) { return "even"; }
    return "odd";
  });

  if (groups.length != 2) { return 1; }
  return 0;
}

function testTypeof(): number {
  let t1: string = typeof(42);
  if (t1 != "number") { return 1; }

  let t2: string = typeof(0);
  if (t2 != "null") { return 2; }
  return 0;
}

function testRange(): number {
  let result: number[] = range(0, 5, 1);

  if (result.length != 5) { return 1; }
  if (result[0] != 0) { return 2; }
  if (result[4] != 4) { return 3; }
  return 0;
}

function testRangeNegative(): number {
  let result: number[] = range(5, 0, -1);

  if (result.length != 5) { return 1; }
  if (result[0] != 5) { return 2; }
  if (result[4] != 1) { return 3; }
  return 0;
}

function testRangeStep(): number {
  let result: number[] = range(0, 10, 2);

  if (result.length != 5) { return 1; }
  if (result[0] != 0) { return 2; }
  if (result[4] != 8) { return 3; }
  return 0;
}

function testRepeat(): number {
  let result: number[] = repeat(42, 3);

  if (result.length != 3) { return 1; }
  if (result[0] != 42) { return 2; }
  if (result[2] != 42) { return 3; }
  return 0;
}

function testRepeatZero(): number {
  let result: number[] = repeat(42, 0);
  if (result.length != 0) { return 1; }
  return 0;
}
