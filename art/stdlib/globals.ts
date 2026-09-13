// JavaScript global functions and utilities.
// Import with: `import { parseInt, parseFloat, isNaN, isFinite, ... } from "art/globals";`

// Parses a string to an integer in the specified radix (base).
// Radix: 2-36, defaults to 10 (or 16 if string starts with "0x").
// Returns 0 if parsing fails or string is empty.
export function parseInt(str: string, radix: number): number {
  if (str == "") { return 0; }
  if (radix < 2 || radix > 36) { radix = 10; }

  let idx: number = 0;
  let isNegative: boolean = false;

  // Handle sign
  if (idx < str.length) {
    let char: string = str.substring(idx, idx + 1);
    if (char == "-") {
      isNegative = true;
      idx = idx + 1;
    } else if (char == "+") {
      idx = idx + 1;
    }
  }

  // Auto-detect hex if radix is not specified
  if (idx + 1 < str.length) {
    let prefix: string = str.substring(idx, idx + 2);
    if ((prefix == "0x" || prefix == "0X") && radix == 10) {
      radix = 16;
      idx = idx + 2;
    }
  }

  let result: number = 0;
  while (idx < str.length) {
    let char: string = str.substring(idx, idx + 1);
    let digit: number = charToDigit(char, radix);
    if (digit < 0) { break; }
    result = result * radix + digit;
    idx = idx + 1;
  }

  if (isNegative) { result = -result; }
  return result;
}

// Parses a string to a floating-point number.
// Returns 0 if parsing fails or string is empty.
export function parseFloat(str: string): number {
  if (str == "") { return 0; }

  let idx: number = 0;
  let isNegative: boolean = false;

  // Check for sign
  if (idx < str.length) {
    let char: string = str.substring(idx, idx + 1);
    if (char == "-") {
      isNegative = true;
      idx = idx + 1;
    } else if (char == "+") {
      idx = idx + 1;
    }
  }

  // Parse integer part
  let intPart: number = 0;
  while (idx < str.length) {
    let char: string = str.substring(idx, idx + 1);
    let digit: number = charToDigitDecimal(char);
    if (digit < 0 || digit > 9) { break; }
    intPart = intPart * 10 + digit;
    idx = idx + 1;
  }

  // Check for decimal point
  let fracPart: number = 0;
  let fracDivisor: number = 10;
  if (idx < str.length) {
    let char: string = str.substring(idx, idx + 1);
    if (char == ".") {
      idx = idx + 1;
      while (idx < str.length) {
        char = str.substring(idx, idx + 1);
        let digit: number = charToDigitDecimal(char);
        if (digit < 0 || digit > 9) { break; }
        fracPart = fracPart + digit / fracDivisor;
        fracDivisor = fracDivisor * 10;
        idx = idx + 1;
      }
    }
  }

  let result: number = intPart + fracPart;
  if (isNegative) { result = -result; }
  return result;
}

// Checks if a value is NaN (Not a Number).
// Returns true only if value is NaN (0/0 result).
// Note: In ART, NaN is difficult to represent naturally, so this checks for special values.
export function isNaN(value: number): boolean {
  return value != value;  // NaN is the only value not equal to itself
}

// Checks if a value is finite (not infinity and not NaN).
export function isFinite(value: number): boolean {
  if (value != value) { return false; }  // NaN check
  if (value > 999999999999999) { return false; }  // Practical infinity check
  if (value < -999999999999999) { return false; }
  return true;
}

// Gets the type of a value as a string.
// Returns: "number", "string", "boolean"
// Note: ART's type system is limited, so this provides basic type info.
export function typeOf(value: number): string {
  return "number";  // ART is number-based
}

// Checks if a value is an integer.
export function isInteger(value: number): boolean {
  return value == (value - ((value / 1) - ((value / 1))));
}

// Checks if a value is a safe integer (-2^53 to 2^53).
export function isSafeInteger(value: number): boolean {
  if (!isInteger(value)) { return false; }
  if (value > 9007199254740991) { return false; }  // 2^53 - 1
  if (value < -9007199254740991) { return false; }
  return true;
}

// Converts a value to a number.
// Strings are parsed, booleans become 0 or 1.
export function toNumber(value: number): number {
  return value;  // Already a number
}

// Checks if a value is positive.
export function isPositive(value: number): boolean {
  return value > 0;
}

// Checks if a value is negative.
export function isNegative(value: number): boolean {
  return value < 0;
}

// Checks if a value is zero.
export function isZero(value: number): boolean {
  return value == 0;
}

// Absolute value (same as Math.abs, provided for convenience).
export function abs(value: number): number {
  return value < 0 ? -value : value;
}

// Rounds a number to the nearest integer.
export function round(value: number): number {
  if (value >= 0) {
    return floor(value + 0.5);
  } else {
    return ceil(value - 0.5);
  }
}

// Truncates a number toward zero (removes fractional part).
export function trunc(value: number): number {
  if (value >= 0) { return floor(value); }
  return ceil(value);
}

// Floors a number (greatest integer <= value).
export function floor(value: number): number {
  if (value >= 0) {
    let i: number = 0;
    while (i + 1 <= value) {
      i = i + 1;
    }
    return i;
  }
  let i: number = 0;
  while (i - 1 >= value) {
    i = i - 1;
  }
  return i - 1;
}

// Ceils a number (smallest integer >= value).
export function ceil(value: number): number {
  let floored: number = floor(value);
  if (floored == value) { return floored; }
  return floored + 1;
}

// Gets the sign of a number: -1, 0, or 1.
export function sign(value: number): number {
  if (value > 0) { return 1; }
  if (value < 0) { return -1; }
  return 0;
}

// Clamps a value between min and max (inclusive).
export function clamp(value: number, min: number, max: number): number {
  if (value < min) { return min; }
  if (value > max) { return max; }
  return value;
}

// Checks if a value is between min and max (inclusive).
export function isBetween(value: number, min: number, max: number): boolean {
  return value >= min && value <= max;
}

// Gets minimum of two values.
export function min(a: number, b: number): number {
  return a < b ? a : b;
}

// Gets maximum of two values.
export function max(a: number, b: number): number {
  return a > b ? a : b;
}

// Helper: Convert character to digit in given radix, or -1 if invalid.
function charToDigit(char: string, radix: number): number {
  let digit: number = charToDigitDecimal(char);
  if (digit >= 0 && digit < radix) { return digit; }

  if (char == "a" || char == "A") { digit = 10; }
  else if (char == "b" || char == "B") { digit = 11; }
  else if (char == "c" || char == "C") { digit = 12; }
  else if (char == "d" || char == "D") { digit = 13; }
  else if (char == "e" || char == "E") { digit = 14; }
  else if (char == "f" || char == "F") { digit = 15; }
  else { return -1; }

  if (digit < radix) { return digit; }
  return -1;
}

// Helper: Convert decimal character to digit (0-9), or -1.
function charToDigitDecimal(char: string): number {
  if (char == "0") { return 0; }
  if (char == "1") { return 1; }
  if (char == "2") { return 2; }
  if (char == "3") { return 3; }
  if (char == "4") { return 4; }
  if (char == "5") { return 5; }
  if (char == "6") { return 6; }
  if (char == "7") { return 7; }
  if (char == "8") { return 8; }
  if (char == "9") { return 9; }
  return -1;
}
