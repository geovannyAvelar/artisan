# ART Set Operations Library Reference

Set theory operations for arrays without duplicates. Import with:
```typescript
import { union, intersection, difference, ... } from "art/sets";
```

All operations treat arrays as mathematical sets and automatically handle duplicates.

## Basic Set Operations

### union(left: number[], right: number[]): number[]
Returns the union of two arrays (all elements from both, no duplicates).
- Elements from left array appear first, then new elements from right
- Duplicates are automatically removed
- Time complexity: O(n*m) where n and m are array lengths
```typescript
union([1, 2, 3], [3, 4, 5])      // → [1, 2, 3, 4, 5]
union([1, 1, 2], [2, 3, 3])      // → [1, 2, 3]
union([], [1, 2])                // → [1, 2]
```

### intersection(left: number[], right: number[]): number[]
Returns the intersection of two arrays (elements present in both).
- Order: elements appear in same order as left array
- Duplicates are removed
- Time complexity: O(n*m)
```typescript
intersection([1, 2, 3, 4], [3, 4, 5, 6])    // → [3, 4]
intersection([1, 2, 3], [4, 5, 6])          // → []
intersection([1, 1, 2, 2], [1, 2])          // → [1, 2]
```

### difference(left: number[], right: number[]): number[]
Returns the difference of two arrays (elements in left but not in right).
- Order: elements appear in same order as left array
- Removes duplicates from result
- Time complexity: O(n*m)
```typescript
difference([1, 2, 3, 4, 5], [3, 4, 5, 6])   // → [1, 2]
difference([1, 2, 3], [1, 2, 3])            // → []
difference([1, 1, 2], [3, 4])               // → [1, 2]
```

### symmetricDifference(left: number[], right: number[]): number[]
Returns the symmetric difference of two arrays.
- Elements in either left or right, but not in both
- Removes duplicates
- Time complexity: O(n*m)
```typescript
symmetricDifference([1, 2, 3], [3, 4, 5])   // → [1, 2, 4, 5]
symmetricDifference([1, 2], [1, 2])         // → []
symmetricDifference([1, 1], [2, 2])         // → [1, 2]
```

## Set Relationships

### isSubset(left: number[], right: number[]): boolean
Checks if left is a subset of right (all elements of left are in right).
- Empty array is subset of any array
- Time complexity: O(n*m)
```typescript
isSubset([1, 2], [1, 2, 3])      // → true
isSubset([1, 4], [1, 2, 3])      // → false
isSubset([], [1, 2, 3])          // → true
isSubset([1, 2, 3], [1, 2, 3])   // → true
```

### isSuperset(left: number[], right: number[]): boolean
Checks if left is a superset of right (all elements of right are in left).
- Any array is superset of empty array
- Time complexity: O(n*m)
```typescript
isSuperset([1, 2, 3], [1, 2])    // → true
isSuperset([1, 2], [1, 2, 3])    // → false
isSuperset([1, 2, 3], [])        // → true
```

### disjoint(left: number[], right: number[]): boolean
Checks if two arrays have no common elements (disjoint sets).
- Empty arrays are disjoint from any array
- Time complexity: O(n*m)
```typescript
disjoint([1, 2, 3], [4, 5, 6])   // → true
disjoint([1, 2, 3], [3, 4, 5])   // → false
disjoint([], [1, 2, 3])          // → true
```

### setEquals(left: number[], right: number[]): boolean
Checks if two sets are equal (contain same elements, ignoring order and duplicates).
- Order doesn't matter
- Duplicates are ignored
- Time complexity: O(n*m)
```typescript
setEquals([1, 2, 3], [1, 2, 3])      // → true
setEquals([1, 2, 3], [3, 2, 1])      // → true
setEquals([1, 1, 2], [1, 2])         // → true
setEquals([1, 2, 3], [1, 2])         // → false
```

## Utility Functions

### unique(arr: number[]): number[]
Returns unique elements from an array (removes duplicates).
- Order: first occurrence of each element is preserved
- Time complexity: O(n²)
```typescript
unique([1, 2, 2, 3, 3, 3, 4])   // → [1, 2, 3, 4]
unique([1, 1, 1, 1])            // → [1]
unique([1, 2, 3])               // → [1, 2, 3]
```

### hasDuplicates(arr: number[]): boolean
Checks if an array has duplicate values.
- Time complexity: O(n²)
```typescript
hasDuplicates([1, 2, 3, 2])     // → true
hasDuplicates([1, 2, 3, 4])     // → false
hasDuplicates([])               // → false
hasDuplicates([5, 5])           // → true
```

