// Array sorting utilities for ART.
// Import with: `import { sortAsc, sortDesc, sortBy, isSorted, ... } from "art/sort";`

// Sorts an array of numbers in ascending order using insertion sort.
// Modifies the array in place and returns it for chaining.
export function sortAsc(arr: number[]): number[] {
  let i: number = 1;
  while (i < arr.length) {
    let key: number = arr[i];
    let j: number = i - 1;
    while (j >= 0 && arr[j] > key) {
      arr[j + 1] = arr[j];
      j = j - 1;
    }
    arr[j + 1] = key;
    i = i + 1;
  }
  return arr;
}

// Sorts an array of numbers in descending order using insertion sort.
// Modifies the array in place and returns it for chaining.
export function sortDesc(arr: number[]): number[] {
  let i: number = 1;
  while (i < arr.length) {
    let key: number = arr[i];
    let j: number = i - 1;
    while (j >= 0 && arr[j] < key) {
      arr[j + 1] = arr[j];
      j = j - 1;
    }
    arr[j + 1] = key;
    i = i + 1;
  }
  return arr;
}

// Sorts an array of numbers using a custom comparison function.
// The comparator receives (a, b) and should return:
//   < 0 if a comes before b
//   = 0 if a and b are equivalent
//   > 0 if a comes after b
// Uses insertion sort. Modifies the array in place and returns it.
export function sortBy(arr: number[], comparator: (a: number, b: number) => number): number[] {
  let i: number = 1;
  while (i < arr.length) {
    let key: number = arr[i];
    let j: number = i - 1;
    while (j >= 0 && comparator(arr[j], key) > 0) {
      arr[j + 1] = arr[j];
      j = j - 1;
    }
    arr[j + 1] = key;
    i = i + 1;
  }
  return arr;
}

// Checks if an array is sorted in ascending order.
// Returns true if empty or all elements are in non-decreasing order.
export function isSorted(arr: number[]): boolean {
  if (arr.length <= 1) { return true; }
  let i: number = 1;
  while (i < arr.length) {
    if (arr[i] < arr[i - 1]) { return false; }
    i = i + 1;
  }
  return true;
}

// Checks if an array is sorted in descending order.
// Returns true if empty or all elements are in non-increasing order.
export function isSortedDesc(arr: number[]): boolean {
  if (arr.length <= 1) { return true; }
  let i: number = 1;
  while (i < arr.length) {
    if (arr[i] > arr[i - 1]) { return false; }
    i = i + 1;
  }
  return true;
}

// Reverses an array in place.
// Modifies the array and returns it for chaining.
export function reverse(arr: number[]): number[] {
  let left: number = 0;
  let right: number = arr.length - 1;
  while (left < right) {
    let temp: number = arr[left];
    arr[left] = arr[right];
    arr[right] = temp;
    left = left + 1;
    right = right - 1;
  }
  return arr;
}

// Shuffles an array in place using Fisher-Yates algorithm.
// Note: Uses a simple pseudo-random approach based on array indices and values.
// For better randomness, use with a proper random number generator.
// Modifies the array and returns it for chaining.
export function shuffle(arr: number[]): number[] {
  let i: number = arr.length - 1;
  while (i > 0) {
    // Simple pseudo-random index selection based on current value and position
    let randomIdx: number = i - ((arr[i] + i) - ((arr[i] + i) / (i + 1)) * (i + 1));
    if (randomIdx < 0) { randomIdx = 0; }
    if (randomIdx > i) { randomIdx = i; }

    let temp: number = arr[i];
    arr[i] = arr[randomIdx];
    arr[randomIdx] = temp;
    i = i - 1;
  }
  return arr;
}

// Finds the index of the minimum value in an array.
// Returns -1 if the array is empty.
export function minIndex(arr: number[]): number {
  if (arr.length == 0) { return -1; }
  let minIdx: number = 0;
  let i: number = 1;
  while (i < arr.length) {
    if (arr[i] < arr[minIdx]) {
      minIdx = i;
    }
    i = i + 1;
  }
  return minIdx;
}

