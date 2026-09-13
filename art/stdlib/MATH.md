# ART Math Library Reference

Mathematical functions and constants for ART runtime. Import with:
```typescript
import { PI, E, abs, sqrt, pow, ... } from "art/math";
```

## Constants

### PI
```typescript
export const PI: number = 3.141592653589793;
```
The mathematical constant π (pi), accurate to double precision.

### E
```typescript
export const E: number = 2.718281828459045;
```
The mathematical constant e (Euler's number), base of natural logarithm.

## Basic Functions

### abs(x: number): number
Returns the absolute value of a number.
```typescript
abs(5)      // → 5
abs(-5)     // → 5
abs(0)      // → 0
```

### min(a: number, b: number): number
Returns the minimum of two numbers.
```typescript
min(3, 5)   // → 3
min(5, 3)   // → 3
```

### max(a: number, b: number): number
Returns the maximum of two numbers.
```typescript
max(3, 5)   // → 5
max(5, 3)   // → 5
```

### clamp(x: number, minVal: number, maxVal: number): number
Clamps a value between min and max (inclusive).
```typescript
clamp(5, 1, 10)    // → 5
clamp(0, 1, 10)    // → 1
clamp(15, 1, 10)   // → 10
```

### sign(x: number): number
Returns the sign of a number: -1 (negative), 0 (zero), or 1 (positive).
```typescript
sign(5)     // → 1
sign(-5)    // → -1
sign(0)     // → 0
```

## Array Aggregates

### minArray(arr: number[]): number
Returns the minimum of an array of numbers. Returns 0 for empty arrays.
```typescript
minArray([3, 1, 4, 1, 5, 9])   // → 1
minArray([42])                  // → 42
minArray([])                    // → 0
```

### maxArray(arr: number[]): number
Returns the maximum of an array of numbers. Returns 0 for empty arrays.
```typescript
maxArray([3, 1, 4, 1, 5, 9])   // → 9
maxArray([42])                  // → 42
```

### sum(arr: number[]): number
Returns the sum of all numbers in an array.
```typescript
sum([1, 2, 3, 4, 5])   // → 15
sum([])                 // → 0
sum([1, -1])            // → 0
```

### average(arr: number[]): number
Returns the average (mean) of all numbers in an array. Returns 0 for empty arrays.
```typescript
average([2, 4, 6])     // → 4
average([10, 20])      // → 15
average([])            // → 0
```

### product(arr: number[]): number
Returns the product of all numbers in an array. Returns 1 for empty arrays.
```typescript
product([2, 3, 4])     // → 24
product([2, 2, 2])     // → 8
product([0, 5])        // → 0
product([])            // → 1
```

## Rounding Functions

### floor(x: number): number
Returns the greatest integer ≤ x (rounds down). Works for both positive and negative numbers.
```typescript
floor(3.7)     // → 3
floor(3.2)     // → 3
floor(-3.7)    // → -4
floor(3)       // → 3
```

**Note:** Implemented without calling C runtime floor(). Uses iterative integer search approach.

### ceil(x: number): number
Returns the smallest integer ≥ x (rounds up). Opposite of floor.
```typescript
ceil(3.2)      // → 4
ceil(3.7)      // → 4
ceil(3)        // → 3
ceil(-3.7)     // → -3
```

### round(x: number): number
Rounds x to the nearest integer. 0.5 rounds up (away from zero for positive, toward zero for negative).
```typescript
round(3.2)     // → 3
round(3.5)     // → 4
round(3.7)     // → 4
round(-3.5)    // → -3
```

### trunc(x: number): number
Truncates x toward zero (removes fractional part).
```typescript
trunc(3.7)     // → 3
trunc(3.2)     // → 3
trunc(-3.7)    // → -3
```

## Advanced Functions

### sqrt(x: number): number
Returns the square root of x using Newton's method (20 iterations for precision).
- For negative numbers, returns 0
- Accurate to ~7 decimal places
```typescript
sqrt(4)        // → 2
sqrt(9)        // → 3
sqrt(2)        // → ~1.414
sqrt(1)        // → 1
sqrt(0)        // → 0
sqrt(-1)       // → 0
```

**Note:** Pure ART implementation using Newton's method iterative approximation.

### pow(x: number, y: number): number
Returns x raised to the power y (x^y). Handles integer and fractional exponents.
- For integer exponents: uses repeated multiplication (exponentiation by squaring)
- For fractional exponents: approximates using floor/frac decomposition
- Supports negative exponents
```typescript
pow(2, 3)      // → 8
pow(2, 0)      // → 1
pow(2, -1)     // → 0.5
pow(3, 2)      // → 9
```

## Utility Functions

### remainder(x: number, y: number): number
Returns the remainder of x divided by y (similar to x % y). Returns 0 if y is 0.
```typescript
remainder(7, 3)    // → 1
remainder(10, 2)   // → 0
remainder(5, 5)    // → 0
```

### frac(x: number): number
Returns the fractional part of a number (x - floor(x)). Result is in [0, 1) for positive x.
```typescript
frac(3.7)      // → ~0.7
frac(3)        // → 0
frac(-3.7)     // → ~-0.7
```

### gcd(a: number, b: number): number
Returns the greatest common divisor of a and b using Euclidean algorithm. Works with negative numbers.
```typescript
gcd(12, 8)     // → 4
gcd(17, 19)    // → 1
gcd(100, 50)   // → 50
gcd(0, 5)      // → 5
```

### lcm(a: number, b: number): number
Returns the least common multiple of a and b. Returns 0 if either input is 0.
```typescript
lcm(4, 6)      // → 12
lcm(3, 5)      // → 15
lcm(0, 5)      // → 0
```

## Predicates (Boolean Functions)

### isInteger(x: number): boolean
Returns true if x is an integer (no fractional part).
```typescript
isInteger(5)       // → true
isInteger(5.5)     // → false
isInteger(-3)      // → true
```

### isEven(x: number): boolean
Returns true if x is even (for integer x). Uses remainder(x, 2).
```typescript
isEven(4)          // → true
isEven(5)          // → false
```

### isOdd(x: number): boolean
Returns true if x is odd (for integer x).
```typescript
isOdd(5)           // → true
isOdd(4)           // → false
```

### isPowerOf2(x: number): boolean
Returns true if x is a power of 2. Returns false for x ≤ 0.
```typescript
isPowerOf2(1)      // → true
isPowerOf2(2)      // → true
isPowerOf2(4)      // → true
isPowerOf2(3)      // → false
isPowerOf2(0)      // → false
```

## Power of 2 Functions

### nextPowerOf2(x: number): number
Returns the next power of 2 ≥ x. Returns 1 for x ≤ 1.
```typescript
nextPowerOf2(1)    // → 1
nextPowerOf2(3)    // → 4
nextPowerOf2(5)    // → 8
nextPowerOf2(8)    // → 8
nextPowerOf2(9)    // → 16
```

## Interpolation and Mapping

### lerp(a: number, b: number, t: number): number
Linear interpolation between a and b by factor t. t should be in [0, 1] for normal interpolation.
```typescript
lerp(0, 10, 0)     // → 0
lerp(0, 10, 1)     // → 10
lerp(0, 10, 0.5)   // → 5
lerp(5, 15, 0.5)   // → 10
```

### inverseLerp(a: number, b: number, x: number): number
Inverse linear interpolation - returns how far x is between a and b as a factor in [0, 1].
Returns 0 if a == b.
```typescript
inverseLerp(0, 10, 0)      // → 0
inverseLerp(0, 10, 10)     // → 1
inverseLerp(0, 10, 5)      // → 0.5
```

### map(x: number, inMin: number, inMax: number, outMin: number, outMax: number): number
Maps a value from one range [inMin, inMax] to another range [outMin, outMax].
Returns outMin if inMin == inMax.
```typescript
map(5, 0, 10, 0, 100)      // → 50
map(0, 0, 10, 100, 200)    // → 100
map(10, 0, 10, 100, 200)   // → 200
```

## Angle Conversion

### degreesToRadians(degrees: number): number
Converts degrees to radians (multiplies by π/180).
```typescript
degreesToRadians(0)        // → 0
degreesToRadians(180)      // → ~3.14159 (π)
degreesToRadians(90)       // → ~1.5708 (π/2)
```

### radiansToDegrees(radians: number): number
Converts radians to degrees (multiplies by 180/π).
```typescript
radiansToDegrees(0)        // → 0
radiansToDegrees(PI)       // → 180
radiansToDegrees(PI / 2)   // → 90
```

## Implementation Notes

### Pure ART Implementation
All functions are implemented in pure ART without C++ FFI calls:
- **floor/ceil**: Use iterative integer search instead of C runtime functions
- **sqrt**: Uses Newton's method (20 iterations for double precision)
- **pow**: Uses exponentiation by squaring for integers, approximation for fractional exponents
- **No external dependencies**: All math is self-contained

### Performance Characteristics
- **Fast**: abs, min, max, sign, clamp, frac (~1 operation)
- **Moderate**: floor, ceil, round, trunc, isInteger, isEven, isOdd (O(n) where n is integer part)
- **Slower**: sqrt (20 Newton iterations), pow with exponents (variable iterations)
- **Array operations**: O(n) where n is array length

### Accuracy Notes
- Constants (PI, E): Double precision (IEEE 754)
- sqrt: ~7 decimal places accuracy after 20 iterations
- Trigonometric conversions: Accurate to ~10 decimal places
- floor/ceil for large integers: Works correctly for all double-precision integers
