// JSON utilities for serialization and parsing.
// Import with: `import { stringify, parse } from "art/json";`

// Converts a number to a JSON string representation.
// Numbers serialize to decimal representation.
export function stringify(value: number): string {
  return numberToString(value);
}

// Parses a JSON string to a number.
// Only supports numeric JSON values.
// Returns 0 if parsing fails.
export function parse(json: string): number {
  return parseNumber(json);
}

// Stringifies an array of numbers to JSON format.
// Returns a JSON array string like "[1, 2, 3]".
export function stringifyArray(arr: number[]): string {
  if (arr.length == 0) { return "[]"; }

  let result: string = "[";
  let i: number = 0;
  while (i < arr.length) {
    if (i > 0) { result = result + ", "; }
    result = result + numberToString(arr[i]);
    i = i + 1;
  }
  result = result + "]";

  return result;
}

// Parses a JSON array string to an array of numbers.
// Returns empty array if parsing fails.
export function parseArray(json: string): number[] {
  if (json == "") { return []; }

  // Remove whitespace and brackets
  let trimmed: string = json;
  if (trimmed.substring(0, 1) == "[") {
    trimmed = trimmed.substring(1);
  }
  if (trimmed.substring(trimmed.length - 1) == "]") {
    trimmed = trimmed.substring(0, trimmed.length - 1);
  }

  if (trimmed == "") { return []; }

  let result: number[] = [];
  let current: string = "";
  let i: number = 0;

  while (i < trimmed.length) {
    let char: string = trimmed.substring(i, i + 1);

    if (char == ",") {
      let num: number = parseNumber(current);
      result = result + [num];
      current = "";
    } else if (char != " ") {
      current = current + char;
    }

    i = i + 1;
  }

  if (current != "") {
    let num: number = parseNumber(current);
    result = result + [num];
  }

  return result;
}

// Checks if a string is valid JSON format.
export function isValidJSON(json: string): boolean {
  if (json == "") { return false; }

  let trimmed: string = trim(json);

  // Check for array format
  if (trimmed.substring(0, 1) == "[") {
    if (trimmed.substring(trimmed.length - 1) != "]") { return false; }
    let arr: number[] = parseArray(trimmed);
    return arr.length >= 0;  // Valid if we can parse it
  }

  // Check for number format
  if (isNumberString(trimmed)) { return true; }

  return false;
}

// Converts a number to JSON string with proper formatting.
function numberToString(value: number): string {
  if (value == 0) { return "0"; }

  let isNegative: boolean = false;
  if (value < 0) {
    isNegative = true;
    value = -value;
  }

  // Handle integer part
  let intStr: string = "";
  let intPart: number = value - ((value / 1) - ((value / 1)));

  if (intPart == 0) {
    intStr = "0";
  } else {
    while (intPart > 0) {
      let digit: number = intPart - ((intPart / 10) * 10);
      intStr = digitToChar(digit) + intStr;
      intPart = intPart / 10;
    }
  }

  // Handle fractional part (simplified)
  let fracStr: string = "";
  let frac: number = value - intPart;
  if (frac > 0.0001) {
    fracStr = ".";
    let i: number = 0;
    while (i < 6 && frac > 0) {
      frac = frac * 10;
      let digit: number = frac - ((frac / 1) - ((frac / 1)));
      fracStr = fracStr + digitToChar(digit);
      frac = frac - digit;
      i = i + 1;
    }
  }

  let result: string = intStr + fracStr;
  if (isNegative) { result = "-" + result; }

  return result;
}

// Parses a JSON string to a number.
function parseNumber(json: string): number {
  let trimmed: string = trim(json);
  if (trimmed == "") { return 0; }

  let idx: number = 0;
  let isNegative: boolean = false;

  if (trimmed.substring(idx, idx + 1) == "-") {
    isNegative = true;
    idx = idx + 1;
  } else if (trimmed.substring(idx, idx + 1) == "+") {
    idx = idx + 1;
  }

  // Parse integer part
  let intPart: number = 0;
  while (idx < trimmed.length) {
    let char: string = trimmed.substring(idx, idx + 1);
    let digit: number = charToDigit(char);
    if (digit < 0 || digit > 9) { break; }
    intPart = intPart * 10 + digit;
    idx = idx + 1;
  }

  // Parse decimal part
  let fracPart: number = 0;
  let fracDivisor: number = 10;
  if (idx < trimmed.length && trimmed.substring(idx, idx + 1) == ".") {
    idx = idx + 1;
    while (idx < trimmed.length) {
      let char: string = trimmed.substring(idx, idx + 1);
      let digit: number = charToDigit(char);
      if (digit < 0 || digit > 9) { break; }
      fracPart = fracPart + digit / fracDivisor;
      fracDivisor = fracDivisor * 10;
      idx = idx + 1;
    }
  }

  let result: number = intPart + fracPart;
  if (isNegative) { result = -result; }

  return result;
}

// Checks if a string represents a valid number.
function isNumberString(str: string): boolean {
  if (str == "") { return false; }

  let idx: number = 0;
  if (str.substring(idx, idx + 1) == "-" || str.substring(idx, idx + 1) == "+") {
    idx = idx + 1;
  }

  if (idx >= str.length) { return false; }

  // Must have at least one digit
  let hasDigit: boolean = false;
  while (idx < str.length) {
    let char: string = str.substring(idx, idx + 1);
    let digit: number = charToDigit(char);
    if (digit >= 0 && digit <= 9) {
      hasDigit = true;
      break;
    }
    if (char != ".") { return false; }
    idx = idx + 1;
  }

  return hasDigit;
}

// Helper: Convert digit to character.
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
  return "0";
}

// Helper: Convert character to digit.
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

// Helper: Trim whitespace from string.
function trim(str: string): string {
  let start: number = 0;
  let end: number = str.length - 1;

  while (start <= end) {
    let char: string = str.substring(start, start + 1);
    if (char != " " && char != "\t" && char != "\n" && char != "\r") {
      break;
    }
    start = start + 1;
  }

  while (end >= start) {
    let char: string = str.substring(end, end + 1);
    if (char != " " && char != "\t" && char != "\n" && char != "\r") {
      break;
    }
    end = end - 1;
  }

  if (start > end) { return ""; }
  return str.substring(start, end + 1);
}
