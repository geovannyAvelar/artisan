# Array Methods in ART

ART now includes a standard library module providing higher-order array methods, available in the `arrays` module.

## Overview

All array methods are declared as generic functions that work with any element type `T`. They are implemented in pure ART code (not C++ FFI), making them available in any ART program.

## Importing

```ts
import { forEach, some, every, find, findIndex, includes, indexOf, lastIndexOf, reduce, join } from "art/arrays";
```

## Methods

### Iteration & Predicates

#### `forEach<T>(arr: T[], fn: (x: T) => void): void`

Calls a function on each element for side effects.

```ts
let nums: number[] = [1, 2, 3];
forEach::<number>(nums, function(x: number): void {
  console.log(numberToString(x));
});
```

#### `some<T>(arr: T[], fn: (x: T) => boolean): boolean`

Returns `true` if the predicate returns `true` for any element.

```ts
let hasEven: boolean = some::<number>([1, 2, 3], function(x: number): boolean {
  return x % 2 == 0;  // true (2 is even)
});
```

#### `every<T>(arr: T[], fn: (x: T) => boolean): boolean`

Returns `true` if the predicate returns `true` for all elements.

```ts
let allEven: boolean = every::<number>([2, 4, 6], function(x: number): boolean {
  return x % 2 == 0;  // true
});
```

### Search

#### `find<T>(arr: T[], fn: (x: T) => boolean): T | null`

Returns the first element matching the predicate, or `null`.

```ts
let first: number | null = find::<number>([1, 2, 3, 4], function(x: number): boolean {
  return x > 2;  // 3
});
```

#### `findIndex<T>(arr: T[], fn: (x: T) => boolean): number`

Returns the index of the first element matching the predicate, or `-1`.

```ts
let idx: number = findIndex::<number>([10, 20, 30], function(x: number): boolean {
  return x > 15;  // 1 (20 is at index 1)
});
```

#### `includes<T>(arr: T[], value: T): boolean`

Returns `true` if the array contains the value (using `==` equality).

```ts
if (includes::<number>([1, 2, 3], 2)) {
  // true
}
```

#### `indexOf<T>(arr: T[], value: T): number`

Returns the index of the first occurrence of the value, or `-1`.

```ts
let idx: number = indexOf::<number>([5, 10, 5, 20], 5);  // 0
```

#### `lastIndexOf<T>(arr: T[], value: T): number`

Returns the index of the last occurrence of the value, or `-1`.

```ts
let idx: number = lastIndexOf::<number>([5, 10, 5, 20], 5);  // 2
```

### Reduction

#### `reduce<T, U>(arr: T[], fn: (acc: U, x: T) => U, initial: U): U`

Applies a function against an accumulator and each element to reduce the array to a single value. This is the most powerful array method - you can implement many other operations using it.

```ts
// Sum
let sum: number = reduce::<number, number>([1, 2, 3, 4], function(acc: number, x: number): number {
  return acc + x;
}, 0);  // 10

// Product
let product: number = reduce::<number, number>([1, 2, 3, 4], function(acc: number, x: number): number {
  return acc * x;
}, 1);  // 24

// Count elements matching a condition
let evenCount: number = reduce::<number, number>([1, 2, 3, 4], function(acc: number, x: number): number {
  return acc + (x % 2 == 0 ? 1 : 0);
}, 0);  // 2
```

### String Conversion

#### `join(arr: number[], separator: string): string`

Joins numeric array elements into a string with a separator.

```ts
let result: string = join([1, 2, 3, 4], "-");  // "1-2-3-4"
let empty: string = join(makeArray::<number>(0, 0), ",");  // ""
```

For string arrays, use `reduce`:
```ts
let strs: string[] = ["hello", "world"];
let result: string = reduce::<string, string>(strs, function(acc: string, x: string): string {
  return acc + (acc == "" ? x : " " + x);
}, "");  // "hello world"
```

## Implementation Notes

- All array methods use explicit generic instantiation - you must provide type parameters at call sites
- Methods that iterate (like `forEach`, `some`, `every`, `find`, `findIndex`) take handler functions with `=> void` or `=> boolean` returns
- Methods like `reduce` support transforming between types via separate generic type parameters
- For `reduce` with nullable results, consider using the `any` type if needed to bypass narrowing restrictions

## Pure Functional Approach

These methods follow a pure functional programming style - they don't mutate the original array. This makes code easier to reason about and test.

```ts
let original: number[] = [1, 2, 3];
let isEven: (n: number) => boolean = function(n: number): boolean { return n % 2 == 0; };

// Methods on number[] itself
original.length;  // 3
original[0];      // 1
for (let x of original) { /* ... */ }  // for...of works

// Methods from this module (imported)
every::<number>(original, isEven);  // false (1 and 3 are odd)
some::<number>(original, isEven);   // true (2 is even)
```

## Closure Capture

Handler functions can capture outer variables:

```ts
let threshold: number = 5;
let aboveThreshold: boolean = some::<number>([1, 2, 6, 3], function(x: number): boolean {
  return x > threshold;  // captures threshold
});  // true
```

## Combining Methods

Since `reduce` is very powerful, you can implement other operations:

```ts
// Count matches (instead of having a separate count/filter)
let count: number = reduce::<number, number>(
  [1, 2, 3, 4, 5],
  function(acc: number, x: number): number {
    return acc + (x > 2 ? 1 : 0);
  },
  0
);

// Build a string from a filtered set
let msg: string = reduce::<number, string>(
  [1, 2, 3, 4, 5],
  function(acc: string, x: number): number {
    if (x % 2 == 0) {
      return acc + (acc == "" ? numberToString(x) : "," + numberToString(x));
    }
    return acc;
  },
  ""
);  // "2,4"
```

## Future Enhancements

The current array methods don't include `map` and `filter` due to challenges with the template parameter pattern (they need to allocate new arrays of unknown size). A future version may add these using different patterns or compiler support for method calls on built-in array types.

Workarounds for now:
- Use `reduce` to build a new array
- Use a helper function with `makeArray` to pre-allocate
- Implement custom loops with explicit array allocation
