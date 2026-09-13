// RegExp module for ART. Import with: `import { RegExp, test, match, replace, ... } from "art/regex";`
// Provides regular expression support with pattern matching and string operations.

// RegExp type: represents a compiled regular expression pattern
export type RegExp = number;

// Regex flags
export const GLOBAL: number = 1;
export const IGNORE_CASE: number = 2;
export const MULTILINE: number = 4;

// Internal regex storage
type RegexPattern = [pattern: string, flags: number];
let _regexPatterns: RegexPattern[] = [];
let _regexCounter: number = 0;

// Compiles a regex pattern from a string.
export function compile(pattern: string, flags: number): RegExp {
  let regexId: RegExp = _regexCounter;
  _regexCounter = _regexCounter + 1;
  _regexPatterns = _regexPatterns + [[pattern, flags]];
  return regexId;
}

// Compiles a regex pattern without flags.
export function compileSimple(pattern: string): RegExp {
  return compile(pattern, 0);
}

// Tests if a pattern matches at the start of a string.
export function test(regex: RegExp, str: string): boolean {
  if (regex < 0 || regex >= _regexPatterns.length) { return false; }

  let pattern: string = _regexPatterns[regex][0];
  let flags: number = _regexPatterns[regex][1];

  if (pattern.length == 0) { return true; }
  if (str.length == 0) { return pattern == ""; }

  return matchPattern(pattern, str, flags);
}

// Tests if pattern matches anywhere in the string.
export function search(regex: RegExp, str: string): number {
  if (regex < 0 || regex >= _regexPatterns.length) { return -1; }

  let pattern: string = _regexPatterns[regex][0];
  let flags: number = _regexPatterns[regex][1];

  let i: number = 0;
  while (i <= str.length) {
    let substring: string = str.substring(i);
    if (matchPattern(pattern, substring, flags)) {
      return i;
    }
    i = i + 1;
  }

  return -1;
}

// Finds all matches in a string.
export function match(regex: RegExp, str: string): string[] {
  if (regex < 0 || regex >= _regexPatterns.length) { return []; }

  let pattern: string = _regexPatterns[regex][0];
  let flags: number = _regexPatterns[regex][1];
  let result: string[] = [];

  let i: number = 0;
  while (i < str.length) {
    let substring: string = str.substring(i);
    let matched: string = tryMatch(pattern, substring, flags);

    if (matched.length > 0) {
      result = result + [matched];
      i = i + matched.length;
    } else {
      i = i + 1;
    }
  }

  return result;
}

// Replaces first or all occurrences.
export function replace(regex: RegExp, str: string, replacement: string): string {
  if (regex < 0 || regex >= _regexPatterns.length) { return str; }

  let pattern: string = _regexPatterns[regex][0];
  let flags: number = _regexPatterns[regex][1];
  let isGlobal: boolean = (flags & GLOBAL) != 0;

  let result: string = "";
  let lastIndex: number = 0;

  let i: number = 0;
  while (i <= str.length) {
    let substring: string = str.substring(i);
    let matched: string = tryMatch(pattern, substring, flags);

    if (matched.length > 0) {
      result = result + str.substring(lastIndex, i);
      result = result + replacement;
      i = i + matched.length;
      lastIndex = i;

      if (!isGlobal) {
        result = result + str.substring(i);
        return result;
      }
    } else {
      i = i + 1;
    }
  }

  result = result + str.substring(lastIndex);
  return result;
}

// Splits a string by a pattern.
export function split(regex: RegExp, str: string): string[] {
  if (regex < 0 || regex >= _regexPatterns.length) { return [str]; }

  let pattern: string = _regexPatterns[regex][0];
  let flags: number = _regexPatterns[regex][1];
  let result: string[] = [];

  let lastIndex: number = 0;
  let i: number = 0;

  while (i < str.length) {
    let substring: string = str.substring(i);
    let matched: string = tryMatch(pattern, substring, flags);

    if (matched.length > 0) {
      if (i > lastIndex) {
        result = result + [str.substring(lastIndex, i)];
      }
      i = i + matched.length;
      lastIndex = i;
    } else {
      i = i + 1;
    }
  }

  if (lastIndex < str.length) {
    result = result + [str.substring(lastIndex)];
  }

  return result;
}

