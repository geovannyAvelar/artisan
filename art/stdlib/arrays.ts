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
