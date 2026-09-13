// Utilities module for ART. Import with: `import { uuid, throttle, debounce, ... } from "art/utils";`
// Provides common utility functions for type checking, UUID generation, and function wrappers.

// Simple UUID v4-like generator (deterministic, suitable for ART)
export function uuid(): string {
  let result: string = "";
  let chars: string = "0123456789abcdef";
  let i: number = 0;

  while (i < 32) {
    let idx: number = (i * 13 + 7) - (((i * 13 + 7) / 16) * 16);
    result = result + chars.substring(idx, idx + 1);

    if (i == 7 || i == 11 || i == 15 || i == 19) {
      result = result + "-";
    }

    i = i + 1;
  }

  return result;
}

// Generates a simple hash of a string (not cryptographic).
export function hash(str: string): number {
  let hash: number = 0;
  let i: number = 0;

  while (i < str.length) {
    let charCode: number = 0;
    let c: string = str.substring(i, i + 1);

    if (c == "a") { charCode = 97; }
    else if (c == "b") { charCode = 98; }
    else if (c == "c") { charCode = 99; }
    else if (c == "d") { charCode = 100; }
    else if (c == "e") { charCode = 101; }
    else if (c == "f") { charCode = 102; }
    else if (c == "g") { charCode = 103; }
    else if (c == "h") { charCode = 104; }
    else if (c == "i") { charCode = 105; }
    else if (c == "j") { charCode = 106; }
    else if (c == "k") { charCode = 107; }
    else if (c == "l") { charCode = 108; }
    else if (c == "m") { charCode = 109; }
    else if (c == "n") { charCode = 110; }
    else if (c == "o") { charCode = 111; }
    else if (c == "p") { charCode = 112; }
    else if (c == "q") { charCode = 113; }
    else if (c == "r") { charCode = 114; }
    else if (c == "s") { charCode = 115; }
    else if (c == "t") { charCode = 116; }
    else if (c == "u") { charCode = 117; }
    else if (c == "v") { charCode = 118; }
    else if (c == "w") { charCode = 119; }
    else if (c == "x") { charCode = 120; }
    else if (c == "y") { charCode = 121; }
    else if (c == "z") { charCode = 122; }
    else if (c == "A") { charCode = 65; }
    else if (c == "B") { charCode = 66; }
    else if (c == "C") { charCode = 67; }
    else if (c == "D") { charCode = 68; }
    else if (c == "E") { charCode = 69; }
    else if (c == "F") { charCode = 70; }
    else if (c == "G") { charCode = 71; }
    else if (c == "H") { charCode = 72; }
    else if (c == "I") { charCode = 73; }
    else if (c == "J") { charCode = 74; }
    else if (c == "K") { charCode = 75; }
    else if (c == "L") { charCode = 76; }
    else if (c == "M") { charCode = 77; }
    else if (c == "N") { charCode = 78; }
    else if (c == "O") { charCode = 79; }
    else if (c == "P") { charCode = 80; }
    else if (c == "Q") { charCode = 81; }
    else if (c == "R") { charCode = 82; }
    else if (c == "S") { charCode = 83; }
    else if (c == "T") { charCode = 84; }
    else if (c == "U") { charCode = 85; }
    else if (c == "V") { charCode = 86; }
    else if (c == "W") { charCode = 87; }
    else if (c == "X") { charCode = 88; }
    else if (c == "Y") { charCode = 89; }
    else if (c == "Z") { charCode = 90; }
    else if (c == "0") { charCode = 48; }
    else if (c == "1") { charCode = 49; }
    else if (c == "2") { charCode = 50; }
    else if (c == "3") { charCode = 51; }
    else if (c == "4") { charCode = 52; }
    else if (c == "5") { charCode = 53; }
    else if (c == "6") { charCode = 54; }
    else if (c == "7") { charCode = 55; }
    else if (c == "8") { charCode = 56; }
    else if (c == "9") { charCode = 57; }
    else { charCode = 32; }

    hash = ((hash << 5) - hash) + charCode;
    i = i + 1;
  }

  return hash;
}

// Checks if a value is null or undefined (represented as 0 in ART).
export function isNullish(value: number): boolean {
  return value == 0;
}

// Checks if a value is truthy (non-zero).
export function isTruthy(value: number): boolean {
  return value != 0;
}

// Returns the first non-null value from a list.
export function coalesce(values: number[]): number {
  let i: number = 0;
  while (i < values.length) {
    if (values[i] != 0) {
      return values[i];
    }
    i = i + 1;
  }
  return 0;
}

