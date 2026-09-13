// Encoding utilities for ART. Import with: `import { encodeBase64, decodeBase64, ... } from "art/encoding";`
// Provides Base64, URL, and simple text encoding/decoding.

// Base64 alphabet
let BASE64_CHARS: string = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// Encodes string to Base64.
export function encodeBase64(str: string): string {
  let result: string = "";
  let i: number = 0;

  while (i < str.length) {
    let a: number = charCode(str[i]);
    let b: number = (i + 1 < str.length) ? charCode(str[i + 1]) : 0;
    let c: number = (i + 2 < str.length) ? charCode(str[i + 2]) : 0;

    let bitmap: number = (a << 16) | (b << 8) | c;

    result = result + BASE64_CHARS.substring(((bitmap >> 18) & 63), ((bitmap >> 18) & 63) + 1);
    result = result + BASE64_CHARS.substring(((bitmap >> 12) & 63), ((bitmap >> 12) & 63) + 1);

    if (i + 1 < str.length) {
      result = result + BASE64_CHARS.substring(((bitmap >> 6) & 63), ((bitmap >> 6) & 63) + 1);
    } else {
      result = result + "=";
    }

    if (i + 2 < str.length) {
      result = result + BASE64_CHARS.substring(bitmap & 63, (bitmap & 63) + 1);
    } else {
      result = result + "=";
    }

    i = i + 3;
  }

  return result;
}

// Decodes Base64 string to string.
export function decodeBase64(encoded: string): string {
  let result: string = "";
  let i: number = 0;

  while (i < encoded.length) {
    let c1: string = encoded.substring(i, i + 1);
    let c2: string = (i + 1 < encoded.length) ? encoded.substring(i + 1, i + 2) : "";
    let c3: string = (i + 2 < encoded.length) ? encoded.substring(i + 2, i + 3) : "";
    let c4: string = (i + 3 < encoded.length) ? encoded.substring(i + 3, i + 4) : "";

    let b1: number = base64CharToNum(c1);
    let b2: number = (c2 != "") ? base64CharToNum(c2) : 0;
    let b3: number = (c3 != "" && c3 != "=") ? base64CharToNum(c3) : 0;
    let b4: number = (c4 != "" && c4 != "=") ? base64CharToNum(c4) : 0;

    let bitmap: number = (b1 << 18) | (b2 << 12) | (b3 << 6) | b4;

    let a: number = (bitmap >> 16) & 255;
    let b: number = (bitmap >> 8) & 255;
    let c: number = bitmap & 255;

    result = result + charFromCode(a);
    if (c3 != "=") { result = result + charFromCode(b); }
    if (c4 != "=") { result = result + charFromCode(c); }

    i = i + 4;
  }

  return result;
}

// Encodes string for URL (percent encoding).
export function encodeURL(str: string): string {
  let result: string = "";
  let i: number = 0;

  while (i < str.length) {
    let c: string = str.substring(i, i + 1);
    if ((c >= "A" && c <= "Z") || (c >= "a" && c <= "z") || (c >= "0" && c <= "9") ||
        c == "-" || c == "_" || c == "." || c == "~") {
      result = result + c;
    } else if (c == " ") {
      result = result + "+";
    } else {
      let code: number = charCode(c);
      result = result + "%" + toHex(code);
    }
    i = i + 1;
  }

  return result;
}

// Decodes URL-encoded string (percent decoding).
export function decodeURL(encoded: string): string {
  let result: string = "";
  let i: number = 0;

  while (i < encoded.length) {
    let c: string = encoded.substring(i, i + 1);
    if (c == "+") {
      result = result + " ";
    } else if (c == "%") {
      if (i + 2 < encoded.length) {
        let hex: string = encoded.substring(i + 1, i + 3);
        let code: number = hexToNum(hex);
        result = result + charFromCode(code);
        i = i + 2;
      }
    } else {
      result = result + c;
    }
    i = i + 1;
  }

  return result;
}

// Encodes string using simple hex encoding.
export function encodeHex(str: string): string {
  let result: string = "";
  let i: number = 0;

  while (i < str.length) {
    let code: number = charCode(str.substring(i, i + 1));
    result = result + toHex(code);
    i = i + 1;
  }

  return result;
}

// Decodes hex-encoded string.
export function decodeHex(hex: string): string {
  let result: string = "";
  let i: number = 0;

  while (i < hex.length) {
    if (i + 1 < hex.length) {
      let byte: string = hex.substring(i, i + 2);
      let code: number = hexToNum(byte);
      result = result + charFromCode(code);
      i = i + 2;
    } else {
      i = i + 1;
    }
  }

  return result;
}