### countCommon(left: number[], right: number[]): number
Counts how many elements from one array appear in another.
- Counts first occurrence of each element
- Time complexity: O(n*m)
```typescript
countCommon([1, 2, 3], [2, 3, 4])    // → 2 (elements 2 and 3)
countCommon([1, 2, 3], [4, 5, 6])    // → 0
countCommon([1, 2, 3], [1, 2, 3])    // → 3
```

## Advanced Operations

### cartesianProduct(left: number[], right: number[]): number[]
Returns the Cartesian product of two arrays (all pairs).
- Result contains all pairs [a, b] where a is from left and b is from right
- Result is flattened with pairs side-by-side: [a1, b1, a2, b1, a1, b2, ...]
- Time complexity: O(n*m)
```typescript
cartesianProduct([1, 2], [3, 4])
// → [1, 3, 1, 4, 2, 3, 2, 4] (four pairs flattened)
// Pairs: (1,3), (1,4), (2,3), (2,4)

let result: number[] = cartesianProduct([1, 2], [3, 4]);
// Extract pairs: result[0,1] = (1,3), result[2,3] = (1,4), etc.
```

### complement(arr: number[], universe: number[]): number[]
Returns the complement of an array with respect to a universal set.
- Elements in universe but not in arr
- Alias for `difference(universe, arr)`
- Time complexity: O(n*m)
```typescript
complement([1, 2], [1, 2, 3, 4, 5])   // → [3, 4, 5]
complement([2, 3], [1, 2, 3, 4])      // → [1, 4]
```

### jaccardSimilarity(left: number[], right: number[]): number
Calculates Jaccard similarity between two sets (0 to 1).
- Formula: |intersection| / |union|
- 1 = identical sets, 0 = disjoint sets
- Time complexity: O(n*m)
```typescript
jaccardSimilarity([1, 2, 3], [1, 2, 3])     // → 1.0 (identical)
jaccardSimilarity([1, 2], [3, 4])           // → 0.0 (disjoint)
jaccardSimilarity([1, 2, 3], [2, 3, 4])     // → 0.5 (50% similar)
```

## Common Patterns

### Filter Based on Another Set
```typescript
let data: number[] = [1, 2, 3, 4, 5];
let toRemove: number[] = [2, 4];
let filtered: number[] = difference(data, toRemove);
// filtered = [1, 3, 5]
```

### Find Common Interests
```typescript
let userA: number[] = [1, 2, 3, 4];  // Users A follows
let userB: number[] = [3, 4, 5, 6];  // Users B follows
let common: number[] = intersection(userA, userB);
// common = [3, 4] (both follow users 3 and 4)
```

### Combine Multiple Lists (No Duplicates)
```typescript
let list1: number[] = [1, 2, 3];
let list2: number[] = [3, 4, 5];
let list3: number[] = [5, 6, 7];

let combined: number[] = union(list1, list2);
combined = union(combined, list3);
// combined = [1, 2, 3, 4, 5, 6, 7]
```

### Check Set Membership
```typescript
let allowedRoles: number[] = [1, 2, 3];
let userRole: number = 2;
if (countCommon([userRole], allowedRoles) > 0) {
  // User has allowed role
}
```

### Chained Set Operations
```typescript
let a: number[] = [1, 2, 3];
let b: number[] = [3, 4, 5];
let c: number[] = [5, 6, 7];

// (a ∪ b) ∩ c = [5]
let result: number[] = intersection(union(a, b), c);
```

## Implementation Notes

### Duplicate Handling
All set operations automatically treat arrays as sets:
- Duplicates within a single array are ignored
- Input arrays are not modified
- Results never contain duplicates
- `unique()` can be called explicitly to clean data

### Order Preservation
- `union`, `intersection`, `difference`: order from left array preserved first
- `symmetricDifference`, `cartesianProduct`: specific ordering defined
- Use `sort` module if sorted order is needed

### Performance Considerations
- Most operations are O(n*m) due to duplicate checking
- For very large arrays (>1000 elements), consider:
  - Sorting first if operations will be repeated
  - Using difference arrays when possible (faster single pass)
- Empty arrays handled efficiently in all operations

### Edge Cases
- Empty arrays: handled gracefully in all operations
- Duplicate input: automatically normalized
- Same array for both operands: semantics defined (e.g., `intersection(a, a)` = `unique(a)`)
- Negative numbers: treated same as positive numbers

### Mathematical Properties
All operations follow standard set theory:
- Union is commutative: `union(a, b)` = `union(b, a)` (order may differ)
- Intersection is commutative: `intersection(a, b)` = `intersection(b, a)`
- Difference is NOT commutative: `difference(a, b)` ≠ `difference(b, a)`
- De Morgan's laws apply with complement operation