// Finds the index of the maximum value in an array.
// Returns -1 if the array is empty.
export function maxIndex(arr: number[]): number {
  if (arr.length == 0) { return -1; }
  let maxIdx: number = 0;
  let i: number = 1;
  while (i < arr.length) {
    if (arr[i] > arr[maxIdx]) {
      maxIdx = i;
    }
    i = i + 1;
  }
  return maxIdx;
}

// Partitions an array around a pivot value.
// Elements less than pivot come before it, elements greater come after.
// Returns the final index of the pivot.
export function partition(arr: number[], left: number, right: number, pivot: number): number {
  let i: number = left;
  let j: number = right;

  while (i <= j) {
    while (arr[i] < pivot && i <= j) {
      i = i + 1;
    }
    while (arr[j] > pivot && i <= j) {
      j = j - 1;
    }
    if (i < j) {
      let temp: number = arr[i];
      arr[i] = arr[j];
      arr[j] = temp;
      i = i + 1;
      j = j - 1;
    } else if (i == j) {
      i = i + 1;
    }
  }
  return i - 1;
}

// Checks if an array contains duplicates.
// Returns true if any value appears more than once.
export function hasDuplicates(arr: number[]): boolean {
  let i: number = 0;
  while (i < arr.length) {
    let j: number = i + 1;
    while (j < arr.length) {
      if (arr[i] == arr[j]) { return true; }
      j = j + 1;
    }
    i = i + 1;
  }
  return false;
}

// Removes duplicates from a sorted array in place.
// Returns the new length of the array.
// Note: Array must be sorted beforehand (use sortAsc first).
export function removeDuplicates(arr: number[]): number {
  if (arr.length == 0) { return 0; }

  let writeIdx: number = 0;
  let i: number = 1;
  while (i < arr.length) {
    if (arr[i] != arr[writeIdx]) {
      writeIdx = writeIdx + 1;
      arr[writeIdx] = arr[i];
    }
    i = i + 1;
  }
  return writeIdx + 1;
}

// Merges two sorted arrays into a single sorted array.
// Both input arrays must be sorted in ascending order.
// Returns a new array without modifying inputs.
export function merge(left: number[], right: number[]): number[] {
  let result: number[] = [];
  let i: number = 0;
  let j: number = 0;

  while (i < left.length && j < right.length) {
    if (left[i] <= right[j]) {
      result = result + [left[i]];
      i = i + 1;
    } else {
      result = result + [right[j]];
      j = j + 1;
    }
  }

  while (i < left.length) {
    result = result + [left[i]];
    i = i + 1;
  }

  while (j < right.length) {
    result = result + [right[j]];
    j = j + 1;
  }

  return result;
}

// Finds the median of an array.
// For even-length arrays, returns the lower middle value.
// Modifies the array (sorts it in place).
export function median(arr: number[]): number {
  if (arr.length == 0) { return 0; }
  sortAsc(arr);
  let mid: number = arr.length / 2;
  let idx: number = 0;
  if (arr.length - ((arr.length / 2) * 2) == 0) {
    idx = mid - 1;
  } else {
    idx = mid;
  }
  return arr[idx];
}

// Finds the kth smallest element in an array (1-indexed).
// k=1 returns the minimum, k=length returns the maximum.
// Uses quickselect algorithm. Modifies the array.
export function kthSmallest(arr: number[], k: number): number {
  if (k < 1 || k > arr.length) { return 0; }
  sortAsc(arr);
  return arr[k - 1];
}

// Counts the number of inversions in an array (pairs where i < j but arr[i] > arr[j]).
// Uses a simple O(n^2) approach.
export function countInversions(arr: number[]): number {
  let count: number = 0;
  let i: number = 0;
  while (i < arr.length) {
    let j: number = i + 1;
    while (j < arr.length) {
      if (arr[i] > arr[j]) {
        count = count + 1;
      }
      j = j + 1;
    }
    i = i + 1;
  }
  return count;
}

// Checks if array is palindromic (reads the same forwards and backwards).
export function isPalindrome(arr: number[]): boolean {
  let left: number = 0;
  let right: number = arr.length - 1;
  while (left < right) {
    if (arr[left] != arr[right]) { return false; }
    left = left + 1;
    right = right - 1;
  }
  return true;
}