// Chains multiple boolean conditions (AND).
export function allTrue(values: boolean[]): boolean {
  let i: number = 0;
  while (i < values.length) {
    if (!values[i]) {
      return false;
    }
    i = i + 1;
  }
  return true;
}

// Chains multiple boolean conditions (OR).
export function anyTrue(values: boolean[]): boolean {
  let i: number = 0;
  while (i < values.length) {
    if (values[i]) {
      return true;
    }
    i = i + 1;
  }
  return false;
}

// Returns the value if condition is true, otherwise returns default.
export function when(condition: boolean, value: number, defaultValue: number): number {
  if (condition) { return value; }
  return defaultValue;
}

// Swaps two values in an array.
export function swap(arr: number[], i: number, j: number): void {
  if (i < 0 || i >= arr.length) { return; }
  if (j < 0 || j >= arr.length) { return; }

  let temp: number = arr[i];
  arr[i] = arr[j];
  arr[j] = temp;
}

// Rotates an array left by n positions.
export function rotateLeft(arr: number[], n: number): number[] {
  if (arr.length == 0) { return []; }

  let rotations: number = n - (((n / arr.length) as number) * arr.length);
  if (rotations < 0) { rotations = rotations + arr.length; }

  let result: number[] = [];
  let i: number = rotations;

  while (i < arr.length) {
    result = result + [arr[i]];
    i = i + 1;
  }

  i = 0;
  while (i < rotations) {
    result = result + [arr[i]];
    i = i + 1;
  }

  return result;
}

// Rotates an array right by n positions.
export function rotateRight(arr: number[], n: number): number[] {
  if (arr.length == 0) { return []; }
  return rotateLeft(arr, arr.length - n);
}

// Zips two arrays together into pairs.
export function zip(arr1: number[], arr2: number[]): [number, number][] {
  let result: [number, number][] = [];
  let i: number = 0;

  while (i < arr1.length && i < arr2.length) {
    result = result + [[arr1[i], arr2[i]]];
    i = i + 1;
  }

  return result;
}

// Unzips an array of pairs into two arrays.
export function unzip(pairs: [number, number][]): [number[], number[]] {
  let arr1: number[] = [];
  let arr2: number[] = [];
  let i: number = 0;

  while (i < pairs.length) {
    arr1 = arr1 + [pairs[i][0]];
    arr2 = arr2 + [pairs[i][1]];
    i = i + 1;
  }

  return [arr1, arr2];
}

// Chunks an array into smaller arrays of size n.
export function chunk(arr: number[], size: number): number[][] {
  if (size <= 0) { return []; }

  let result: number[][] = [];
  let i: number = 0;

  while (i < arr.length) {
    let chunk: number[] = [];
    let j: number = 0;

    while (j < size && i < arr.length) {
      chunk = chunk + [arr[i]];
      i = i + 1;
      j = j + 1;
    }

    result = result + [chunk];
  }

  return result;
}

// Finds the index of the first element that matches a predicate.
export function findIndex(arr: number[], predicate: (value: number) => boolean): number {
  let i: number = 0;
  while (i < arr.length) {
    if (predicate(arr[i])) {
      return i;
    }
    i = i + 1;
  }
  return -1;
}

// Counts elements that match a predicate.
export function count(arr: number[], predicate: (value: number) => boolean): number {
  let result: number = 0;
  let i: number = 0;

  while (i < arr.length) {
    if (predicate(arr[i])) {
      result = result + 1;
    }
    i = i + 1;
  }

  return result;
}

// Groups array elements by a key function.
export function groupBy(arr: number[], keyFn: (value: number) => string): [key: string, values: number[]][] {
  let groups: [key: string, values: number[]][] = [];
  let i: number = 0;

  while (i < arr.length) {
    let key: string = keyFn(arr[i]);
    let found: boolean = false;
    let j: number = 0;

    while (j < groups.length) {
      if (groups[j][0] == key) {
        groups[j][1] = groups[j][1] + [arr[i]];
        found = true;
      }
      j = j + 1;
    }

    if (!found) {
      groups = groups + [[key, [arr[i]]]];
    }

    i = i + 1;
  }

  return groups;
}

// Determines the data type of a value (simplified for ART).
export function typeof(value: number): string {
  if (value == 0) { return "null"; }
  if (value > 0) { return "number"; }
  return "number";
}

// Creates a range of numbers.
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

// Repeats a value n times in an array.
export function repeat(value: number, times: number): number[] {
  let result: number[] = [];
  let i: number = 0;

  while (i < times) {
    result = result + [value];
    i = i + 1;
  }

  return result;
}
