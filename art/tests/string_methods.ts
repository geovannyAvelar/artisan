import {
  charAt, startsWith, endsWith, indexOf, lastIndexOf, includes,
  substring, slice, trim, replaceAll, isBlank, repeat, padStart, padEnd, concat
} from "art/strings";

function testCharAt(): number {
  let s: string = "hello";
  if (charAt(s, 0) != "h") { return 1; }
  if (charAt(s, 4) != "o") { return 2; }
  if (charAt(s, 10) != "") { return 3; }
  if (charAt(s, -1) != "") { return 4; }
  return 0;
}

function testStartsWith(): number {
  let s: string = "hello world";
  if (!startsWith(s, "hello")) { return 1; }
  if (startsWith(s, "world")) { return 2; }
  if (!startsWith(s, "")) { return 3; }
  if (startsWith(s, "hello world extra")) { return 4; }
  return 0;
}

function testEndsWith(): number {
  let s: string = "hello world";
  if (!endsWith(s, "world")) { return 1; }
  if (endsWith(s, "hello")) { return 2; }
  if (!endsWith(s, "")) { return 3; }
  if (endsWith(s, "extra hello world")) { return 4; }
  return 0;
}

function testIndexOf(): number {
  let s: string = "hello hello";
  if (indexOf(s, "hello") != 0) { return 1; }
  if (indexOf(s, "lo") != 3) { return 2; }
  if (indexOf(s, "xyz") != -1) { return 3; }
  if (indexOf(s, "") != 0) { return 4; }
  return 0;
}

function testLastIndexOf(): number {
  let s: string = "hello hello";
  if (lastIndexOf(s, "hello") != 6) { return 1; }
  if (lastIndexOf(s, "lo") != 9) { return 2; }
  if (lastIndexOf(s, "xyz") != -1) { return 3; }
  if (lastIndexOf(s, "") != 11) { return 4; }
  return 0;
}

function testIncludes(): number {
  let s: string = "hello world";
  if (!includes(s, "hello")) { return 1; }
  if (!includes(s, "world")) { return 2; }
  if (!includes(s, "o w")) { return 3; }
  if (includes(s, "xyz")) { return 4; }
  return 0;
}

function testSubstring(): number {
  let s: string = "hello";
  if (substring(s, 0, 5) != "hello") { return 1; }
  if (substring(s, 1, 4) != "ell") { return 2; }
  if (substring(s, 0, 0) != "") { return 3; }
  if (substring(s, 4, 1) != "ell") { return 4; }  // swaps if backwards
  return 0;
}

function testSlice(): number {
  let s: string = "hello";
  if (slice(s, 0, 5) != "hello") { return 1; }
  if (slice(s, 1, 3) != "ell") { return 2; }
  if (slice(s, 0, 0) != "") { return 3; }
  return 0;
}

function testTrim(): number {
  if (trim("  hello  ") != "hello") { return 1; }
  if (trim("\nhello\t") != "hello") { return 2; }
  if (trim("hello") != "hello") { return 3; }
  if (trim("   ") != "") { return 4; }
  return 0;
}

function testReplaceAll(): number {
  if (replaceAll("hello hello", "hello", "hi") != "hi hi") { return 1; }
  if (replaceAll("aaa", "aa", "b") != "ba") { return 2; }
  if (replaceAll("test", "x", "y") != "test") { return 3; }
  if (replaceAll("test", "", "x") != "test") { return 4; }
  return 0;
}

function testIsBlank(): number {
  if (!isBlank("   ")) { return 1; }
  if (!isBlank("\t\n")) { return 2; }
  if (isBlank("a")) { return 3; }
  if (!isBlank("")) { return 4; }
  return 0;
}

function testRepeat(): number {
  if (repeat("ab", 3) != "ababab") { return 1; }
  if (repeat("x", 1) != "x") { return 2; }
  if (repeat("y", 0) != "") { return 3; }
  return 0;
}

function testPadStart(): number {
  if (padStart("5", 3, "0") != "005") { return 1; }
  if (padStart("hello", 3, "x") != "hello") { return 2; }
  return 0;
}

function testPadEnd(): number {
  if (padEnd("5", 3, "0") != "500") { return 1; }
  if (padEnd("hello", 3, "x") != "hello") { return 2; }
  return 0;
}

function testConcat(): number {
  if (concat("hello", " world") != "hello world") { return 1; }
  if (concat("", "test") != "test") { return 2; }
  return 0;
}

function main(): number {
  if (testCharAt() != 0) { return 1; }
  if (testStartsWith() != 0) { return 2; }
  if (testEndsWith() != 0) { return 3; }
  if (testIndexOf() != 0) { return 4; }
  if (testLastIndexOf() != 0) { return 5; }
  if (testIncludes() != 0) { return 6; }
  if (testSubstring() != 0) { return 7; }
  if (testSlice() != 0) { return 8; }
  if (testTrim() != 0) { return 9; }
  if (testReplaceAll() != 0) { return 10; }
  if (testIsBlank() != 0) { return 11; }
  if (testRepeat() != 0) { return 12; }
  if (testPadStart() != 0) { return 13; }
  if (testPadEnd() != 0) { return 14; }
  if (testConcat() != 0) { return 15; }
  return 0;
}