// Checks if a string starts with a pattern.
export function startsWith(regex: RegExp, str: string): boolean {
  if (regex < 0 || regex >= _regexPatterns.length) { return false; }

  let pattern: string = _regexPatterns[regex][0];
  let flags: number = _regexPatterns[regex][1];

  return matchPattern(pattern, str, flags);
}

// Checks if a string contains a pattern.
export function contains(regex: RegExp, str: string): boolean {
  return search(regex, str) >= 0;
}

// Gets the pattern string from a compiled regex.
export function getPattern(regex: RegExp): string {
  if (regex < 0 || regex >= _regexPatterns.length) { return ""; }
  return _regexPatterns[regex][0];
}

// Gets the flags from a compiled regex.
export function getFlags(regex: RegExp): number {
  if (regex < 0 || regex >= _regexPatterns.length) { return 0; }
  return _regexPatterns[regex][1];
}

// Predefined patterns for common use cases
export function literal(str: string): RegExp {
  return compile(escape(str), 0);
}

export function digit(): RegExp {
  return compile("[0-9]", 0);
}

export function notDigit(): RegExp {
  return compile("[^0-9]", 0);
}

export function whitespace(): RegExp {
  return compile("[ \\t\\n\\r]", 0);
}

export function notWhitespace(): RegExp {
  return compile("[^ \\t\\n\\r]", 0);
}

export function word(): RegExp {
  return compile("[a-zA-Z0-9_]", 0);
}

export function notWord(): RegExp {
  return compile("[^a-zA-Z0-9_]", 0);
}

export function anyChar(): RegExp {
  return compile(".", 0);
}

export function startOfString(): RegExp {
  return compile("^", 0);
}

export function endOfString(): RegExp {
  return compile("$", 0);
}

// Pattern: one or more digits
export function digits(): RegExp {
  return compile("[0-9]+", 0);
}

// Pattern: word characters
export function wordChars(): RegExp {
  return compile("[a-zA-Z0-9_]+", 0);
}

// Pattern: email-like pattern (simplified)
export function email(): RegExp {
  return compile("[a-zA-Z0-9._-]+@[a-zA-Z0-9.-]+", 0);
}

// Pattern: URL-like pattern (simplified)
export function url(): RegExp {
  return compile("https?://[a-zA-Z0-9._/-]+", 0);
}

// Pattern: hex color (simplified)
export function hexColor(): RegExp {
  return compile("#[0-9a-fA-F][0-9a-fA-F][0-9a-fA-F]", 0);
}

// Helper: Escapes special characters in a string for literal matching.
function escape(str: string): string {
  let result: string = "";
  let i: number = 0;

  while (i < str.length) {
    let c: string = str.substring(i, i + 1);

    if (c == "." || c == "*" || c == "+" || c == "?" || c == "[" || c == "]" || c == "(" || c == ")" || c == "{" || c == "}" || c == "^" || c == "$" || c == "|" || c == "\\") {
      result = result + "\\";
    }

    result = result + c;
    i = i + 1;
  }

  return result;
}

// Helper: Tries to match a pattern at the start of a string, returns matched portion.
function tryMatch(pattern: string, str: string, flags: number): string {
  if (matchPattern(pattern, str, flags)) {
    return extractMatch(pattern, str);
  }
  return "";
}

// Helper: Extracts the matched portion.
function extractMatch(pattern: string, str: string): string {
  let i: number = 0;
  let patIdx: number = 0;
  let result: string = "";

  while (patIdx < pattern.length && i < str.length) {
    let c: string = str.substring(i, i + 1);
    let p: string = pattern.substring(patIdx, patIdx + 1);

    if (p == "." || p == c) {
      result = result + c;
      i = i + 1;
      patIdx = patIdx + 1;
    } else if (p == "*") {
      if (patIdx > 0) {
        let prevP: string = pattern.substring(patIdx - 1, patIdx);
        while (i < str.length && (prevP == "." || prevP == str.substring(i, i + 1))) {
          result = result + str.substring(i, i + 1);
          i = i + 1;
        }
        patIdx = patIdx + 1;
      } else {
        i = i + 1;
      }
    } else if (p == "[") {
      let charClass: string = "";
      patIdx = patIdx + 1;
      while (patIdx < pattern.length && pattern.substring(patIdx, patIdx + 1) != "]") {
        charClass = charClass + pattern.substring(patIdx, patIdx + 1);
        patIdx = patIdx + 1;
      }
      patIdx = patIdx + 1;

      if (isInCharClass(c, charClass)) {
        result = result + c;
        i = i + 1;
      } else {
        return result;
      }
    } else if (p == "^" && i == 0) {
      patIdx = patIdx + 1;
    } else if (p == "$" && i == str.length) {
      patIdx = patIdx + 1;
    } else {
      return result;
    }
  }

  if (patIdx < pattern.length) {
    while (patIdx < pattern.length) {
      let p: string = pattern.substring(patIdx, patIdx + 1);
      if (p == "*" || p == "?" || p == "$" || p == "^") {
        patIdx = patIdx + 1;
      } else {
        return result;
      }
    }
  }

  return result;
}

