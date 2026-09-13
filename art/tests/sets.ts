import { union, intersection, difference, symmetricDifference, isSubset, isSuperset, disjoint, setEquals, unique, hasDuplicates, cartesianProduct, complement, countCommon, jaccardSimilarity } from "art/sets";

function testUnion(): number {
  let a: number[] = [1, 2, 3];
  let b: number[] = [3, 4, 5];
  let result: number[] = union(a, b);

  if (result.length != 5) { return 1; }
  if (result[0] != 1) { return 2; }
  if (result[4] != 5) { return 3; }
  return 0;
}

function testIntersection(): number {
  let a: number[] = [1, 2, 3, 4];
  let b: number[] = [3, 4, 5, 6];
  let result: number[] = intersection(a, b);

  if (result.length != 2) { return 1; }
  if (result[0] != 3) { return 2; }
  if (result[1] != 4) { return 3; }
  return 0;
}

function testDifference(): number {
  let a: number[] = [1, 2, 3, 4, 5];
  let b: number[] = [3, 4, 5, 6];
  let result: number[] = difference(a, b);

  if (result.length != 2) { return 1; }
  if (result[0] != 1) { return 2; }
  if (result[1] != 2) { return 3; }
  return 0;
}

function testSymmetricDifference(): number {
  let a: number[] = [1, 2, 3];
  let b: number[] = [3, 4, 5];
  let result: number[] = symmetricDifference(a, b);

  if (result.length != 4) { return 1; }  // 1, 2, 4, 5
  return 0;
}

function testIsSubset(): number {
  if (!isSubset([1, 2], [1, 2, 3])) { return 1; }
  if (isSubset([1, 4], [1, 2, 3])) { return 2; }
  if (!isSubset([], [1, 2, 3])) { return 3; }
  if (!isSubset([1, 2, 3], [1, 2, 3])) { return 4; }
  return 0;
}

function testIsSuperset(): number {
  if (!isSuperset([1, 2, 3], [1, 2])) { return 1; }
  if (isSuperset([1, 2], [1, 2, 3])) { return 2; }
  if (!isSuperset([1, 2, 3], [])) { return 3; }
  if (!isSuperset([1, 2, 3], [1, 2, 3])) { return 4; }
  return 0;
}

function testDisjoint(): number {
  if (!disjoint([1, 2, 3], [4, 5, 6])) { return 1; }
  if (disjoint([1, 2, 3], [3, 4, 5])) { return 2; }
  if (!disjoint([], [1, 2, 3])) { return 3; }
  if (!disjoint([1, 2], [])) { return 4; }
  return 0;
}

function testSetEquals(): number {
  if (!setEquals([1, 2, 3], [1, 2, 3])) { return 1; }
  if (!setEquals([1, 2, 3], [3, 2, 1])) { return 2; }
  if (setEquals([1, 2, 3], [1, 2])) { return 3; }
  if (!setEquals([1, 1, 2], [1, 2])) { return 4; }  // Duplicates ignored
  return 0;
}

function testUnique(): number {
  let result: number[] = unique([1, 2, 2, 3, 3, 3, 4]);
  if (result.length != 4) { return 1; }
  if (result[0] != 1) { return 2; }
  if (result[3] != 4) { return 3; }
  return 0;
}

function testHasDuplicates(): number {
  if (!hasDuplicates([1, 2, 3, 2])) { return 1; }
  if (hasDuplicates([1, 2, 3, 4])) { return 2; }
  if (!hasDuplicates([])) { return 3; }
  if (!hasDuplicates([5, 5])) { return 4; }
  return 0;
}

function testCartesianProduct(): number {
  let result: number[] = cartesianProduct([1, 2], [3, 4]);

  if (result.length != 8) { return 1; }  // 4 pairs * 2 elements each
  if (result[0] != 1) { return 2; }  // First pair (1, 3)
  if (result[1] != 3) { return 3; }
  if (result[2] != 1) { return 4; }  // Second pair (1, 4)
  if (result[3] != 4) { return 5; }
  if (result[4] != 2) { return 6; }  // Third pair (2, 3)
  if (result[5] != 3) { return 7; }

  return 0;
}

function testComplement(): number {
  let result: number[] = complement([1, 2], [1, 2, 3, 4, 5]);
  if (result.length != 3) { return 1; }
  return 0;
}

function testCountCommon(): number {
  if (countCommon([1, 2, 3], [2, 3, 4]) != 2) { return 1; }
  if (countCommon([1, 2, 3], [4, 5, 6]) != 0) { return 2; }
  if (countCommon([1, 2, 3], [1, 2, 3]) != 3) { return 3; }
  return 0;
}

function testJaccardSimilarity(): number {
  let sim1: number = jaccardSimilarity([1, 2, 3], [1, 2, 3]);
  if (sim1 < 0.99 || sim1 > 1.01) { return 1; }  // Should be 1.0

  let sim2: number = jaccardSimilarity([1, 2], [3, 4]);
  if (sim2 > 0.01) { return 2; }  // Should be 0.0 (disjoint)

  let sim3: number = jaccardSimilarity([1, 2, 3], [2, 3, 4]);
  if (sim3 < 0.3 || sim3 > 0.7) { return 3; }  // Should be around 0.5

  return 0;
}

function testSetOperationsWithEmpty(): number {
  let empty: number[] = [];
  let arr: number[] = [1, 2, 3];

  if (union(empty, arr).length != 3) { return 1; }
  if (intersection(empty, arr).length != 0) { return 2; }
  if (difference(arr, empty).length != 3) { return 3; }
  if (difference(empty, arr).length != 0) { return 4; }

  return 0;
}

function testSetOperationsWithDuplicates(): number {
  let a: number[] = [1, 1, 2, 2, 3];
  let b: number[] = [2, 2, 3, 3, 4];

  let inter: number[] = intersection(a, b);
  if (inter.length != 2) { return 1; }  // 2 and 3, no duplicates

  let uni: number[] = union(a, b);
  if (uni.length != 4) { return 2; }  // 1, 2, 3, 4, no duplicates

  return 0;
}

function testSetChaining(): number {
  let a: number[] = [1, 2, 3];
  let b: number[] = [3, 4, 5];
  let c: number[] = [5, 6, 7];

  // (a ∪ b) ∩ c
  let step1: number[] = union(a, b);
  let result: number[] = intersection(step1, c);

  if (result.length != 1) { return 1; }  // Should be [5]
  if (result[0] != 5) { return 2; }

  return 0;
}
