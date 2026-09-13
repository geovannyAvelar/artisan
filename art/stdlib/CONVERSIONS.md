# ART Conversions Library Reference

Type conversion and formatting utilities for ART runtime. Import with:
```typescript
import { toString, parseNumber, toHex, parseHex, ... } from "art/conversions";
```

## Number to String Conversions

### toString(value: number): string
Converts a number to its string representation. Handles negative numbers, integers, and floating point values.
```typescript
toString(0)       // → "0"
toString(123)     // → "123"
toString(-456)    // → "-456"
toString(1)       // → "1"
```

**Note:** For floating-point numbers, only the integer part is converted. For fractional representation, manually build with string concatenation.

## String to Number Conversions

### parseNumber(str: string): number
Parses a string to a number. Supports integer and decimal notation with optional sign.
- Format: `[+|-]digits[.digits]`
- Returns 0 if string cannot be parsed or is empty
```typescript
parseNumber("0")       // → 0
parseNumber("123")     // → 123
parseNumber("-456")    // → -456
parseNumber("+789")    // → 789
parseNumber("3.5")     // → 3.5
parseNumber("-2.5")    // → -2.5
parseNumber("")        // → 0
parseNumber("abc")     // → 0 (invalid)
```

## Boolean Conversions

### toBoolean(value: number): boolean
Converts a number to boolean.
- 0 or 0.0 → false
- All other numbers → true
```typescript
toBoolean(0)       // → false
toBoolean(0.0)     // → false
toBoolean(1)       // → true
toBoolean(5)       // → true
toBoolean(-1)      // → true
```

### stringToBoolean(str: string): boolean
Converts a string to boolean using multiple matching rules.
- Empty string → false
- "0" → false
- "false", "FALSE", "False" (case-insensitive) → false
- "no", "NO", "No" (case-insensitive) → false
- "off", "OFF", "Off" (case-insensitive) → false
- All other non-empty strings → true
```typescript
stringToBoolean("")       // → false
stringToBoolean("0")      // → false
stringToBoolean("false")  // → false
stringToBoolean("FALSE")  // → false
stringToBoolean("no")     // → false
stringToBoolean("off")    // → false
stringToBoolean("true")   // → true
stringToBoolean("yes")    // → true
stringToBoolean("1")      // → true
```

## Case Conversion

### toLower(str: string): string
Converts all uppercase ASCII letters to lowercase. Non-ASCII characters and digits are unchanged.
```typescript
toLower("ABC")        // → "abc"
toLower("Hello")      // → "hello"
toLower("Hello123")   // → "hello123"
toLower("MiXeD")      // → "mixed"
toLower("")           // → ""
```

### toUpper(str: string): string
Converts all lowercase ASCII letters to uppercase. Non-ASCII characters and digits are unchanged.
```typescript
toUpper("abc")        // → "ABC"
toUpper("hello")      // → "HELLO"
toUpper("hello123")   // → "HELLO123"
toUpper("MiXeD")      // → "MIXED"
```

## Hexadecimal Conversions

### toHex(value: number): string
Converts a number to hexadecimal string representation (lowercase, no "0x" prefix).
- Supports positive and negative numbers
- Negative values are prefixed with "-"
```typescript
toHex(0)        // → "0"
toHex(15)       // → "f"
toHex(16)       // → "10"
toHex(255)      // → "ff"
toHex(256)      // → "100"
toHex(-15)      // → "-f"
toHex(4095)     // → "fff"
```

### parseHex(str: string): number
Parses a hexadecimal string to a number.
- Supports optional "0x" or "0X" prefix
- Supports optional leading "-" sign
- Case-insensitive (both "a" and "A" work)
- Returns 0 if not a valid hex number
```typescript
parseHex("0")       // → 0
parseHex("f")       // → 15
parseHex("F")       // → 15
parseHex("10")      // → 16
parseHex("0x10")    // → 16
parseHex("0X10")    // → 16
parseHex("ff")      // → 255
parseHex("FF")      // → 255
parseHex("-ff")     // → -255
parseHex("xyz")     // → 0 (invalid)
```

**Round-trip:** `parseHex(toHex(n))` returns `n` for any number.

## Binary Conversions

### toBinary(value: number): string
Converts a number to binary string representation (no "0b" prefix).
- Supports positive and negative numbers
- Negative values are prefixed with "-"
```typescript
toBinary(0)      // → "0"
toBinary(1)      // → "1"
toBinary(2)      // → "10"
toBinary(3)      // → "11"
toBinary(4)      // → "100"
toBinary(8)      // → "1000"
toBinary(15)     // → "1111"
toBinary(-8)     // → "-1000"
```

### parseBinary(str: string): number
Parses a binary string to a number.
- Supports optional "0b" or "0B" prefix
- Supports optional leading "-" sign
- Returns 0 if not a valid binary number
```typescript
parseBinary("0")        // → 0
parseBinary("1")        // → 1
parseBinary("10")       // → 2
parseBinary("11")       // → 3
parseBinary("100")      // → 4
parseBinary("1000")     // → 8
parseBinary("0b1010")   // → 10
parseBinary("0B1010")   // → 10
parseBinary("-1010")    // → -10
parseBinary("102")      // → 0 (invalid - '2' is not binary)
```

**Round-trip:** `parseBinary(toBinary(n))` returns `n` for any number.

## Common Patterns

### Number Formatting
While ART conversions don't support printf-style formatting, you can build custom formatting:
```typescript
let hours: number = 9;
let minutes: number = 5;
let formatted: string = toString(hours) + ":" + 
                        (if minutes < 10 then "0" else "") + 
                        toString(minutes);
// Result: "9:05"
```

### Parse with Fallback
```typescript
let value: number = parseNumber(input);
if (value == 0 && input != "0") {
  value = 42;  // Use default if parse failed
}
```

### Case-Insensitive Comparison
```typescript
let userInput: string = "SomeName";
let expected: string = "somename";
if (toLower(userInput) == toLower(expected)) {
  // Names match (case-insensitive)
}
```

### Converting Between Bases
```typescript
let decimal: number = 255;
let hex: string = toHex(decimal);          // → "ff"
let binary: string = toBinary(decimal);    // → "11111111"

let parsed: number = parseHex("ff");       // → 255
let parsed2: number = parseBinary("11111111");  // → 255
```

## Implementation Notes

### Error Handling
- Invalid strings in parse functions return 0 (the zero value)
- There is no null/error type in ART, so invalid parse always yields a default value
- To validate parsing, check if input == "0" before using result

### Performance
- **Fast**: toBoolean, stringToBoolean (~1 operation)
- **Moderate**: toLower, toUpper, toHex, parseHex, toBinary, parseBinary (O(log n) where n is the value magnitude)
- **Variable**: toString, parseNumber (O(d) where d is number of digits)

### String Building
All conversion functions use ART's string concatenation which creates new string objects. For converting many numbers, collect them in a loop and concatenate once at the end for better performance.

### Character Coverage
- **Case conversion**: ASCII letters (A-Z, a-z) only; other characters unchanged
- **Hex digits**: 0-9, a-f, A-F
- **Binary digits**: 0-1
- **Sign handling**: "-" prefix supported; "+" prefix accepted and ignored in parse functions
