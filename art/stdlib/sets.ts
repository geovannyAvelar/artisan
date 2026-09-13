// Set operations for arrays without duplicates.
// Import with: `import { union, intersection, difference, ... } from "art/sets";`

// Returns the union of two arrays (all elements from both, no duplicates).
// Order: elements from left array first, then new elements from right array.
export function union(left: number[], right: number[]): number[] {
  let result: number[] = [];

  // Add all elements from left array
  let i: number = 0;
  while (i < left.length) {
    if (!contains(result, left[i])) {
      result = result + [left[i]];
    }
    i = i + 1;
  }

  // Add unique elements from right array
  i = 0;
  while (i < right.length) {
    if (!contains(result, right[i])) {
      result = result + [right[i]];
    }
    i = i + 1;
  }

  return result;
}

// Returns the intersection of two arrays (elements present in both).
// Order: elements appear in same order as left array.
export function intersection(left: number[], right: number[]): number[] {
  let result: number[] = [];

  let i: number = 0;
  while (i < left.length) {
    if (contains(right, left[i]) && !contains(result, left[i])) {
      result = result + [left[i]];
    }
    i = i + 1;
  }

  return result;
}

// Returns the difference of two arrays (elements in left but not in right).
// Order: elements appear in same order as left array.
export function difference(left: number[], right: number[]): number[] {
  let result: number[] = [];

  let i: number = 0;
  while (i < left.length) {
    if (!contains(right, left[i]) && !contains(result, left[i])) {
      result = result + [left[i]];
    }
    i = i + 1;
  }

  return result;
}

// Returns the symmetric difference of two arrays.
// Elements that are in either left or right, but not in both.
export function symmetricDifference(left: number[], right: number[]): number[] {
  let result: number[] = [];

  // Add elements from left that are not in right
  let i: number = 0;
  while (i < left.length) {
    if (!contains(right, left[i]) && !contains(result, left[i])) {
      result = result + [left[i]];
    }
    i = i + 1;
  }

  // Add elements from right that are not in left
  i = 0;
  while (i < right.length) {
    if (!contains(left, right[i]) && !contains(result, right[i])) {
      result = result + [right[i]];
    }
    i = i + 1;
  }

  return result;
}

// Checks if left is a subset of right (all elements of left are in right).
// Empty array is subset of any array.
export function isSubset(left: number[], right: number[]): boolean {
  if (left.length == 0) { return true; }

  let i: number = 0;
  while (i < left.length) {
    if (!contains(right, left[i])) {
      return false;
    }
    i = i + 1;
  }
  return true;
}

// Checks if left is a superset of right (all elements of right are in left).
// Any array is superset of empty array.
export function isSuperset(left: number[], right: number[]): boolean {
  if (right.length == 0) { return true; }

  let i: number = 0;
  while (i < right.length) {
    if (!contains(left, right[i])) {
      return false;
    }
    i = i + 1;
  }
  return true;
}

// Checks if two arrays have no common elements (disjoint sets).
// Empty arrays are disjoint from any array.
export function disjoint(left: number[], right: number[]): boolean {
  if (left.length == 0 || right.length == 0) { return true; }

  let i: number = 0;
  while (i < left.length) {
    if (contains(right, left[i])) {
      return false;
    }
    i = i + 1;
  }
  return true;
}

// Checks if two sets are equal (contain same elements, ignoring order and duplicates).
export function setEquals(left: number[], right: number[]): boolean {
  let leftUnique: number[] = unique(left);
  let rightUnique: number[] = unique(right);

  if (leftUnique.length != rightUnique.length) { return false; }

  let i: number = 0;
  while (i < leftUnique.length) {
    if (!contains(rightUnique, leftUnique[i])) {
      return false;
    }
    i = i + 1;
  }
  return true;
}

// Returns unique elements from an array (removes duplicates).
// Order: first occurrence of each element is preserved.
export function unique(arr: number[]): number[] {
  let result: number[] = [];

  let i: number = 0;
  while (i < arr.length) {
    if (!contains(result, arr[i])) {
      result = result + [arr[i]];
    }
    i = i + 1;
  }

  return result;
}

// Checks if an array has duplicates.
export function hasDuplicates(arr: number[]): boolean {
  let i: number = 0;
  while (i < arr.length) {
    let j: number = i + 1;
    while (j < arr.length) {
      if (arr[i] == arr[j]) {
        return true;
      }
      j = j + 1;
    }
    i = i + 1;
  }
  return false;
}

// Returns the Cartesian product of two arrays.
// Result contains all pairs [a, b] where a is from left and b is from right.
// Note: Result is flattened to a single number[] with pairs side-by-side.
export function cartesianProduct(left: number[], right: number[]): number[] {
  let result: number[] = [];

  let i: number = 0;
  while (i < left.length) {
    let j: number = 0;
    while (j < right.length) {
      result = result + [left[i]];
      result = result + [right[j]];
      j = j + 1;
    }
    i = i + 1;
  }

  return result;
}

// Returns the complement of an array with respect to a universal set.
// Elements in universe but not in arr.
export function complement(arr: number[], universe: number[]): number[] {
  return difference(universe, arr);
}

// Counts how many elements from one array are in another.
export function countCommon(left: number[], right: number[]): number {
  let count: number = 0;

  let i: number = 0;
  while (i < left.length) {
    if (contains(right, left[i])) {
      count = count + 1;
    }
    i = i + 1;
  }

  return count;
}

// Checks the Jaccard similarity between two arrays (0 to 1).
// Similarity = |intersection| / |union|
// Returns 1 if sets are identical, 0 if disjoint.
export function jaccardSimilarity(left: number[], right: number[]): number {
  let leftUnique: number[] = unique(left);
  let rightUnique: number[] = unique(right);

  let inter: number[] = intersection(leftUnique, rightUnique);
  let uni: number[] = union(leftUnique, rightUnique);

  if (uni.length == 0) { return 1; }
  return inter.length / uni.length;
}

// Helper: Checks if an array contains a specific value.
function contains(arr: number[], value: number): boolean {
  let i: number = 0;
  while (i < arr.length) {
    if (arr[i] == value) {
      return true;
    }
    i = i + 1;
  }
  return false;
}