// Checks if string is valid Base64.
export function isValidBase64(str: string): boolean {
  if (str.length % 4 != 0) { return false; }

  let i: number = 0;
  while (i < str.length) {
    let c: string = str.substring(i, i + 1);
    if (!((c >= "A" && c <= "Z") || (c >= "a" && c <= "z") || (c >= "0" && c <= "9") ||
          c == "+" || c == "/" || c == "=")) {
      return false;
    }
    i = i + 1;
  }

  return true;
}

// Checks if string is valid URL-encoded.
export function isValidURLEncoded(str: string): boolean {
  let i: number = 0;
  while (i < str.length) {
    let c: string = str.substring(i, i + 1);
    if (c == "%") {
      if (i + 2 >= str.length) { return false; }
      let hex: string = str.substring(i + 1, i + 3);
      if (!isHexValid(hex)) { return false; }
      i = i + 2;
    }
    i = i + 1;
  }
  return true;
}

// Helper: Get character code (simplified, ASCII only)
function charCode(c: string): number {
  if (c == " ") { return 32; }
  if (c >= "!" && c <= "~") {
    let first: number = 33;
    let idx: number = 0;
    let current: number = first;
    while (current <= 126) {
      if (charFromCode(current) == c) { return current; }
      current = current + 1;
    }
  }
  return 0;
}

// Helper: Get character from code (simplified, ASCII only)
function charFromCode(code: number): string {
  if (code >= 32 && code <= 126) {
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
    if (code >= 48 && code <= 57) { return "" + (code - 48) + ""; }
    if (code == 58) { return ":"; }
    if (code == 59) { return ";"; }
    if (code == 60) { return "<"; }
    if (code == 61) { return "="; }
    if (code == 62) { return ">"; }
    if (code == 63) { return "?"; }
    if (code == 64) { return "@"; }
    if (code >= 65 && code <= 90) { return String.fromCharCode(code); }
    if (code == 91) { return "["; }
    if (code == 92) { return "\\"; }
    if (code == 93) { return "]"; }
    if (code == 94) { return "^"; }
    if (code == 95) { return "_"; }
    if (code == 96) { return "`"; }
    if (code >= 97 && code <= 122) { return String.fromCharCode(code); }
    if (code == 123) { return "{"; }
    if (code == 124) { return "|"; }
    if (code == 125) { return "}"; }
    if (code == 126) { return "~"; }
  }
  return "";
}

// Helper: Convert code to hex string
function toHex(code: number): string {
  let hex: string = "";
  let high: number = (code / 16);
  let low: number = code - (high * 16);
  hex = hexDigit(high) + hexDigit(low);
  return hex;
}

// Helper: Convert hex digit to character
function hexDigit(n: number): string {
  if (n < 10) { return "" + n + ""; }
  if (n == 10) { return "A"; }
  if (n == 11) { return "B"; }
  if (n == 12) { return "C"; }
  if (n == 13) { return "D"; }
  if (n == 14) { return "E"; }
  if (n == 15) { return "F"; }
  return "0";
}

// Helper: Convert hex character to number
function hexCharToNum(c: string): number {
  if (c >= "0" && c <= "9") { return 48 + (c[0] - "0"[0]); }
  if (c == "A" || c == "a") { return 10; }
  if (c == "B" || c == "b") { return 11; }
  if (c == "C" || c == "c") { return 12; }
  if (c == "D" || c == "d") { return 13; }
  if (c == "E" || c == "e") { return 14; }
  if (c == "F" || c == "f") { return 15; }
  return -1;
}

// Helper: Convert 2-char hex string to number
function hexToNum(hex: string): number {
  let high: number = hexCharToNum(hex.substring(0, 1));
  let low: number = hexCharToNum(hex.substring(1, 2));
  if (high < 0 || low < 0) { return 0; }
  return high * 16 + low;
}

// Helper: Check if string is valid hex
function isHexValid(hex: string): boolean {
  if (hex.length != 2) { return false; }
  return hexCharToNum(hex.substring(0, 1)) >= 0 && hexCharToNum(hex.substring(1, 2)) >= 0;
}

// Helper: Convert Base64 character to number
function base64CharToNum(c: string): number {
  let idx: number = BASE64_CHARS.indexOf(c);
  if (idx >= 0) { return idx; }
  return 0;
}
