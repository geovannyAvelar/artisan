// Type conversion utilities for ART.
// Import with: `import { toString, parseNumber, toBoolean, ... } from "art/conversions";`

// Converts a number to its string representation.
// Handles negative numbers, integers, and floating point values.
export function toString(value: number): string {
  if (value == 0) { return "0"; }

  let isNegative: boolean = false;
  if (value < 0) {
    isNegative = true;
    value = -value;
  }

  let result: string = "";
  let intPart: number = 0;
  while (value >= 1) {
    intPart = intPart * 10;
    let digit: number = value - ((value / 10) * 10);
    intPart = intPart + digit;
    value = value / 10;
  }

  // Build string from integer part
  if (intPart == 0) {
    result = "0";
  } else {
    while (intPart > 0) {
      let digit: number = intPart - ((intPart / 10) * 10);
      let char: string = digitToChar(digit);
      result = char + result;
      intPart = intPart / 10;
    }
  }

  if (isNegative) { result = "-" + result; }
  return result;
}

// Helper: converts a digit (0-9) to its character ('0'-'9').
function digitToChar(digit: number): string {
  if (digit == 0) { return "0"; }
  if (digit == 1) { return "1"; }
  if (digit == 2) { return "2"; }
  if (digit == 3) { return "3"; }
  if (digit == 4) { return "4"; }
  if (digit == 5) { return "5"; }
  if (digit == 6) { return "6"; }
  if (digit == 7) { return "7"; }
  if (digit == 8) { return "8"; }
  if (digit == 9) { return "9"; }
  return "?";
}

