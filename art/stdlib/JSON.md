# ART JSON Reference

JSON utilities for serialization and parsing. Import with:
```typescript
import { stringify, parse, stringifyArray, parseArray, isValidJSON } from "art/json";
```

Provides JSON serialization and parsing for numbers and arrays of numbers. Designed for JavaScript compatibility while working within ART's type system.

## Core Functions

### stringify(value: number): string
Converts a number to a JSON string representation.
- Numbers serialize to decimal representation
- Handles positive, negative, and zero values
- Formats with proper decimal precision
- Returns string like "123", "-456", "3.14"

```typescript
stringify(0)             // → "0"
stringify(123)           // → "123"
stringify(-456)          // → "-456"
stringify(3.14)          // → "3.14"
```

### parse(json: string): number
Parses a JSON string to a number.
- Only supports numeric JSON values
- Returns 0 if parsing fails
- Handles positive, negative, and decimal numbers
- Strips leading/trailing whitespace

```typescript
parse("0")               // → 0
parse("123")             // → 123
parse("-456")            // → -456
parse("3.14")            // → 3.14
parse("")                // → 0 (invalid)
parse("abc")             // → 0 (invalid)
```

### stringifyArray(arr: number[]): string
Stringifies an array of numbers to JSON format.
- Returns a JSON array string like "[1, 2, 3]"
- Empty arrays return "[]"
- Numbers are comma-separated with spaces
- Proper JSON array formatting

```typescript
stringifyArray([])       // → "[]"
stringifyArray([1, 2, 3])   // → "[1, 2, 3]"
stringifyArray([0])      // → "[0]"
stringifyArray([-1, 3.14])  // → "[-1, 3.14]"
```

### parseArray(json: string): number[]
Parses a JSON array string to an array of numbers.
- Returns empty array if parsing fails
- Handles brackets "[" and "]"
- Strips whitespace around values
- Returns array of parsed numbers

```typescript
parseArray("")           // → []
parseArray("[]")         // → []
parseArray("[1, 2, 3]")  // → [1, 2, 3]
parseArray("[0]")        // → [0]
parseArray("[ 1 , 2 , 3 ]")  // → [1, 2, 3]
```

### isValidJSON(json: string): boolean
Checks if a string is valid JSON format.
- Returns false for empty strings
- Validates array format: starts with "[", ends with "]"
- Validates number format using numeric string rules
- Strips whitespace for validation

```typescript
isValidJSON("0")         // → true
isValidJSON("123")       // → true
isValidJSON("[]")        // → true
isValidJSON("[1, 2, 3]") // → true
isValidJSON("")          // → false (empty)
isValidJSON("abc")       // → false (invalid)
isValidJSON("[")         // → false (incomplete)
```

## Use Cases

### Numeric Serialization
```typescript
let num: number = 42;
let json: string = stringify(num);  // "42"
let restored: number = parse(json);  // 42
```

### Array Serialization
```typescript
let data: number[] = [1, 2, 3, 4, 5];
let json: string = stringifyArray(data);
// "[1, 2, 3, 4, 5]"

let restored: number[] = parseArray(json);
// [1, 2, 3, 4, 5]
```

### Round-Trip Conversion
```typescript
let original: number = 3.14159;
let serialized: string = stringify(original);
let parsed: number = parse(serialized);
// parsed ≈ original (within floating-point precision)
```

### Data Validation
```typescript
let input: string = getUserInput();
if (isValidJSON(input)) {
  let value: number = parse(input);
  // Safe to use value
}
```

### Array Processing
```typescript
let dataStr: string = "[10, 20, 30, 40, 50]";
let arr: number[] = parseArray(dataStr);

let i: number = 0;
while (i < arr.length) {
  arr[i] = arr[i] * 2;  // Transform
  i = i + 1;
}

let result: string = stringifyArray(arr);
// "[20, 40, 60, 80, 100]"
```

## Supported Formats

### Number Format
- Integers: "0", "123", "-456"
- Decimals: "3.14", "-2.5"
- Exponential: Not directly supported (use decimal representation)
- Leading zeros: Parsed as decimal (e.g., "007" → 7)

### Array Format
- Empty: "[]"
- Single element: "[42]"
- Multiple elements: "[1, 2, 3]"
- Whitespace: "[ 1 , 2 , 3 ]" (automatically stripped)
- Decimals: "[1.5, 2.5, 3.5]"
- Negatives: "[-1, -2, -3]"

### Invalid Formats (return default values)
- Empty string: ""
- Non-numeric: "abc", "null", "true"
- Incomplete arrays: "[1, 2", "1, 2]"
- Object notation: "{}" (not supported)
- Nested arrays: "[[1, 2], [3, 4]]" (not supported)

## Implementation Notes

### Number Conversion
- Uses character-by-character conversion
- Handles decimal point parsing
- Supports +/- signs
- Precision limited by floating-point representation

### Array Parsing
- Bracket removal (strips "[" and "]")
- Comma-based element splitting
- Whitespace trimming around elements
- Individual element parsing via parseNumber

### Validation Strategy
- Empty check first (quick path)
- Format checking (array vs number)
- Semantic validation (actually parseable)

### Performance Characteristics
- **stringify**: O(n) where n is digit count
- **parse**: O(n) where n is string length
- **stringifyArray**: O(m*n) where m is array length, n is avg digits
- **parseArray**: O(m*n) where m is array length, n is avg chars per element
- **isValidJSON**: O(n) where n is string length

## Error Handling

All functions use sensible defaults:
- Invalid input → 0 (for numbers) or [] (for arrays)
- Empty input → 0 (for numbers) or [] (for arrays)
- Incomplete data → best-effort parse up to error point
- No exceptions thrown (ART style)

This matches JavaScript's parseFloat/parseInt behavior of returning NaN on invalid input, adapted to ART's numeric error model.

## Comparison with JavaScript

| Feature | ART | JavaScript |
|---------|-----|-----------|
| Number stringify | Custom | JSON.stringify() |
| Number parse | Custom | JSON.parse() or parseFloat() |
| Array stringify | Custom | JSON.stringify() |
| Array parse | Custom | JSON.parse() |
| Error handling | Returns 0/[] | Throws or returns NaN |
| Type coverage | Numbers only | All types |
| Precision | IEEE 754 | IEEE 754 |

## Common Patterns

### Data Persistence (Simulated)
```typescript
let data: number[] = [10, 20, 30];
let saved: string = stringifyArray(data);
// Store saved string somewhere
let loaded: number[] = parseArray(saved);
// Use loaded data
```

### Configuration Parsing
```typescript
let configStr: string = "[1, 1, 0, 1]";  // flags as array
if (isValidJSON(configStr)) {
  let config: number[] = parseArray(configStr);
  // config[0], config[1], etc.
}
```

### Safe Type Conversion
```typescript
let input: number = 42;
let json: string = stringify(input);
let output: number = parse(json);
if (input == output) {
  // Round-trip succeeded
}
```
