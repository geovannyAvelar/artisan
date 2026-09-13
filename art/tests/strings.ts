import { trim, trimStart, trimEnd, padStart, padEnd, repeat, startsWith, endsWith, includes, indexOf, replace, split, toUpperCase, toLowerCase, reverse, slice, charAt } from "art/strings";

function testTrim(): number {
  let result: string = trim("  hello  ");
  if (result != "hello") { return 1; }
  return 0;
}

function testTrimStart(): number {
  let result: string = trimStart("  hello  ");
  if (result != "hello  ") { return 1; }
  return 0;
}

function testTrimEnd(): number {
  let result: string = trimEnd("  hello  ");
  if (result != "  hello") { return 1; }
  return 0;
}

function testPadStart(): number {
  let result: string = padStart("5", 3, "0");
  if (result != "005") { return 1; }
  return 0;
}

function testPadEnd(): number {
  let result: string = padEnd("5", 3, "0");
  if (result != "500") { return 1; }
  return 0;
}

function testRepeat(): number {
  let result: string = repeat("ab", 3);
  if (result != "ababab") { return 1; }
  return 0;
}

function testStartsWith(): number {
  let str: string = "hello world";
  if (!startsWith(str, "hello")) { return 1; }
  if (startsWith(str, "world")) { return 2; }
  return 0;
}

function testEndsWith(): number {
  let str: string = "hello world";
  if (!endsWith(str, "world")) { return 1; }
  if (endsWith(str, "hello")) { return 2; }
  return 0;
}

function testIncludes(): number {
  let str: string = "hello world";
  if (!includes(str, "lo wo")) { return 1; }
  if (includes(str, "xyz")) { return 2; }
  return 0;
}

function testIndexOf(): number {
  let str: string = "hello world";
  if (indexOf(str, "world") != 6) { return 1; }
  if (indexOf(str, "xyz") != -1) { return 2; }
  return 0;
}

function testReplace(): number {
  let result: string = replace("hello world", "world", "there");
  if (result != "hello there") { return 1; }
  return 0;
}

function testReplaceAll(): number {
  let result: string = replaceAll("aaabaaab", "aa", "x");
  if (result.length == 0) { return 1; }
  return 0;
}

function testSplit(): number {
  let result: string[] = split("a,b,c", ",");
  if (result.length != 3) { return 1; }
  if (result[0] != "a") { return 2; }
  if (result[1] != "b") { return 3; }
  return 0;
}

function testToUpperCase(): number {
  let result: string = toUpperCase("hello");
  if (result != "HELLO") { return 1; }
  return 0;
}

function testToLowerCase(): number {
  let result: string = toLowerCase("HELLO");
  if (result != "hello") { return 1; }
  return 0;
}

function testReverse(): number {
  let result: string = reverse("hello");
  if (result != "olleh") { return 1; }
  return 0;
}

function testSlice(): number {
  let result: string = slice("hello", 1, 4);
  if (result != "ell") { return 1; }
  return 0;
}

function testCharAt(): number {
  let result: string = charAt("hello", 1);
  if (result != "e") { return 1; }
  return 0;
}

function testSplitEmpty(): number {
  let result: string[] = split("abc", "");
  if (result.length == 0) { return 1; }
  return 0;
}

function testPadStartExceed(): number {
  let result: string = padStart("hello", 3, "0");
  if (result != "hello") { return 1; }
  return 0;
}

function testTrimNewline(): number {
  let result: string = trim("\nhello\n");
  if (result != "hello") { return 1; }
  return 0;
}
