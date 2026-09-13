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
export function split(s: string, separator: string): string[] {
  if (separator.length == 0) {
    let result: string[] = [];
    let i: number = 0;
    while (i < s.length) {
      result = result + [(s[i] + "")];
      i = i + 1;
    }
    return result;
  }

  let result: string[] = [];
  let current: string = "";
  let i: number = 0;

  while (i < s.length) {
    let found: boolean = true;
    let j: number = 0;
    while (j < separator.length && i + j < s.length) {
      if (s[i + j] != separator[j]) {
        found = false;
      }
      j = j + 1;
    }

    if (found && i + separator.length <= s.length) {
      result = result + [current];
      current = "";
      i = i + separator.length;
    } else {
      current = current + (s[i] + "");
      i = i + 1;
    }
  }

  result = result + [current];
  return result;
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

// Converts string to lowercase (ASCII letters only).
export function toLowerCase(s: string): string {
  let result: string = "";
  let i: number = 0;
  while (i < s.length) {
    let c: string = s[i] + "";
    if (c >= "A" && c <= "Z") {
      let code: number = charCode(c);
      result = result + charFromCode(code + 32);
    } else {
      result = result + c;
    }
    i = i + 1;
  }
  return result;
}

// Converts string to uppercase (ASCII letters only).
export function toUpperCase(s: string): string {
  let result: string = "";
  let i: number = 0;
  while (i < s.length) {
    let c: string = s[i] + "";
    if (c >= "a" && c <= "z") {
      let code: number = charCode(c);
      result = result + charFromCode(code - 32);
    } else {
      result = result + c;
    }
    i = i + 1;
  }
  return result;
}

// Replaces first occurrence of search with replacement.
export function replace(s: string, search: string, replacement: string): string {
  let idx: number = indexOf(s, search);
  if (idx < 0) { return s; }

  let before: string = substring(s, 0, idx);
  let after: string = substring(s, idx + search.length, s.length);
  return before + replacement + after;
}

// Returns the index of first occurrence of pattern (simple string search).
export function search(s: string, pattern: string): number {
  return indexOf(s, pattern);
}

// Returns true if string matches pattern (simple substring match).
export function match(s: string, pattern: string): boolean {
  return indexOf(s, pattern) >= 0;
}

// Returns character code at index, or 0 if out of bounds.
function charCode(c: string): number {
  if (c == "\n") { return 10; }
  if (c == "\r") { return 13; }
  if (c == "\t") { return 9; }
  if (c == " ") { return 32; }
  if (c == "!") { return 33; }
  if (c == "\"") { return 34; }
  if (c == "#") { return 35; }
  if (c == "$") { return 36; }
  if (c == "%") { return 37; }
  if (c == "&") { return 38; }
  if (c == "'") { return 39; }
  if (c == "(") { return 40; }
  if (c == ")") { return 41; }
  if (c == "*") { return 42; }
  if (c == "+") { return 43; }
  if (c == ",") { return 44; }
  if (c == "-") { return 45; }
  if (c == ".") { return 46; }
  if (c == "/") { return 47; }
  if (c >= "0" && c <= "9") { return 48 + (c[0] - "0"[0]); }
  if (c == ":") { return 58; }
  if (c == ";") { return 59; }
  if (c == "<") { return 60; }
  if (c == "=") { return 61; }
  if (c == ">") { return 62; }
  if (c == "?") { return 63; }
  if (c == "@") { return 64; }
  if (c >= "A" && c <= "Z") { return 65 + (c[0] - "A"[0]); }
  if (c == "[") { return 91; }
  if (c == "\\") { return 92; }
  if (c == "]") { return 93; }
  if (c == "^") { return 94; }
  if (c == "_") { return 95; }
  if (c == "`") { return 96; }
  if (c >= "a" && c <= "z") { return 97 + (c[0] - "a"[0]); }
  if (c == "{") { return 123; }
  if (c == "|") { return 124; }
  if (c == "}") { return 125; }
  if (c == "~") { return 126; }
  return 0;
}

// Returns character from code, or empty string if invalid.
// For uppercase letters (65-90) and lowercase (97-122)
function charFromCode(code: number): string {
  if (code == 10) { return "\n"; }
  if (code == 13) { return "\r"; }
  if (code == 9) { return "\t"; }
  if (code == 32) { return " "; }
  if (code == 33) { return "!"; }
  if (code == 34) { return "\""; }
  if (code == 35) { return "#"; }
  if (code == 36) { return "$"; }
  if (code == 37) { return "%"; }
  if (code == 38) { return "&"; }
  if (code == 39) { return "'"; }
  if (code == 40) { return "("; }
  if (code == 41) { return ")"; }
  if (code == 42) { return "*"; }
  if (code == 43) { return "+"; }
  if (code == 44) { return ","; }
  if (code == 45) { return "-"; }
  if (code == 46) { return "."; }
  if (code == 47) { return "/"; }
  if (code == 48) { return "0"; }
  if (code == 49) { return "1"; }
  if (code == 50) { return "2"; }
  if (code == 51) { return "3"; }
  if (code == 52) { return "4"; }
  if (code == 53) { return "5"; }
  if (code == 54) { return "6"; }
  if (code == 55) { return "7"; }
  if (code == 56) { return "8"; }
  if (code == 57) { return "9"; }
  if (code == 58) { return ":"; }
  if (code == 59) { return ";"; }
  if (code == 60) { return "<"; }
  if (code == 61) { return "="; }
  if (code == 62) { return ">"; }
  if (code == 63) { return "?"; }
  if (code == 64) { return "@"; }
  if (code == 65) { return "A"; }
  if (code == 66) { return "B"; }
  if (code == 67) { return "C"; }
  if (code == 68) { return "D"; }
  if (code == 69) { return "E"; }
  if (code == 70) { return "F"; }
  if (code == 71) { return "G"; }
  if (code == 72) { return "H"; }
  if (code == 73) { return "I"; }
  if (code == 74) { return "J"; }
  if (code == 75) { return "K"; }
  if (code == 76) { return "L"; }
  if (code == 77) { return "M"; }
  if (code == 78) { return "N"; }
  if (code == 79) { return "O"; }
  if (code == 80) { return "P"; }
  if (code == 81) { return "Q"; }
  if (code == 82) { return "R"; }
  if (code == 83) { return "S"; }
  if (code == 84) { return "T"; }
  if (code == 85) { return "U"; }
  if (code == 86) { return "V"; }
  if (code == 87) { return "W"; }
  if (code == 88) { return "X"; }
  if (code == 89) { return "Y"; }
  if (code == 90) { return "Z"; }
  if (code == 91) { return "["; }
  if (code == 92) { return "\\"; }
  if (code == 93) { return "]"; }
  if (code == 94) { return "^"; }
  if (code == 95) { return "_"; }
  if (code == 96) { return "`"; }
  if (code == 97) { return "a"; }
  if (code == 98) { return "b"; }
  if (code == 99) { return "c"; }
  if (code == 100) { return "d"; }
  if (code == 101) { return "e"; }
  if (code == 102) { return "f"; }
  if (code == 103) { return "g"; }
  if (code == 104) { return "h"; }
  if (code == 105) { return "i"; }
  if (code == 106) { return "j"; }
  if (code == 107) { return "k"; }
  if (code == 108) { return "l"; }
  if (code == 109) { return "m"; }
  if (code == 110) { return "n"; }
  if (code == 111) { return "o"; }
  if (code == 112) { return "p"; }
  if (code == 113) { return "q"; }
  if (code == 114) { return "r"; }
  if (code == 115) { return "s"; }
  if (code == 116) { return "t"; }
  if (code == 117) { return "u"; }
  if (code == 118) { return "v"; }
  if (code == 119) { return "w"; }
  if (code == 120) { return "x"; }
  if (code == 121) { return "y"; }
  if (code == 122) { return "z"; }
  if (code == 123) { return "{"; }
  if (code == 124) { return "|"; }
  if (code == 125) { return "}"; }
  if (code == 126) { return "~"; }
  return "";
}
