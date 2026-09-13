import { map, filter, reverse, slice, flat, fill, concat, isEmpty, first, last, unique } from "art/arrays";

function testMap(): number {
  let arr: number[] = [1, 2, 3];
  let result: number[] = map::<number, number>(arr, function(x: number): number { return x * 2; });
  if (result.length != 3) { return 1; }
  if (result[0] != 2 || result[1] != 4 || result[2] != 6) { return 2; }
  return 0;
}

function testFilter(): number {
  let arr: number[] = [1, 2, 3, 4, 5];
  let result: number[] = filter::<number>(arr, function(x: number): boolean { return x > 2; });
  if (result.length != 3) { return 1; }
  if (result[0] != 3 || result[1] != 4 || result[2] != 5) { return 2; }
  return 0;
}

function testReverse(): number {
  let arr: number[] = [1, 2, 3];
  let result: number[] = reverse::<number>(arr);
  if (result.length != 3) { return 1; }
  if (result[0] != 3 || result[1] != 2 || result[2] != 1) { return 2; }
  return 0;
}

function testSlice(): number {
  let arr: number[] = [1, 2, 3, 4, 5];
  let result: number[] = slice::<number>(arr, 1, 4);
  if (result.length != 3) { return 1; }
  if (result[0] != 2 || result[1] != 3 || result[2] != 4) { return 2; }
  return 0;
}

function testFlat(): number {
  let arr: number[][] = [[1, 2], [3, 4], [5]];
  let result: number[] = flat::<number>(arr);
  if (result.length != 5) { return 1; }
  if (result[0] != 1 || result[4] != 5) { return 2; }
  return 0;
}

function testFill(): number {
  let arr: number[] = [1, 2, 3, 4, 5];
  fill::<number>(arr, 0, 1, 3);
  if (arr[0] != 1) { return 1; }
  if (arr[1] != 0 || arr[2] != 0) { return 2; }
  if (arr[3] != 4) { return 3; }
  return 0;
}

function testConcat(): number {
  let arr1: number[] = [1, 2];
  let arr2: number[] = [3, 4];
  let result: number[] = concat::<number>(arr1, arr2);
  if (result.length != 4) { return 1; }
  if (result[2] != 3 || result[3] != 4) { return 2; }
  return 0;
}

function testIsEmpty(): number {
  let empty: number[] = [];
  let nonempty: number[] = [1];
  if (!isEmpty::<number>(empty)) { return 1; }
  if (isEmpty::<number>(nonempty)) { return 2; }
  return 0;
}

function testFirst(): number {
  let arr: number[] = [5, 4, 3];
  let f: number | null = first::<number>(arr);
  if (f == null) { return 1; }
  if (f != 5) { return 2; }

  let empty: number[] = [];
  let ef: number | null = first::<number>(empty);
  if (ef != null) { return 3; }
  return 0;
}

function testLast(): number {
  let arr: number[] = [1, 2, 3];
  let l: number | null = last::<number>(arr);
  if (l == null) { return 1; }
  if (l != 3) { return 2; }
  return 0;
}

function testUnique(): number {
  let arr: number[] = [1, 2, 2, 3, 3, 3];
  let result: number[] = unique::<number>(arr);
  if (result.length != 3) { return 1; }
  if (result[0] != 1 || result[1] != 2 || result[2] != 3) { return 2; }
  return 0;
}