// Helper: Checks if a character is in a character class.
function isInCharClass(c: string, charClass: string): boolean {
  let negated: boolean = false;
  let idx: number = 0;

  if (charClass.length > 0 && charClass.substring(0, 1) == "^") {
    negated = true;
    idx = 1;
  }

  let found: boolean = false;
  while (idx < charClass.length) {
    let ch: string = charClass.substring(idx, idx + 1);

    if (ch == "-" && idx > 0 && idx < charClass.length - 1) {
      let from: string = charClass.substring(idx - 1, idx);
      let to: string = charClass.substring(idx + 1, idx + 2);
      if (isInRange(c, from, to)) {
        found = true;
      }
      idx = idx + 2;
    } else if (ch == c) {
      found = true;
      idx = idx + 1;
    } else if (ch == "\\") {
      idx = idx + 1;
      if (idx < charClass.length) {
        let escaped: string = charClass.substring(idx, idx + 1);
        if (escaped == c) {
          found = true;
        }
        idx = idx + 1;
      }
    } else {
      idx = idx + 1;
    }
  }

  if (negated) {
    return !found;
  }
  return found;
}

// Helper: Checks if character is in a range [from-to].
function isInRange(c: string, from: string, to: string): boolean {
  return c >= from && c <= to;
}

// Helper: Checks if a pattern matches at the start of a string.
function matchPattern(pattern: string, str: string, flags: number): boolean {
  if (pattern.length == 0) { return true; }
  if (str.length == 0) { return pattern == ""; }

  let ignoreCase: boolean = (flags & IGNORE_CASE) != 0;
  let patStr: string = pattern;
  let testStr: string = str;

  if (ignoreCase) {
    patStr = patStr.toLowerCase();
    testStr = testStr.toLowerCase();
  }

  let i: number = 0;
  let patIdx: number = 0;

  while (patIdx < patStr.length && i < testStr.length) {
    let c: string = testStr.substring(i, i + 1);
    let p: string = patStr.substring(patIdx, patIdx + 1);

    if (p == "." || p == c) {
      i = i + 1;
      patIdx = patIdx + 1;
    } else if (p == "*") {
      if (patIdx > 0) {
        let prevP: string = patStr.substring(patIdx - 1, patIdx);
        while (i < testStr.length && (prevP == "." || prevP == testStr.substring(i, i + 1))) {
          i = i + 1;
        }
        patIdx = patIdx + 1;
      } else {
        return false;
      }
    } else if (p == "[") {
      let charClass: string = "";
      patIdx = patIdx + 1;
      while (patIdx < patStr.length && patStr.substring(patIdx, patIdx + 1) != "]") {
        charClass = charClass + patStr.substring(patIdx, patIdx + 1);
        patIdx = patIdx + 1;
      }
      patIdx = patIdx + 1;

      if (!isInCharClass(c, charClass)) {
        return false;
      }
      i = i + 1;
    } else if (p == "^" && i == 0) {
      patIdx = patIdx + 1;
    } else if (p == "$" && i == testStr.length) {
      patIdx = patIdx + 1;
    } else if (p == "?") {
      patIdx = patIdx + 1;
    } else {
      return false;
    }
  }

  if (patIdx < patStr.length) {
    while (patIdx < patStr.length) {
      let p: string = patStr.substring(patIdx, patIdx + 1);
      if (p == "*" || p == "?" || p == "$") {
        patIdx = patIdx + 1;
      } else {
        return false;
      }
    }
  }

  return patIdx >= patStr.length;
}

// Clears all regex patterns (for testing).
export function clearAllRegex(): void {
  _regexPatterns = [];
  _regexCounter = 0;
}
