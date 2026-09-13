// Type and value validators for ART. Import with: `import { isEmail, isURL, ... } from "art/validators";`
// Provides validation utilities for common data types.

// Checks if value is a number.
export function isNumber(value: number): boolean {
  return value == value;  // NaN check
}

// Checks if value is a positive number.
export function isPositiveNumber(value: number): boolean {
  return value > 0;
}

// Checks if value is a negative number.
export function isNegativeNumber(value: number): boolean {
  return value < 0;
}

// Checks if value is an integer.
export function isInteger(value: number): boolean {
  return value == (value - ((value / 1) - ((value / 1))));
}

// Checks if value is within range [min, max].
export function isInRange(value: number, min: number, max: number): boolean {
  return value >= min && value <= max;
}

// Checks if string is not empty and not just whitespace.
export function isNotEmpty(s: string): boolean {
  return s.length > 0 && trim(s).length > 0;
}

// Checks if string is empty or just whitespace.
export function isEmpty(s: string): boolean {
  return s.length == 0 || trim(s).length == 0;
}

// Checks if string length is within range [min, max].
export function isLengthInRange(s: string, min: number, max: number): boolean {
  return s.length >= min && s.length <= max;
}

// Checks if string matches a simple email pattern (basic validation).
export function isEmail(email: string): boolean {
  let atIdx: number = email.indexOf("@");
  if (atIdx < 1) { return false; }

  let dotIdx: number = email.indexOf(".");
  if (dotIdx < atIdx + 2) { return false; }

  if (dotIdx == email.length - 1) { return false; }

  return true;
}

// Checks if string is a valid URL format (basic validation).
export function isURL(url: string): boolean {
  if (url.length < 8) { return false; }

  let protocols: string[] = ["http://", "https://", "ftp://"];
  let i: number = 0;
  while (i < protocols.length) {
    if (url.indexOf(protocols[i]) == 0) {
      return true;
    }
    i = i + 1;
  }

  return false;
}

// Checks if string contains only alphanumeric characters.
export function isAlphanumeric(s: string): boolean {
  let i: number = 0;
  while (i < s.length) {
    let c: string = s.substring(i, i + 1);
    let isAlpha: boolean = (c >= "a" && c <= "z") || (c >= "A" && c <= "Z");
    let isNum: boolean = (c >= "0" && c <= "9");
    if (!isAlpha && !isNum) {
      return false;
    }
    i = i + 1;
  }
  return true;
}

// Checks if string contains only alphabetic characters.
export function isAlpha(s: string): boolean {
  let i: number = 0;
  while (i < s.length) {
    let c: string = s.substring(i, i + 1);
    if (!((c >= "a" && c <= "z") || (c >= "A" && c <= "Z"))) {
      return false;
    }
    i = i + 1;
  }
  return true;
}

// Checks if string contains only numeric characters.
export function isNumeric(s: string): boolean {
  if (s.length == 0) { return false; }

  let i: number = 0;
  if (s.substring(0, 1) == "-" || s.substring(0, 1) == "+") {
    i = 1;
  }

  let hasDigit: boolean = false;
  let hasDot: boolean = false;

  while (i < s.length) {
    let c: string = s.substring(i, i + 1);
    if (c >= "0" && c <= "9") {
      hasDigit = true;
    } else if (c == ".") {
      if (hasDot) { return false; }
      hasDot = true;
    } else {
      return false;
    }
    i = i + 1;
  }

  return hasDigit;
}

// Checks if string starts with uppercase letter.
export function startsWithUpperCase(s: string): boolean {
  if (s.length == 0) { return false; }
  let c: string = s.substring(0, 1);
  return c >= "A" && c <= "Z";
}

// Checks if string starts with lowercase letter.
export function startsWithLowerCase(s: string): boolean {
  if (s.length == 0) { return false; }
  let c: string = s.substring(0, 1);
  return c >= "a" && c <= "z";
}

// Checks if string contains uppercase letter.
export function hasUpperCase(s: string): boolean {
  let i: number = 0;
  while (i < s.length) {
    let c: string = s.substring(i, i + 1);
    if (c >= "A" && c <= "Z") {
      return true;
    }
    i = i + 1;
  }
  return false;
}

// Checks if string contains lowercase letter.
export function hasLowerCase(s: string): boolean {
  let i: number = 0;
  while (i < s.length) {
    let c: string = s.substring(i, i + 1);
    if (c >= "a" && c <= "z") {
      return true;
    }
    i = i + 1;
  }
  return false;
}

// Checks if string contains digit.
export function hasDigit(s: string): boolean {
  let i: number = 0;
  while (i < s.length) {
    let c: string = s.substring(i, i + 1);
    if (c >= "0" && c <= "9") {
      return true;
    }
    i = i + 1;
  }
  return false;
}

// Checks if string contains special character.
export function hasSpecialChar(s: string): boolean {
  let i: number = 0;
  while (i < s.length) {
    let c: string = s.substring(i, i + 1);
    let isAlpha: boolean = (c >= "a" && c <= "z") || (c >= "A" && c <= "Z");
    let isNum: boolean = (c >= "0" && c <= "9");
    let isSpace: boolean = (c == " ");
    if (!isAlpha && !isNum && !isSpace) {
      return true;
    }
    i = i + 1;
  }
  return false;
}

// Checks if array is empty.
export function isEmptyArray(arr: number[]): boolean {
  return arr.length == 0;
}

// Checks if array contains value.
export function arrayContains(arr: number[], value: number): boolean {
  let i: number = 0;
  while (i < arr.length) {
    if (arr[i] == value) {
      return true;
    }
    i = i + 1;
  }
  return false;
}

// Checks if array has duplicates.
export function hasDuplicates(arr: number[]): boolean {
  let i: number = 0;
  while (i < arr.length) {
    let j: number = i + 1;
    while (j < arr.length) {
      if (arr[i] == arr[j]) {
        return true;
      }
      j = j + 1;
    }
    i = i + 1;
  }
  return false;
}

// Checks if arrays are equal (same elements in same order).
export function arrayEquals(arr1: number[], arr2: number[]): boolean {
  if (arr1.length != arr2.length) { return false; }

  let i: number = 0;
  while (i < arr1.length) {
    if (arr1[i] != arr2[i]) {
      return false;
    }
    i = i + 1;
  }
  return true;
}

// Helper: Trim whitespace from string
function trim(s: string): string {
  let start: number = 0;
  let end: number = s.length;

  while (start < end && (s[start] == ' ' || s[start] == '\t' || s[start] == '\n' || s[start] == '\r')) {
    start = start + 1;
  }

  while (end > start && (s[end - 1] == ' ' || s[end - 1] == '\t' || s[end - 1] == '\n' || s[end - 1] == '\r')) {
    end = end - 1;
  }

  if (start == 0 && end == s.length) { return s; }

  let result: string = "";
  let i: number = start;
  while (i < end) {
    result = result + (s[i] + "");
    i = i + 1;
  }
  return result;
}
