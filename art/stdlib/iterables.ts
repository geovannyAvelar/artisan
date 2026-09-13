// Iterables module for ART. Import with: `import { range, repeat, filter, map, ... } from "art/iterables";`
// Provides utilities for creating and working with iterables that work with for...of loops.

// Creates a range of numbers as an array (iterable with for...of).
export function range(start: number, end: number, step: number): number[] {
  if (step == 0) { return []; }

  let result: number[] = [];

  if (step > 0) {
    while (start < end) {
      result = result + [start];
      start = start + step;
    }
  } else {
    while (start > end) {
      result = result + [start];
      start = start + step;
    }
  }

  return result;
}

// Repeats a value n times (creates an iterable array).
export function repeat<T>(value: T, times: number): T[] {
  let result: T[] = [];
  let i: number = 0;

  while (i < times) {
    result = result + [value];
    i = i + 1;
  }

  return result;
}

// Filters an array based on a predicate (usable with for...of).
export function filter<T>(arr: T[], predicate: (item: T) => boolean): T[] {
  let result: T[] = [];
  let i: number = 0;

  while (i < arr.length) {
    if (predicate(arr[i])) {
      result = result + [arr[i]];
    }
    i = i + 1;
  }

  return result;
}

// Maps over an array (usable with for...of).
export function map<T, U>(arr: T[], fn: (item: T) => U): U[] {
  let result: U[] = [];
  let i: number = 0;

  while (i < arr.length) {
    result = result + [fn(arr[i])];
    i = i + 1;
  }

  return result;
}

// Takes the first n elements of an array.
export function take<T>(arr: T[], n: number): T[] {
  if (n <= 0) { return []; }

  let result: T[] = [];
  let i: number = 0;

  while (i < n && i < arr.length) {
    result = result + [arr[i]];
    i = i + 1;
  }

  return result;
}

// Skips the first n elements of an array.
export function skip<T>(arr: T[], n: number): T[] {
  if (n <= 0) { return arr; }

  let result: T[] = [];
  let i: number = n;

  while (i < arr.length) {
    result = result + [arr[i]];
    i = i + 1;
  }

  return result;
}

// Finds all elements matching a predicate.
export function findAll<T>(arr: T[], predicate: (item: T) => boolean): T[] {
  let result: T[] = [];
  let i: number = 0;

  while (i < arr.length) {
    if (predicate(arr[i])) {
      result = result + [arr[i]];
    }
    i = i + 1;
  }

  return result;
}

// Partitions an array into two: elements matching and not matching predicate.
export function partition<T>(arr: T[], predicate: (item: T) => boolean): [T[], T[]] {
  let matching: T[] = [];
  let notMatching: T[] = [];
  let i: number = 0;

  while (i < arr.length) {
    if (predicate(arr[i])) {
      matching = matching + [arr[i]];
    } else {
      notMatching = notMatching + [arr[i]];
    }
    i = i + 1;
  }

  return [matching, notMatching];
}

// Creates pairs of consecutive elements: [[a,b], [b,c], [c,d], ...].
export function pairs<T>(arr: T[]): [T, T][] {
  if (arr.length < 2) { return []; }

  let result: [T, T][] = [];
  let i: number = 0;

  while (i < arr.length - 1) {
    result = result + [[arr[i], arr[i + 1]]];
    i = i + 1;
  }

  return result;
}

// Groups consecutive equal elements together.
export function group<T>(arr: T[], eq: (a: T, b: T) => boolean): T[][] {
  if (arr.length == 0) { return []; }

  let result: T[][] = [];
  let currentGroup: T[] = [arr[0]];
  let i: number = 1;

  while (i < arr.length) {
    if (eq(arr[i - 1], arr[i])) {
      currentGroup = currentGroup + [arr[i]];
    } else {
      result = result + [currentGroup];
      currentGroup = [arr[i]];
    }
    i = i + 1;
  }

  result = result + [currentGroup];
  return result;
}

// Interleaves elements from two arrays.
export function interleave<T>(arr1: T[], arr2: T[]): T[] {
  let result: T[] = [];
  let i: number = 0;

  while (i < arr1.length && i < arr2.length) {
    result = result + [arr1[i]];
    result = result + [arr2[i]];
    i = i + 1;
  }

  while (i < arr1.length) {
    result = result + [arr1[i]];
    i = i + 1;
  }

  while (i < arr2.length) {
    result = result + [arr2[i]];
    i = i + 1;
  }

  return result;
}

