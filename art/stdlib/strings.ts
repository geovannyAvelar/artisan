// String utilities module for ART. Import with: `import { trim, padStart, padEnd, ... } from "art/strings";`
// Provides enhanced string manipulation methods.

export function trim(str: string): string {
  return trimStart(trimEnd(str));
}

export function trimStart(str: string): string {
  let i: number = 0;
  while (i < str.length) {
    let c: string = str.substring(i, i + 1);
    if (c != " " && c != "\t" && c != "\n" && c != "\r") {
      break;
    }
    i = i + 1;
  }
  return str.substring(i, str.length);
}

export function trimEnd(str: string): string {
  let i: number = str.length - 1;
  while (i >= 0) {
    let c: string = str.substring(i, i + 1);
    if (c != " " && c != "\t" && c != "\n" && c != "\r") {
      break;
    }
    i = i - 1;
  }
  return str.substring(0, i + 1);
}

export function padStart(str: string, targetLength: number, padString: string): string {
  if (str.length >= targetLength) { return str; }
  
  let padCount: number = targetLength - str.length;
  let padding: string = "";
  
  while (padding.length < padCount) {
    if (padding.length + padString.length <= padCount) {
      padding = padding + padString;
    } else {
      padding = padding + padString.substring(0, padCount - padding.length);
    }
  }
  
  return padding + str;
}

export function padEnd(str: string, targetLength: number, padString: string): string {
  if (str.length >= targetLength) { return str; }
  
  let padCount: number = targetLength - str.length;
  let padding: string = "";
  
  while (padding.length < padCount) {
    if (padding.length + padString.length <= padCount) {
      padding = padding + padString;
    } else {
      padding = padding + padString.substring(0, padCount - padding.length);
    }
  }
  
  return str + padding;
}

export function repeat(str: string, count: number): string {
  let result: string = "";
  let i: number = 0;
  
  while (i < count) {
    result = result + str;
    i = i + 1;
  }
  
  return result;
}

export function startsWith(str: string, search: string): boolean {
  if (search.length > str.length) { return false; }
  
  let i: number = 0;
  while (i < search.length) {
    if (str.substring(i, i + 1) != search.substring(i, i + 1)) {
      return false;
    }
    i = i + 1;
  }
  
  return true;
}

export function endsWith(str: string, search: string): boolean {
  if (search.length > str.length) { return false; }
  
  let offset: number = str.length - search.length;
  let i: number = 0;
  
  while (i < search.length) {
    if (str.substring(offset + i, offset + i + 1) != search.substring(i, i + 1)) {
      return false;
    }
    i = i + 1;
  }
  
  return true;
}

export function includes(str: string, search: string): boolean {
  return indexOf(str, search) >= 0;
}

export function indexOf(str: string, search: string): number {
  let i: number = 0;
  while (i <= str.length - search.length) {
    let match: boolean = true;
    let j: number = 0;
    while (j < search.length) {
      if (str.substring(i + j, i + j + 1) != search.substring(j, j + 1)) {
        match = false;
      }
      j = j + 1;
    }
    if (match) { return i; }
    i = i + 1;
  }
  return -1;
}

export function lastIndexOf(str: string, search: string): number {
  let result: number = -1;
  let i: number = 0;
  
  while (i <= str.length - search.length) {
    let match: boolean = true;
    let j: number = 0;
    while (j < search.length) {
      if (str.substring(i + j, i + j + 1) != search.substring(j, j + 1)) {
        match = false;
      }
      j = j + 1;
    }
    if (match) { result = i; }
    i = i + 1;
  }
  
  return result;
}

export function replace(str: string, search: string, replacement: string): string {
  let idx: number = indexOf(str, search);
  if (idx < 0) { return str; }
  
  let before: string = str.substring(0, idx);
  let after: string = str.substring(idx + search.length, str.length);
  
  return before + replacement + after;
}

export function replaceAll(str: string, search: string, replacement: string): string {
  let result: string = "";
  let lastIdx: number = 0;
  let idx: number = indexOf(str, search);
  
  while (idx >= 0) {
    result = result + str.substring(lastIdx, idx);
    result = result + replacement;
    lastIdx = idx + search.length;
    idx = indexOf(str.substring(lastIdx, str.length), search);
    if (idx >= 0) {
      idx = idx + lastIdx;
    }
  }
  
  result = result + str.substring(lastIdx, str.length);
  return result;
}

export function split(str: string, separator: string): string[] {
  if (separator.length == 0) { return [str]; }
  
  let result: string[] = [];
  let current: string = "";
  let i: number = 0;
  
  while (i <= str.length - separator.length) {
    let match: boolean = true;
    let j: number = 0;
    while (j < separator.length) {
      if (str.substring(i + j, i + j + 1) != separator.substring(j, j + 1)) {
        match = false;
      }
      j = j + 1;
    }
    
    if (match) {
      result = result + [current];
      current = "";
      i = i + separator.length;
    } else {
      current = current + str.substring(i, i + 1);
      i = i + 1;
    }
  }
  
  while (i < str.length) {
    current = current + str.substring(i, i + 1);
    i = i + 1;
  }
  
  result = result + [current];
  return result;
}

export function toUpperCase(str: string): string {
  let result: string = "";
  let i: number = 0;
  
  while (i < str.length) {
    let c: string = str.substring(i, i + 1);
    if (c >= "a" && c <= "z") {
      let code: number = c.charCodeAt(0) - 32;
      result = result + String.fromCharCode(code);
    } else {
      result = result + c;
    }
    i = i + 1;
  }
  
  return result;
}

export function toLowerCase(str: string): string {
  let result: string = "";
  let i: number = 0;
  
  while (i < str.length) {
    let c: string = str.substring(i, i + 1);
    if (c >= "A" && c <= "Z") {
      let code: number = c.charCodeAt(0) + 32;
      result = result + String.fromCharCode(code);
    } else {
      result = result + c;
    }
    i = i + 1;
  }
  
  return result;
}

export function reverse(str: string): string {
  let result: string = "";
  let i: number = str.length - 1;
  
  while (i >= 0) {
    result = result + str.substring(i, i + 1);
    i = i - 1;
  }
  
  return result;
}

export function slice(str: string, start: number, end: number): string {
  if (start < 0) { start = str.length + start; }
  if (end < 0) { end = str.length + end; }
  
  if (start < 0) { start = 0; }
  if (end > str.length) { end = str.length; }
  if (start > end) { return ""; }
  
  return str.substring(start, end);
}

export function charAt(str: string, index: number): string {
  if (index < 0 || index >= str.length) { return ""; }
  return str.substring(index, index + 1);
}

export function charCodeAt(str: string, index: number): number {
  if (index < 0 || index >= str.length) { return -1; }
  return str.substring(index, index + 1).charCodeAt(0);
}
