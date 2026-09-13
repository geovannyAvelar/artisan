# String Methods in ART

ART now includes a standard library module providing utility functions for working with strings.

## Overview

String methods are implemented as exported functions in the `strings` module, all working with ART's built-in `string` type.

## Importing

```ts
import { 
  charAt, startsWith, endsWith, indexOf, lastIndexOf, includes,
  substring, slice, trim, replaceAll, isBlank, repeat, padStart, padEnd
} from "art/strings";
```

## Methods

### Character Access

#### `charAt(s: string, index: number): string`

Returns the character at the specified index as a single-character string, or empty string if out of bounds.

```ts
let s: string = "hello";
let c: string = charAt(s, 1);  // "e"
let x: string = charAt(s, 10);  // ""
```

### Search & Match

#### `startsWith(s: string, prefix: string): boolean`

Returns `true` if the string starts with the given prefix.

```ts
if (startsWith("hello world", "hello")) {
  // true
}
```

#### `endsWith(s: string, suffix: string): boolean`

Returns `true` if the string ends with the given suffix.

```ts
if (endsWith("hello.txt", ".txt")) {
  // true
}
```

#### `indexOf(s: string, substring: string): number`

Returns the index of the first occurrence of the substring, or `-1` if not found.

```ts
let idx: number = indexOf("hello world", "world");  // 6
let notFound: number = indexOf("hello", "xyz");     // -1
```

#### `lastIndexOf(s: string, substring: string): number`

Returns the index of the last occurrence of the substring, or `-1` if not found.

```ts
let idx: number = lastIndexOf("hello hello", "hello");  // 6
```

#### `includes(s: string, substring: string): boolean`

Returns `true` if the string contains the substring.

```ts
if (includes("hello world", "world")) {
  // true
}
```

### Substring Extraction

#### `substring(s: string, start: number, end: number): string`

Returns a substring from `start` to `end` (exclusive). If `start > end`, they are swapped.

```ts
let sub: string = substring("hello", 1, 4);  // "ell"
```

#### `slice(s: string, start: number, length: number): string`

Returns a substring starting at `start` with the given length.

```ts
let sub: string = slice("hello", 1, 3);  // "ell"
```

### Transformation

#### `trim(s: string): string`

Returns a new string with leading and trailing whitespace removed.

```ts
let cleaned: string = trim("  hello world  ");  // "hello world"
```

#### `replaceAll(s: string, search: string, replacement: string): string`

Returns a new string with all occurrences of `search` replaced with `replacement`.

```ts
let result: string = replaceAll("hello hello", "hello", "hi");  // "hi hi"
```

#### `repeat(s: string, count: number): string`

Returns a new string with the string repeated `count` times.

```ts
let result: string = repeat("ab", 3);  // "ababab"
```

#### `padStart(s: string, length: number, padString: string): string`

Pads the string with `padString` on the left to reach the specified length.

```ts
let padded: string = padStart("5", 3, "0");  // "005"
```

#### `padEnd(s: string, length: number, padString: string): string`

Pads the string with `padString` on the right to reach the specified length.

```ts
let padded: string = padEnd("5", 3, "0");  // "500"
```

### Query & Inspection

#### `isBlank(s: string): boolean`

Returns `true` if the string contains only whitespace (or is empty).

```ts
if (isBlank("   ")) {
  // true
}
```

#### `concat(s: string, other: string): string`

Concatenates two strings. (Equivalent to `s + other`, provided as a named function.)

```ts
let result: string = concat("hello", " world");  // "hello world"
```

## Built-in String Operations

ART's `string` type also has built-in operations that don't require importing:

```ts
let s: string = "hello";

// Length
let len: number = s.length;  // 5

// Indexing
let c: string = s[0] + "";  // "h" - must convert to string
let code: number = s[0];     // Character code (char is a number)

// Concatenation
let result: string = "hello" + " " + "world";

// Comparison
if (s == "hello") { /* ... */ }
if (s != "world") { /* ... */ }
```

## Pattern: Building Strings

Since string methods are immutable and functions (not methods on the type), use composition:

```ts
let result: string = trim(replaceAll("  hello  ", "hello", "hi"));  // "hi"
```

## Limitations & Workarounds

### split()

ART's type system makes it difficult to return an array of strings from a generic function. For now, manually split using a custom loop with `indexOf`:

```ts
function splitByComma(s: string): number {
  // Returns count of parts; use manual iteration to populate array
  let count: number = 1;
  let i: number = 0;
  while (i < s.length) {
    if (s[i] == ',') { count = count + 1; }
    i = i + 1;
  }
  return count;
}
```

### Case Conversion

Lower/upper case conversion requires more complex handling of character codes. For ASCII:

```ts
function toUpperAscii(s: string): string {
  let result: string = "";
  let i: number = 0;
  while (i < s.length) {
    let c: number = s[i];
    if (c >= 97 && c <= 122) {  // a-z
      c = c - 32;  // Convert to uppercase ASCII
    }
    result = result + (c + "");
    i = i + 1;
  }
  return result;
}
```

## Examples

### Validate Email Domain

```ts
function hasEmailDomain(email: string, domain: string): boolean {
  if (!includes(email, "@")) { return false; }
  let atIdx: number = indexOf(email, "@");
  let domainPart: string = substring(email, atIdx + 1, email.length);
  return includes(domainPart, domain);
}
```

### Extract File Extension

```ts
function getExtension(filename: string): string {
  let dotIdx: number = lastIndexOf(filename, ".");
  if (dotIdx < 0) { return ""; }
  return substring(filename, dotIdx + 1, filename.length);
}
```

### Format Number with Padding

```ts
function formatNumber(n: number, width: number): string {
  return padStart(numberToString(n), width, "0");
}
```

### Sanitize Input

```ts
function sanitize(input: string): string {
  return trim(replaceAll(input, "<", "&lt;"));
}
```