// Concatenates multiple iterables into one.
export function concat<T>(arrays: T[][]): T[] {
  let result: T[] = [];
  let i: number = 0;

  while (i < arrays.length) {
    let j: number = 0;
    while (j < arrays[i].length) {
      result = result + [arrays[i][j]];
      j = j + 1;
    }
    i = i + 1;
  }

  return result;
}

// Flattens a nested array structure.
export function flatten<T>(arr: T[][]): T[] {
  let result: T[] = [];
  let i: number = 0;

  while (i < arr.length) {
    let j: number = 0;
    while (j < arr[i].length) {
      result = result + [arr[i][j]];
      j = j + 1;
    }
    i = i + 1;
  }

  return result;
}

// Cycles through an array's elements n times.
export function cycle<T>(arr: T[], times: number): T[] {
  if (arr.length == 0 || times <= 0) { return []; }

  let result: T[] = [];
  let i: number = 0;

  while (i < times) {
    let j: number = 0;
    while (j < arr.length) {
      result = result + [arr[j]];
      j = j + 1;
    }
    i = i + 1;
  }

  return result;
}

// Reverses an iterable.
export function reverse<T>(arr: T[]): T[] {
  if (arr.length == 0) { return []; }

  let result: T[] = [];
  let i: number = arr.length - 1;

  while (i >= 0) {
    result = result + [arr[i]];
    i = i - 1;
  }

  return result;
}

// Zips multiple arrays together.
export function zipMany<T>(arrays: T[][]): T[][] {
  if (arrays.length == 0) { return []; }

  let maxLen: number = 0;
  let i: number = 0;
  while (i < arrays.length) {
    if (arrays[i].length > maxLen) { maxLen = arrays[i].length; }
    i = i + 1;
  }

  let result: T[][] = [];
  let idx: number = 0;

  while (idx < maxLen) {
    let tuple: T[] = [];
    let j: number = 0;
    while (j < arrays.length) {
      if (idx < arrays[j].length) {
        tuple = tuple + [arrays[j][idx]];
      }
      j = j + 1;
    }
    if (tuple.length > 0) {
      result = result + [tuple];
    }
    idx = idx + 1;
  }

  return result;
}

// Creates an iterable from a predicate starting at a value.
export function iterate(initial: number, predicate: (value: number) => boolean, step: (value: number) => number): number[] {
  let result: number[] = [];
  let current: number = initial;

  while (predicate(current)) {
    result = result + [current];
    current = step(current);
  }

  return result;
}

// Generates an infinite-like sequence (until a predicate stops it).
export function generate(seed: number, next: (value: number) => number, maxIterations: number): number[] {
  let result: number[] = [];
  let current: number = seed;
  let i: number = 0;

  while (i < maxIterations) {
    result = result + [current];
    current = next(current);
    i = i + 1;
  }

  return result;
}

// Checks if any element matches a predicate.
export function any<T>(arr: T[], predicate: (item: T) => boolean): boolean {
  let i: number = 0;
  while (i < arr.length) {
    if (predicate(arr[i])) {
      return true;
    }
    i = i + 1;
  }
  return false;
}

// Checks if all elements match a predicate.
export function all<T>(arr: T[], predicate: (item: T) => boolean): boolean {
  let i: number = 0;
  while (i < arr.length) {
    if (!predicate(arr[i])) {
      return false;
    }
    i = i + 1;
  }
  return true;
}

// Finds the first element matching a predicate.
export function find<T>(arr: T[], predicate: (item: T) => boolean): T {
  let i: number = 0;
  while (i < arr.length) {
    if (predicate(arr[i])) {
      return arr[i];
    }
    i = i + 1;
  }
  return arr[0];
}

// Counts elements matching a predicate.
export function countWhere<T>(arr: T[], predicate: (item: T) => boolean): number {
  let count: number = 0;
  let i: number = 0;

  while (i < arr.length) {
    if (predicate(arr[i])) {
      count = count + 1;
    }
    i = i + 1;
  }

  return count;
}
