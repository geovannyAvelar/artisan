# ART Sorting Utilities Library Reference

Array sorting and searching utilities for ART runtime. Import with:
```typescript
import { sortAsc, sortDesc, sortBy, isSorted, ... } from "art/sort";
```

## Basic Sorting

### sortAsc(arr: number[]): number[]
Sorts an array of numbers in ascending order using insertion sort. Modifies the array in place and returns it for chaining.
- Time complexity: O(n²) average and worst case, O(n) best case
- Space complexity: O(1) - in-place sorting
- Stable: Yes - equal elements maintain relative order
```typescript
let arr: number[] = [3, 1, 4, 1, 5];
sortAsc(arr);
// arr is now [1, 1, 3, 4, 5]
```

### sortDesc(arr: number[]): number[]
Sorts an array of numbers in descending order using insertion sort. Modifies the array in place and returns it for chaining.
- Time complexity: O(n²) average and worst case, O(n) best case
- Space complexity: O(1) - in-place sorting
- Stable: Yes
```typescript
let arr: number[] = [3, 1, 4, 1, 5];
sortDesc(arr);
// arr is now [5, 4, 3, 1, 1]
```

### sortBy(arr: number[], comparator: (a: number, b: number) => number): number[]
Sorts an array using a custom comparison function. The comparator receives (a, b) and should return:
- Negative value if a comes before b
- Zero if a and b are equivalent
- Positive value if a comes after b

Uses insertion sort. Modifies the array in place and returns it.
```typescript
let arr: number[] = [3, 1, 4, 1, 5];
sortBy(arr, function(a: number, b: number): number {
  if (a < b) { return -1; }
  if (a > b) { return 1; }
  return 0;
});
// arr is now [1, 1, 3, 4, 5]

// Sort by absolute value
sortBy(arr, function(a: number, b: number): number {
  let absA: number = a < 0 ? -a : a;
  let absB: number = b < 0 ? -b : b;
  if (absA < absB) { return -1; }
  if (absA > absB) { return 1; }
  return 0;
});
```

## Checking Sort Order

### isSorted(arr: number[]): boolean
Checks if an array is sorted in ascending order. Returns true for empty arrays or single-element arrays.
```typescript
isSorted([1, 2, 3, 4, 5])    // → true
isSorted([5, 4, 3, 2, 1])    // → false
isSorted([1, 1, 2, 2, 3])    // → true (non-decreasing)
isSorted([])                 // → true
```

### isSortedDesc(arr: number[]): boolean
Checks if an array is sorted in descending order. Returns true for empty arrays or single-element arrays.
```typescript
isSortedDesc([5, 4, 3, 2, 1])    // → true
isSortedDesc([1, 2, 3, 4, 5])    // → false
isSortedDesc([5, 5, 4, 4, 3])    // → true (non-increasing)
```

## Array Manipulation

### reverse(arr: number[]): number[]
Reverses an array in place by swapping elements from both ends. Returns the array for chaining.
```typescript
let arr: number[] = [1, 2, 3, 4, 5];
reverse(arr);
// arr is now [5, 4, 3, 2, 1]
```

### shuffle(arr: number[]): number[]
Shuffles an array in place using a Fisher-Yates-like algorithm. Note: Uses a pseudo-random approach based on array indices and values. For true randomness, integrate with a proper random number generator. Modifies the array in place and returns it.
```typescript
let arr: number[] = [1, 2, 3, 4, 5];
shuffle(arr);
// arr is now randomly reordered
```

## Finding Elements

### minIndex(arr: number[]): number
Finds the index of the minimum value in an array. Returns -1 if the array is empty.
```typescript
minIndex([3, 1, 4, 1, 5])    // → 1 (first occurrence of 1)
minIndex([42])               // → 0
minIndex([])                 // → -1
```

### maxIndex(arr: number[]): number
Finds the index of the maximum value in an array. Returns -1 if the array is empty.
```typescript
maxIndex([3, 1, 4, 1, 5])    // → 4 (value 5 at index 4)
maxIndex([9, 2, 7])          // → 0 (value 9 at index 0)
maxIndex([])                 // → -1
```

### kthSmallest(arr: number[], k: number): number
Finds the kth smallest element in an array (1-indexed).
- k=1 returns the minimum
- k=length returns the maximum
- k<1 or k>length returns 0

Note: Sorts the array in place to find the element.
```typescript
let arr: number[] = [3, 1, 4, 1, 5, 9, 2];
kthSmallest(arr, 1)   // → 1 (minimum)
kthSmallest(arr, 3)   // → 2 (3rd smallest)
kthSmallest(arr, 7)   // → 9 (maximum)
```

### median(arr: number[]): number
Finds the median of an array. For even-length arrays, returns the lower middle value. For odd-length arrays, returns the exact middle. Note: Modifies the array (sorts it in place).
```typescript
let arr: number[] = [3, 1, 4, 1, 5];
median(arr)          // → 3 (middle of [1, 1, 3, 4, 5])
```

