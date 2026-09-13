import { sortAsc, sortDesc, sortBy, isSorted, isSortedDesc, reverse, shuffle, minIndex, maxIndex, partition, hasDuplicates, removeDuplicates, merge, median, kthSmallest, countInversions, isPalindrome } from "art/sort";

function testSortAsc(): number {
  let arr: number[] = [3, 1, 4, 1, 5];
  sortAsc(arr);
  if (arr[0] != 1) { return 1; }
  if (arr[1] != 1) { return 2; }
  if (arr[2] != 3) { return 3; }
  if (arr[3] != 4) { return 4; }
  if (arr[4] != 5) { return 5; }
  return 0;
}

function testSortDesc(): number {
  let arr: number[] = [3, 1, 4, 1, 5];
  sortDesc(arr);
  if (arr[0] != 5) { return 1; }
  if (arr[1] != 4) { return 2; }
  if (arr[2] != 3) { return 3; }
  if (arr[3] != 1) { return 4; }
  if (arr[4] != 1) { return 5; }
  return 0;
}

function testSortBy(): number {
  let arr: number[] = [3, 1, 4, 1, 5];
  sortBy(arr, function(a: number, b: number): number {
    if (a < b) { return -1; }
    if (a > b) { return 1; }
    return 0;
  });
  if (arr[0] != 1) { return 1; }
  if (arr[4] != 5) { return 2; }
  return 0;
}

function testIsSorted(): number {
  if (!isSorted([1, 2, 3, 4, 5])) { return 1; }
  if (isSorted([5, 4, 3, 2, 1])) { return 2; }
  if (!isSorted([1, 1, 2, 2, 3])) { return 3; }
  if (!isSorted([])) { return 4; }
  if (!isSorted([1])) { return 5; }
  return 0;
}

function testIsSortedDesc(): number {
  if (!isSortedDesc([5, 4, 3, 2, 1])) { return 1; }
  if (isSortedDesc([1, 2, 3, 4, 5])) { return 2; }
  if (!isSortedDesc([5, 5, 4, 4, 3])) { return 3; }
  if (!isSortedDesc([])) { return 4; }
  return 0;
}

function testReverse(): number {
  let arr: number[] = [1, 2, 3, 4, 5];
  reverse(arr);
  if (arr[0] != 5) { return 1; }
  if (arr[4] != 1) { return 2; }
  if (arr[2] != 3) { return 3; }
  return 0;
}

function testMinMaxIndex(): number {
  let arr: number[] = [3, 1, 4, 1, 5, 9, 2, 6];
  if (minIndex(arr) != 1) { return 1; }  // First occurrence of 1 at index 1
  if (maxIndex(arr) != 5) { return 2; }  // Maximum 9 at index 5
  if (minIndex([]) != -1) { return 3; }
  if (maxIndex([]) != -1) { return 4; }
  return 0;
}

function testHasDuplicates(): number {
  if (!hasDuplicates([1, 2, 3, 1])) { return 1; }
  if (hasDuplicates([1, 2, 3, 4, 5])) { return 2; }
  if (!hasDuplicates([])) { return 3; }
  if (!hasDuplicates([1])) { return 4; }
  if (!hasDuplicates([5, 5])) { return 5; }
  return 0;
}

function testRemoveDuplicates(): number {
  let arr: number[] = [1, 1, 2, 2, 3, 3, 3, 4];
  let newLen: number = removeDuplicates(arr);
  if (newLen != 4) { return 1; }
  if (arr[0] != 1) { return 2; }
  if (arr[1] != 2) { return 3; }
  if (arr[2] != 3) { return 4; }
  if (arr[3] != 4) { return 5; }
  return 0;
}

function testMerge(): number {
  let left: number[] = [1, 3, 5];
  let right: number[] = [2, 4, 6];
  let merged: number[] = merge(left, right);
  if (merged.length != 6) { return 1; }
  if (merged[0] != 1) { return 2; }
  if (merged[1] != 2) { return 3; }
  if (merged[5] != 6) { return 4; }
  return 0;
}

function testMedian(): number {
  let arr1: number[] = [3, 1, 4, 1, 5];
  if (median(arr1) != 3) { return 1; }

  let arr2: number[] = [1, 2, 3, 4];
  if (median(arr2) != 2) { return 2; }

  return 0;
}

function testKthSmallest(): number {
  let arr: number[] = [3, 1, 4, 1, 5, 9, 2, 6];
  if (kthSmallest(arr, 1) != 1) { return 1; }  // Minimum
  if (kthSmallest(arr, 3) != 2) { return 2; }
  if (kthSmallest(arr, 8) != 9) { return 3; }  // Maximum
  if (kthSmallest(arr, 0) != 0) { return 4; }  // Out of bounds
  if (kthSmallest(arr, 10) != 0) { return 5; }  // Out of bounds
  return 0;
}

function testCountInversions(): number {
  if (countInversions([1, 2, 3, 4, 5]) != 0) { return 1; }
  if (countInversions([5, 4, 3, 2, 1]) != 10) { return 2; }
  if (countInversions([3, 1, 2]) != 2) { return 3; }
  if (countInversions([]) != 0) { return 4; }
  return 0;
}

function testIsPalindrome(): number {
  if (!isPalindrome([1, 2, 3, 2, 1])) { return 1; }
  if (!isPalindrome([1, 2, 2, 1])) { return 2; }
  if (isPalindrome([1, 2, 3, 4, 5])) { return 3; }
  if (!isPalindrome([5])) { return 4; }
  if (!isPalindrome([])) { return 5; }
  return 0;
}

function testSortingEdgeCases(): number {
  let empty: number[] = [];
  sortAsc(empty);
  if (empty.length != 0) { return 1; }

  let single: number[] = [42];
  sortAsc(single);
  if (single[0] != 42) { return 2; }

  let duplicates: number[] = [5, 5, 5, 5];
  sortAsc(duplicates);
  if (duplicates[0] != 5 || duplicates[3] != 5) { return 3; }

  let negative: number[] = [-3, -1, -5, 0, 2];
  sortAsc(negative);
  if (negative[0] != -5 || negative[4] != 2) { return 4; }

  return 0;
}
