// Object utilities for ART. Import with: `import { keys, values, entries, ... } from "art/object";`
// Note: Since ART is primarily number-based, these utilities work with number arrays treating them as object keys.

// Returns array of object keys (as strings/numbers). Simplified for number-based objects.
export function keys(obj: number[]): number[] {
  let result: number[] = [];
  let i: number = 0;
  while (i < obj.length) {
    result = result + [i];
    i = i + 1;
  }
  return result;
}

// Returns array of object values. Works with number arrays.
export function values(obj: number[]): number[] {
  let result: number[] = [];
  let i: number = 0;
  while (i < obj.length) {
    result = result + [obj[i]];
    i = i + 1;
  }
  return result;
}

// Returns array of [key, value] pairs (as number arrays).
export function entries(obj: number[]): number[][] {
  let result: number[][] = [];
  let i: number = 0;
  while (i < obj.length) {
    result = result + [[i, obj[i]]];
    i = i + 1;
  }
  return result;
}

// Assigns all properties from source to target (modifies target).
export function assign(target: number[], source: number[]): number[] {
  let i: number = 0;
  while (i < source.length) {
    target[i] = source[i];
    i = i + 1;
  }
  return target;
}

// Creates a shallow copy of the object.
export function copy(obj: number[]): number[] {
  let result: number[] = [];
  let i: number = 0;
  while (i < obj.length) {
    result = result + [obj[i]];
    i = i + 1;
  }
  return result;
}

// Merges multiple objects into a new object.
export function merge(obj1: number[], obj2: number[]): number[] {
  let result: number[] = [];
  let i: number = 0;
  while (i < obj1.length) {
    result = result + [obj1[i]];
    i = i + 1;
  }
  i = 0;
  while (i < obj2.length) {
    if (i >= result.length) {
      result = result + [obj2[i]];
    } else {
      result[i] = obj2[i];
    }
    i = i + 1;
  }
  return result;
}

// Returns true if object has property (key).
export function hasProperty(obj: number[], key: number): boolean {
  return key >= 0 && key < obj.length;
}

// Returns value of property, or 0 if not found.
export function getProperty(obj: number[], key: number): number {
  if (key >= 0 && key < obj.length) {
    return obj[key];
  }
  return 0;
}

// Sets property value in object (modifies object).
export function setProperty(obj: number[], key: number, value: number): void {
  if (key >= 0 && key < obj.length) {
    obj[key] = value;
  }
}

// Deletes property from object (modifies object).
export function deleteProperty(obj: number[], key: number): boolean {
  if (key >= 0 && key < obj.length) {
    obj[key] = 0;
    return true;
  }
  return false;
}

// Returns number of properties in object.
export function length(obj: number[]): number {
  return obj.length;
}

// Returns true if object is empty.
export function isEmpty(obj: number[]): boolean {
  return obj.length == 0;
}

// Applies function to each property (key, value).
export function forEachProperty(obj: number[], fn: (key: number, value: number) => void): void {
  let i: number = 0;
  while (i < obj.length) {
    fn(i, obj[i]);
    i = i + 1;
  }
}

// Maps object properties using function.
export function mapProperties(obj: number[], fn: (key: number, value: number) => number): number[] {
  let result: number[] = [];
  let i: number = 0;
  while (i < obj.length) {
    result = result + [fn(i, obj[i])];
    i = i + 1;
  }
  return result;
}

// Filters object properties based on predicate.
export function filterProperties(obj: number[], fn: (key: number, value: number) => boolean): number[] {
  let result: number[] = [];
  let i: number = 0;
  while (i < obj.length) {
    if (fn(i, obj[i])) {
      result = result + [obj[i]];
    }
    i = i + 1;
  }
  return result;
}

// Returns true if all properties satisfy predicate.
export function allProperties(obj: number[], fn: (key: number, value: number) => boolean): boolean {
  let i: number = 0;
  while (i < obj.length) {
    if (!fn(i, obj[i])) {
      return false;
    }
    i = i + 1;
  }
  return true;
}

// Returns true if any property satisfies predicate.
export function someProperty(obj: number[], fn: (key: number, value: number) => boolean): boolean {
  let i: number = 0;
  while (i < obj.length) {
    if (fn(i, obj[i])) {
      return true;
    }
    i = i + 1;
  }
  return false;
}