// Converts a string to a number.
// Returns the parsed number or 0 if the string cannot be parsed.
// Supports: "123", "-456", "789.5", "0", "-0.5"
export function parseNumber(str: string): number {
  if (str == "") { return 0; }

  let idx: number = 0;
  let isNegative: boolean = false;

  // Check for leading sign
  if (str != "") {
    let firstChar: string = "";
    if (idx < str.length) {
      firstChar = str.substring(idx, idx + 1);
    }
    if (firstChar == "-") {
      isNegative = true;
      idx = idx + 1;
    } else if (firstChar == "+") {
      idx = idx + 1;
    }
  }

  // Parse integer part
  let intPart: number = 0;
  while (idx < str.length) {
    let char: string = str.substring(idx, idx + 1);
    let digit: number = charToDigit(char);
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
        let digit: number = charToDigit(char);
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

// Helper: converts a character ('0'-'9') to its digit value (0-9), or -1 if not a digit.
function charToDigit(char: string): number {
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

// Converts a number to a boolean.
// 0 and 0.0 -> false, all other numbers -> true.
export function toBoolean(value: number): boolean {
  return value != 0;
}

// Converts a string to a boolean.
// Empty string, "0", "false" (case-insensitive) -> false
// All other non-empty strings -> true
export function stringToBoolean(str: string): boolean {
  if (str == "") { return false; }
  if (str == "0") { return false; }
  let lower: string = toLower(str);
  if (lower == "false") { return false; }
  if (lower == "no") { return false; }
  if (lower == "off") { return false; }
  return true;
}

// Converts a string to lowercase.
export function toLower(str: string): string {
  let result: string = "";
  let i: number = 0;
  while (i < str.length) {
    let char: string = str.substring(i, i + 1);
    result = result + charToLower(char);
    i = i + 1;
  }
  return result;
}

// Converts a character to lowercase.
function charToLower(char: string): string {
  if (char == "A") { return "a"; }
  if (char == "B") { return "b"; }
  if (char == "C") { return "c"; }
  if (char == "D") { return "d"; }
  if (char == "E") { return "e"; }
  if (char == "F") { return "f"; }
  if (char == "G") { return "g"; }
  if (char == "H") { return "h"; }
  if (char == "I") { return "i"; }
  if (char == "J") { return "j"; }
  if (char == "K") { return "k"; }
  if (char == "L") { return "l"; }
  if (char == "M") { return "m"; }
  if (char == "N") { return "n"; }
  if (char == "O") { return "o"; }
  if (char == "P") { return "p"; }
  if (char == "Q") { return "q"; }
  if (char == "R") { return "r"; }
  if (char == "S") { return "s"; }
  if (char == "T") { return "t"; }
  if (char == "U") { return "u"; }
  if (char == "V") { return "v"; }
  if (char == "W") { return "w"; }
  if (char == "X") { return "x"; }
  if (char == "Y") { return "y"; }
  if (char == "Z") { return "z"; }
  return char;
}

// Converts a string to uppercase.
export function toUpper(str: string): string {
  let result: string = "";
  let i: number = 0;
  while (i < str.length) {
    let char: string = str.substring(i, i + 1);
    result = result + charToUpper(char);
    i = i + 1;
  }
  return result;
}

// Converts a character to uppercase.
function charToUpper(char: string): string {
  if (char == "a") { return "A"; }
  if (char == "b") { return "B"; }
  if (char == "c") { return "C"; }
  if (char == "d") { return "D"; }
  if (char == "e") { return "E"; }
  if (char == "f") { return "F"; }
  if (char == "g") { return "G"; }
  if (char == "h") { return "H"; }
  if (char == "i") { return "I"; }
  if (char == "j") { return "J"; }
  if (char == "k") { return "K"; }
  if (char == "l") { return "L"; }
  if (char == "m") { return "M"; }
  if (char == "n") { return "N"; }
  if (char == "o") { return "O"; }
  if (char == "p") { return "P"; }
  if (char == "q") { return "Q"; }
  if (char == "r") { return "R"; }
  if (char == "s") { return "S"; }
  if (char == "t") { return "T"; }
  if (char == "u") { return "U"; }
  if (char == "v") { return "V"; }
  if (char == "w") { return "W"; }
  if (char == "x") { return "X"; }
  if (char == "y") { return "Y"; }
  if (char == "z") { return "Z"; }
  return char;
}

// Converts a number to a hexadecimal string (lowercase, no "0x" prefix).
export function toHex(value: number): string {
  if (value == 0) { return "0"; }
  if (value < 0) { return "-" + toHex(-value); }

  let result: string = "";
  while (value > 0) {
    let digit: number = value - ((value / 16) * 16);
    result = hexDigitToChar(digit) + result;
    value = value / 16;
  }
  return result;
}

// Helper: converts a hex digit (0-15) to its character ('0'-'9', 'a'-'f').
function hexDigitToChar(digit: number): string {
  if (digit == 0) { return "0"; }
  if (digit == 1) { return "1"; }
  if (digit == 2) { return "2"; }
  if (digit == 3) { return "3"; }
  if (digit == 4) { return "4"; }
  if (digit == 5) { return "5"; }
  if (digit == 6) { return "6"; }
  if (digit == 7) { return "7"; }
  if (digit == 8) { return "8"; }
  if (digit == 9) { return "9"; }
  if (digit == 10) { return "a"; }
  if (digit == 11) { return "b"; }
  if (digit == 12) { return "c"; }
  if (digit == 13) { return "d"; }
  if (digit == 14) { return "e"; }
  if (digit == 15) { return "f"; }
  return "?";
}

// Converts a hexadecimal string to a number.
// Supports: "ff", "FF", "0x1a", "1A", "-ff", "0"
// Returns 0 if the string cannot be parsed as a valid hex number.
export function parseHex(str: string): number {
  if (str == "") { return 0; }

  let idx: number = 0;
  let isNegative: boolean = false;

  // Check for leading sign
  if (str.substring(idx, idx + 1) == "-") {
    isNegative = true;
    idx = idx + 1;
  }

  // Check for "0x" or "0X" prefix
  if (idx + 1 < str.length) {
    let prefix: string = str.substring(idx, idx + 2);
    if (prefix == "0x" || prefix == "0X") {
      idx = idx + 2;
    }
  }

  // Parse hex digits
  let result: number = 0;
  while (idx < str.length) {
    let char: string = str.substring(idx, idx + 1);
    let digit: number = hexCharToDigit(char);
    if (digit < 0) { break; }
    result = result * 16 + digit;
    idx = idx + 1;
  }

  if (isNegative) { result = -result; }
  return result;
}

// Helper: converts a hex character ('0'-'9', 'a'-'f', 'A'-'F') to its digit value, or -1 if not a hex digit.
function hexCharToDigit(char: string): number {
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
  if (char == "a" || char == "A") { return 10; }
  if (char == "b" || char == "B") { return 11; }
  if (char == "c" || char == "C") { return 12; }
  if (char == "d" || char == "D") { return 13; }
  if (char == "e" || char == "E") { return 14; }
  if (char == "f" || char == "F") { return 15; }
  return -1;
}

// Converts a number to binary string representation (no "0b" prefix).
export function toBinary(value: number): string {
  if (value == 0) { return "0"; }
  if (value < 0) { return "-" + toBinary(-value); }

  let result: string = "";
  while (value > 0) {
    let bit: number = value - ((value / 2) * 2);
    if (bit == 0) {
      result = "0" + result;
    } else {
      result = "1" + result;
    }
    value = value / 2;
  }
  return result;
}

// Parses a binary string to a number.
// Supports: "1010", "0b1010", "0B1010", "-1010"
// Returns 0 if not a valid binary number.
export function parseBinary(str: string): number {
  if (str == "") { return 0; }

  let idx: number = 0;
  let isNegative: boolean = false;

  // Check for leading sign
  if (str.substring(idx, idx + 1) == "-") {
    isNegative = true;
    idx = idx + 1;
  }

  // Check for "0b" or "0B" prefix
  if (idx + 1 < str.length) {
    let prefix: string = str.substring(idx, idx + 2);
    if (prefix == "0b" || prefix == "0B") {
      idx = idx + 2;
    }
  }

  // Parse binary digits
  let result: number = 0;
  while (idx < str.length) {
    let char: string = str.substring(idx, idx + 1);
    if (char == "0") {
      result = result * 2;
    } else if (char == "1") {
      result = result * 2 + 1;
    } else {
      break;
    }
    idx = idx + 1;
  }

  if (isNegative) { result = -result; }
  return result;
}
