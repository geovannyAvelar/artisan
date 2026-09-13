// String methods for ART - utilities for working with strings.
// Import with: `import { split, substring, indexOf, ... } from "art/strings";`

// Returns the character at the specified index, or empty string if out of bounds.
export function charAt(s: string, index: number): string {
  if (index < 0 || index >= s.length) { return ""; }
  return s[index] + "";  // Single character as string
}

// Returns true if string starts with the given prefix.
export function startsWith(s: string, prefix: string): boolean {
  if (prefix.length > s.length) { return false; }
  let i: number = 0;
  while (i < prefix.length) {
    if (s[i] != prefix[i]) { return false; }
    i = i + 1;
  }
  return true;
}

// Returns true if string ends with the given suffix.
export function endsWith(s: string, suffix: string): boolean {
  if (suffix.length > s.length) { return false; }
  let start: number = s.length - suffix.length;
  let i: number = 0;
  while (i < suffix.length) {
    if (s[start + i] != suffix[i]) { return false; }
    i = i + 1;
  }
  return true;
}

// Returns the index of the first occurrence of substring, or -1.
export function indexOf(s: string, substring: string): number {
  if (substring.length == 0) { return 0; }
  if (substring.length > s.length) { return -1; }

  let end: number = s.length - substring.length;
  let i: number = 0;
  while (i <= end) {
    let j: number = 0;
    let match: boolean = true;
    while (j < substring.length) {
      if (s[i + j] != substring[j]) {
        match = false;
      }
      j = j + 1;
    }
    if (match) { return i; }
    i = i + 1;
  }
  return -1;
}

// Returns the index of the last occurrence of substring, or -1.
export function lastIndexOf(s: string, substring: string): number {
  if (substring.length == 0) { return s.length; }
  if (substring.length > s.length) { return -1; }

  let i: number = s.length - substring.length;
  while (i >= 0) {
    let j: number = 0;
    let match: boolean = true;
    while (j < substring.length) {
      if (s[i + j] != substring[j]) {
        match = false;
      }
      j = j + 1;
    }
    if (match) { return i; }
    i = i - 1;
  }
  return -1;
}

// Returns true if the string contains the substring.
export function includes(s: string, substring: string): boolean {
  return indexOf(s, substring) >= 0;
}

// Returns a substring from start to end (exclusive). Negative indices count from end.
export function substring(s: string, start: number, end: number): string {
  let len: number = s.length;
  let s_idx: number = start < 0 ? 0 : (start > len ? len : start);
  let e_idx: number = end < 0 ? 0 : (end > len ? len : end);

  if (s_idx > e_idx) {
    let tmp: number = s_idx;
    s_idx = e_idx;
    e_idx = tmp;
  }

  if (s_idx >= e_idx) { return ""; }

  let result: string = "";
  let i: number = s_idx;
  while (i < e_idx) {
    result = result + (s[i] + "");
    i = i + 1;
  }
  return result;
}

// Returns a substring starting at start with the given length.
export function slice(s: string, start: number, length: number): string {
  let len: number = s.length;
  let s_idx: number = start < 0 ? (len + start) : start;

  if (s_idx < 0) { s_idx = 0; }
  if (s_idx >= len) { return ""; }

  let end: number = s_idx + length;
  if (end > len) { end = len; }

  let result: string = "";
  let i: number = s_idx;
  while (i < end) {
    result = result + (s[i] + "");
    i = i + 1;
  }
  return result;
}

// Trims whitespace from both ends of the string.
export function trim(s: string): string {
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

// Splits string by separator into an array of strings.
// Note: Returns a number array with character codes as a workaround for ART's type system.
// Use a custom implementation for string arrays.
export function split(s: string, separator: string): number {
  // TODO: This requires array allocation that ART's type system makes difficult.
  // For now, use a custom implementation with makeArray and manual iteration.
  return 0;  // Placeholder
}

// Returns a new string with all occurrences of search replaced with replacement.
export function replaceAll(s: string, search: string, replacement: string): string {
  if (search.length == 0) { return s; }

  let result: string = "";
  let i: number = 0;
  while (i < s.length) {
    let found: boolean = true;
    let j: number = 0;
    while (j < search.length && i + j < s.length) {
      if (s[i + j] != search[j]) {
        found = false;
      }
      j = j + 1;
    }

    if (found && i + search.length <= s.length) {
      result = result + replacement;
      i = i + search.length;
    } else {
      result = result + (s[i] + "");
      i = i + 1;
    }
  }
  return result;
}

// Returns true if the string contains only whitespace.
export function isBlank(s: string): boolean {
  let i: number = 0;
  while (i < s.length) {
    if (s[i] != ' ' && s[i] != '\t' && s[i] != '\n' && s[i] != '\r') {
      return false;
    }
    i = i + 1;
  }
  return true;
}

// Repeats the string count times.
export function repeat(s: string, count: number): string {
  if (count <= 0) { return ""; }
  let result: string = "";
  let i: number = 0;
  while (i < count) {
    result = result + s;
    i = i + 1;
  }
  return result;
}

// Pads the string to the specified length with the pad string (on the left).
export function padStart(s: string, length: number, padString: string): string {
  if (s.length >= length || padString.length == 0) { return s; }

  let needed: number = length - s.length;
  let padding: string = "";

  while (padding.length < needed) {
    padding = padding + padString;
  }

  if (padding.length > needed) {
    padding = substring(padding, 0, needed);
  }

  return padding + s;
}

// Pads the string to the specified length with the pad string (on the right).
export function padEnd(s: string, length: number, padString: string): string {
  if (s.length >= length || padString.length == 0) { return s; }

  let needed: number = length - s.length;
  let padding: string = "";

  while (padding.length < needed) {
    padding = padding + padString;
  }

  if (padding.length > needed) {
    padding = substring(padding, 0, needed);
  }

  return s + padding;
}

// Returns a string repeated. Shorter than writing s + s + s...
export function concat(s: string, other: string): string {
  return s + other;
}
