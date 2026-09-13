# ART Globals Reference

Global utility functions for common operations. Import with:
```typescript
import { parseInt, parseFloat, isNaN, isFinite, ... } from "art/globals";
```

JavaScript-compatible global functions including parsing, type checking, and math utilities.

## Parsing Functions

### parseInt(str: string, radix: number): number
Parses a string to an integer with specified radix (base).
- Radix: 2-36 (defaults to 10, or 16 if string starts with "0x")
- Returns 0 if parsing fails or string is empty
- Handles optional +/- signs
- Auto-detects hexadecimal with 0x/0X prefix

```typescript
parseInt("123", 10)      // → 123
parseInt("-456", 10)     // → -456
parseInt("1010", 2)      // → 10 (binary)
parseInt("FF", 16)       // → 255 (hexadecimal)
parseInt("0x10", 10)     // → 16 (auto-detect hex)
parseInt("", 10)         // → 0 (empty)
parseInt("abc", 10)      // → 0 (invalid)
```

### parseFloat(str: string): number
Parses a string to a floating-point number.
- Returns 0 if parsing fails or string is empty
- Handles optional +/- signs
- Parses integer and fractional parts
- Stops at first non-digit character (excluding decimal point)

```typescript
parseFloat("3.14")       // → 3.14
parseFloat("-2.5")       // → -2.5
parseFloat("123.456")    // → 123.456
parseFloat("")           // → 0 (empty)
parseFloat("abc")        // → 0 (invalid)
```

## Type Checking Functions

### isNaN(value: number): boolean
Checks if a value is NaN (Not a Number).
- Returns true only if value is NaN (0/0 result)
- NaN is the only value not equal to itself
- Can be used to detect failed numerical operations

```typescript
let nan: number = 0 / 0;
isNaN(nan)               // → true
isNaN(5)                 // → false
isNaN(0)                 // → false
```

### isFinite(value: number): boolean
Checks if a value is finite (not infinity and not NaN).
- Returns false for NaN
- Returns false for very large values (practical infinity threshold)
- Returns true for normal numeric values

```typescript
isFinite(0)              // → true
isFinite(123.456)        // → true
isFinite(-999)           // → true
let nan: number = 0 / 0;
isFinite(nan)            // → false
```

### typeOf(value: number): string
Gets the type of a value as a string.
- Always returns "number" since ART is primarily number-based
- Provided for JavaScript compatibility

```typescript
typeOf(0)                // → "number"
typeOf(123.456)          // → "number"
typeOf(-999)             // → "number"
```

### isInteger(value: number): boolean
Checks if a value is an integer (no fractional part).
- Returns true for whole numbers
- Returns false for decimal values

```typescript
isInteger(5)             // → true
isInteger(0)             // → true
isInteger(-10)           // → true
isInteger(3.14)          // → false
isInteger(0.5)           // → false
```

### isSafeInteger(value: number): boolean
Checks if a value is a safe integer (-2^53 to 2^53).
- Returns true for integers within safe range
- Returns false for floats or values outside range
- Safe range: -9007199254740991 to 9007199254740991

```typescript
isSafeInteger(0)         // → true
isSafeInteger(100)       // → true
isSafeInteger(-999)      // → true
isSafeInteger(3.14)      // → false (not integer)
isSafeInteger(10000000000000000)  // → false (too large)
```

### isPositive(value: number): boolean
Checks if a value is positive (> 0).

```typescript
isPositive(1)            // → true
isPositive(0.001)        // → true
isPositive(0)            // → false
isPositive(-1)           // → false
```

### isNegative(value: number): boolean
Checks if a value is negative (< 0).

```typescript
isNegative(-1)           // → true
isNegative(-0.001)       // → true
isNegative(0)            // → false
isNegative(1)            // → false
```

### isZero(value: number): boolean
Checks if a value is zero.

```typescript
isZero(0)                // → true
isZero(0.0001)           // → false
isZero(-1)               // → false
isZero(1)                // → false
```

## Math Utility Functions

### toNumber(value: number): number
Converts a value to a number (identity function for compatibility).
- Always returns the input value
- Provided for JavaScript compatibility

```typescript
toNumber(5)              // → 5
toNumber(-3.14)          // → -3.14
```

### abs(value: number): number
Absolute value (distance from zero).
- Returns positive version of number
- abs(0) returns 0

