// Array methods for ART - higher-order functions on arrays.
// Import with: `import { map, filter, forEach, ... } from "art/arrays";`
//
// These are implemented in pure ART code (not C++ FFI), so they're available
// in any ART program. They use generic functions with explicit instantiation
// (::<T> or ::<T, U>) to work with any element type.

// Calls fn on each element for side effects. Returns void.
export function forEach<T>(arr: T[], fn: (x: T) => void): void {
  let i: number = 0;
  while (i < arr.length) {
    fn(arr[i]);
    i = i + 1;
  }
}

// Returns true if fn returns true for any element.
export function some<T>(arr: T[], fn: (x: T) => boolean): boolean {
  let i: number = 0;
  while (i < arr.length) {
    if (fn(arr[i])) { return true; }
    i = i + 1;
  }
  return false;
}

// Returns true if fn returns true for every element.
export function every<T>(arr: T[], fn: (x: T) => boolean): boolean {
  let i: number = 0;
  while (i < arr.length) {
    if (!fn(arr[i])) { return false; }
    i = i + 1;
  }
  return true;
}

// Returns a new array with elements transformed by fn.
export function map<T, U>(arr: T[], fn: (x: T) => U): U[] {
  let result: U[] = [];
  let i: number = 0;
  while (i < arr.length) {
    result = result + [fn(arr[i])];
    i = i + 1;
  }
  return result;
}

// Returns a new array with only elements where fn returns true.
export function filter<T>(arr: T[], fn: (x: T) => boolean): T[] {
  let result: T[] = [];
  let i: number = 0;
  while (i < arr.length) {
    if (fn(arr[i])) {
      result = result + [arr[i]];
    }
    i = i + 1;
  }
  return result;
}

// Returns the first element where fn returns true, or null.
export function find<T>(arr: T[], fn: (x: T) => boolean): T | null {
  let i: number = 0;
  while (i < arr.length) {
    if (fn(arr[i])) {
      return notNull::<T>(arr[i]);
    }
    i = i + 1;
  }
  return null;
}

// Returns the index of the first element where fn returns true, or -1.
export function findIndex<T>(arr: T[], fn: (x: T) => boolean): number {
  let i: number = 0;
  while (i < arr.length) {
    if (fn(arr[i])) { return i; }
    i = i + 1;
  }
  return -1;
}

// Returns true if array contains the value (using == equality).
export function includes<T>(arr: T[], value: T): boolean {
  let i: number = 0;
  while (i < arr.length) {
    if (arr[i] == value) { return true; }
    i = i + 1;
  }
  return false;
}

// Returns the index of the first occurrence of value, or -1.
export function indexOf<T>(arr: T[], value: T): number {
  let i: number = 0;
  while (i < arr.length) {
    if (arr[i] == value) { return i; }
    i = i + 1;
  }
  return -1;
}

// Returns the index of the last occurrence of value, or -1.
export function lastIndexOf<T>(arr: T[], value: T): number {
  let i: number = arr.length - 1;
  while (i >= 0) {
    if (arr[i] == value) { return i; }
    i = i - 1;
  }
  return -1;
}

// Applies a function against an accumulator and each element to reduce to a single value.
export function reduce<T, U>(arr: T[], fn: (acc: U, x: T) => U, initial: U): U {
  let acc: U = initial;
  let i: number = 0;
  while (i < arr.length) {
    acc = fn(acc, arr[i]);
    i = i + 1;
  }
  return acc;
}

// Joins array elements into a string with a separator.
// Works for number arrays. Other types: use custom implementation with reduce.
export function join(arr: number[], separator: string): string {
  if (arr.length == 0) { return ""; }
  let result: string = "";
  let i: number = 0;
  while (i < arr.length) {
    if (i > 0) { result = result + separator; }
    result = result + numberToString(arr[i]);
    i = i + 1;
  }
  return result;
}

// Returns a new array with elements in reverse order.
export function reverse<T>(arr: T[]): T[] {
  let result: T[] = [];
  let i: number = arr.length - 1;
  while (i >= 0) {
    result = result + [arr[i]];
    i = i - 1;
  }
  return result;
}

// Returns a shallow copy of a portion of the array.
export function slice<T>(arr: T[], start: number, end: number): T[] {
  if (start < 0) { start = arr.length + start; }
  if (end < 0) { end = arr.length + end; }
  if (start < 0) { start = 0; }
  if (end > arr.length) { end = arr.length; }
  if (start >= end) { return []; }

  let result: T[] = [];
  let i: number = start;
  while (i < end) {
    result = result + [arr[i]];
    i = i + 1;
  }
  return result;
}

// Returns a new array with all sub-array elements concatenated.
export function flat<T>(arr: T[][]): T[] {
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

// Fills array with value from start to end (mutates array).
export function fill<T>(arr: T[], value: T, start: number, end: number): T[] {
  if (start < 0) { start = 0; }
  if (end > arr.length) { end = arr.length; }

  let i: number = start;
  while (i < end) {
    arr[i] = value;
    i = i + 1;
  }
  return arr;
}

// Concatenates array with other arrays, returning new array.
export function concat<T>(arr: T[], other: T[]): T[] {
  let result: T[] = [];
  let i: number = 0;
  while (i < arr.length) {
    result = result + [arr[i]];
    i = i + 1;
  }
  i = 0;
  while (i < other.length) {
    result = result + [other[i]];
    i = i + 1;
  }
  return result;
}

// Returns true if array is empty.
export function isEmpty<T>(arr: T[]): boolean {
  return arr.length == 0;
}

// Returns the first element of array, or null.
export function first<T>(arr: T[]): T | null {
  if (arr.length == 0) { return null; }
  return notNull::<T>(arr[0]);
}

// Returns the last element of array, or null.
export function last<T>(arr: T[]): T | null {
  if (arr.length == 0) { return null; }
  return notNull::<T>(arr[arr.length - 1]);
}

// Returns a new array with duplicates removed (preserves order).
export function unique<T>(arr: T[]): T[] {
  let result: T[] = [];
  let i: number = 0;
  while (i < arr.length) {
    let found: boolean = false;
    let j: number = 0;
    while (j < result.length) {
      if (result[j] == arr[i]) {
        found = true;
      }
      j = j + 1;
    }
    if (!found) {
      result = result + [arr[i]];
    }
    i = i + 1;
  }
  return result;
}