## Searching and Analysis

### hasDuplicates(arr: number[]): boolean
Checks if an array contains duplicate values. Returns true if any value appears more than once.
- Time complexity: O(n²)
```typescript
hasDuplicates([1, 2, 3, 1])      // → true
hasDuplicates([1, 2, 3, 4, 5])   // → false
hasDuplicates([])                // → false
```

### removeDuplicates(arr: number[]): number
Removes duplicate values from a sorted array in place. Returns the new length of the array.

**Important:** The array must be sorted beforehand using `sortAsc`. After calling, only the first `newLength` elements are meaningful.
```typescript
let arr: number[] = [1, 1, 2, 2, 3, 3, 3, 4];
let newLen: number = removeDuplicates(arr);
// newLen = 4
// arr is now [1, 2, 3, 4, ?, ?, ?, ?] (? = undefined behavior)
```

### countInversions(arr: number[]): number
Counts the number of inversions in an array (pairs where i < j but arr[i] > arr[j]).
- Time complexity: O(n²)
- An inversion represents how "unsorted" the array is
```typescript
countInversions([1, 2, 3, 4, 5])   // → 0 (already sorted)
countInversions([5, 4, 3, 2, 1])   // → 10 (fully reversed)
countInversions([3, 1, 2])         // → 2 (pairs: (3,1), (3,2))
```

## Array Properties

### isPalindrome(arr: number[]): boolean
Checks if an array reads the same forwards and backwards.
```typescript
isPalindrome([1, 2, 3, 2, 1])     // → true
isPalindrome([1, 2, 2, 1])        // → true
isPalindrome([1, 2, 3, 4, 5])     // → false
isPalindrome([5])                 // → true
isPalindrome([])                  // → true
```

## Advanced Operations

### partition(arr: number[], left: number, right: number, pivot: number): number
Partitions an array section around a pivot value. Elements less than pivot come before it; elements greater come after. Returns the final index of the pivot.

Used internally by quicksort-like algorithms.
```typescript
let arr: number[] = [3, 1, 4, 1, 5, 9, 2, 6];
let pivotIdx: number = partition(arr, 0, 7, 5);
// arr is rearranged with values < 5 on left, > 5 on right
```

### merge(left: number[], right: number[]): number[]
Merges two sorted arrays into a single sorted array. Both input arrays must be sorted in ascending order. Returns a new array without modifying inputs.
```typescript
let left: number[] = [1, 3, 5];
let right: number[] = [2, 4, 6];
let merged: number[] = merge(left, right);
// merged is [1, 2, 3, 4, 5, 6]
```

## Common Patterns

### Sort and Check
```typescript
let arr: number[] = [3, 1, 4, 1, 5];
sortAsc(arr);
if (isSorted(arr)) {
  // Always true after sortAsc
}
```

### Find Min/Max
```typescript
let arr: number[] = [3, 1, 4, 1, 5, 9];
let minVal: number = arr[minIndex(arr)];  // → 1
let maxVal: number = arr[maxIndex(arr)];  // → 9
```

### Remove Duplicates
```typescript
let arr: number[] = [1, 3, 1, 2, 2, 3];
sortAsc(arr);                        // [1, 1, 2, 2, 3, 3]
let uniqueLen: number = removeDuplicates(arr);  // → 3
// Use first uniqueLen elements
```

### Measure Disorder
```typescript
let arr: number[] = [3, 1, 4, 1, 5];
let inversions: number = countInversions(arr);
if (inversions == 0) {
  // Array is already sorted
}
```

## Implementation Notes

### Algorithm Choices
- **Insertion Sort**: Used for basic sorting (sortAsc, sortDesc, sortBy)
  - Good for small arrays (n < 50)
  - Stable and adaptive (faster on nearly-sorted data)
  - O(n²) worst case, but O(n) best case
  
### Performance Characteristics
- **Fast** (O(n)): isSorted, isSortedDesc, reverse, minIndex, maxIndex, isPalindrome
- **Moderate** (O(n log n)): merge operations, median (uses sort)
- **Slow** (O(n²)): countInversions, hasDuplicates, partition

### Stability
All sorting functions maintain the relative order of equal elements (stable sort), which is important when sorting complex data.

### Edge Cases
- Empty arrays: Handled gracefully, return sensible defaults
- Single-element arrays: Always return true for sorted checks, unchanged otherwise
- All equal elements: Recognized as sorted in both directions
- Negative numbers: Handled correctly by all comparison functions

### In-Place Operations
Functions that modify arrays in place: sortAsc, sortDesc, sortBy, reverse, shuffle, removeDuplicates (modifies structure but not values for duplicate entries).

Functions that return new arrays: merge