```typescript
abs(5)                   // → 5
abs(-5)                  // → 5
abs(0)                   // → 0
abs(-3.14)               // → 3.14
```

### round(value: number): number
Rounds to nearest integer.
- 0.5 rounds away from zero
- Positive numbers: floor(x + 0.5)
- Negative numbers: ceil(x - 0.5)

```typescript
round(3.2)               // → 3
round(3.7)               // → 4
round(-2.3)              // → -2
round(-2.7)              // → -3
round(5)                 // → 5
```

### trunc(value: number): number
Truncates toward zero (removes fractional part).
- Same as floor for positive numbers
- Same as ceil for negative numbers

```typescript
trunc(3.9)               // → 3
trunc(-3.9)              // → -3
trunc(5)                 // → 5
```

### floor(value: number): number
Greatest integer <= value (rounds down).
- Uses iterative integer search
- floor(3.9) returns 3
- floor(-3.1) returns -4

```typescript
floor(3.9)               // → 3
floor(-3.1)              // → -4
floor(5)                 // → 5
```

### ceil(value: number): number
Smallest integer >= value (rounds up).
- Uses floor + 1 for non-integers
- ceil(3.1) returns 4
- ceil(-3.9) returns -3

```typescript
ceil(3.1)                // → 4
ceil(-3.9)               // → -3
ceil(5)                  // → 5
```

### sign(value: number): number
Gets the sign of a number.
- Returns 1 for positive
- Returns -1 for negative
- Returns 0 for zero

```typescript
sign(5)                  // → 1
sign(-5)                 // → -1
sign(0)                  // → 0
```

### clamp(value: number, min: number, max: number): number
Constrains value between min and max (inclusive).
- Returns min if value < min
- Returns max if value > max
- Returns value otherwise

```typescript
clamp(5, 0, 10)          // → 5
clamp(-5, 0, 10)         // → 0 (clamped to min)
clamp(15, 0, 10)         // → 10 (clamped to max)
```

### isBetween(value: number, min: number, max: number): boolean
Checks if value is between min and max (inclusive).

```typescript
isBetween(5, 0, 10)      // → true
isBetween(0, 0, 10)      // → true (inclusive)
isBetween(10, 0, 10)     // → true (inclusive)
isBetween(-1, 0, 10)     // → false
isBetween(11, 0, 10)     // → false
```

### min(a: number, b: number): number
Gets the minimum of two values.

```typescript
min(5, 10)               // → 5
min(-5, -10)             // → -10
min(0, 0)                // → 0
```

### max(a: number, b: number): number
Gets the maximum of two values.

```typescript
max(5, 10)               // → 10
max(-5, -10)             // → -5
max(0, 0)                // → 0
```

## Common Patterns

### Type Checking Pipeline
```typescript
let value: number = 42;

// Check basic properties
if (isNaN(value)) { /* handle NaN */ }
if (!isFinite(value)) { /* handle infinity */ }
if (isInteger(value)) { /* process as integer */ }
```

### Parsing with Validation
```typescript
let str: string = "123.45";
let num: number = parseFloat(str);

if (isFinite(num)) {
  if (isInteger(num)) {
    // Use as integer
  } else {
    // Use as float
  }
}
```

### Radix-Based Parsing
```typescript
let binary: number = parseInt("1010", 2);      // 10
let octal: number = parseInt("755", 8);        // 493
let hex: number = parseInt("FF", 16);          // 255
let auto: number = parseInt("0xFF", 10);       // 255 (auto-detect)
```

### Value Bounding
```typescript
let raw: number = 256;
let bounded: number = clamp(raw, 0, 255);  // Ensure in range [0, 255]
```

## Implementation Notes

### Radix Support
- parseInt supports radix 2-36
- Hex (0-9, a-f/A-F)
- Octal (0-7)
- Binary (0-1)
- Auto-detection of 0x/0X prefix when radix is 10

### Parsing Behavior
- Empty strings return 0 (no error state in ART)
- Invalid characters stop parsing immediately
- Optional +/- signs at start
- Floating point parsing stops at first invalid char

### Numeric Limits
- Safe integer range: ±2^53 - 1 (±9007199254740991)
- Practical infinity threshold: ±999999999999999
- NaN is only value where x != x

### Compatibility
- All functions designed for JavaScript API compatibility
- Can be imported and used like JavaScript globals
- Follow JavaScript semantics where possible
