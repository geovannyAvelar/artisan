import { split, toLowerCase, toUpperCase, replace, search, match } from "art/strings";

function testSplit(): number {
  let result: string[] = split("a,b,c", ",");
  if (result.length != 3) { return 1; }
  if (result[0] != "a" || result[1] != "b" || result[2] != "c") { return 2; }

  let empty: string[] = split("abc", "");
  if (empty.length != 3) { return 3; }
  return 0;
}

function testToLowerCase(): number {
  if (toLowerCase("ABC") != "abc") { return 1; }
  if (toLowerCase("Hello") != "hello") { return 2; }
  if (toLowerCase("123") != "123") { return 3; }
  return 0;
}

function testToUpperCase(): number {
  if (toUpperCase("abc") != "ABC") { return 1; }
  if (toUpperCase("Hello") != "HELLO") { return 2; }
  if (toUpperCase("123") != "123") { return 3; }
  return 0;
}

function testReplace(): number {
  if (replace("hello world", "world", "everyone") != "hello everyone") { return 1; }
  if (replace("aaa", "a", "b") != "baa") { return 2; }  // First occurrence only
  if (replace("no match", "xyz", "abc") != "no match") { return 3; }
  return 0;
}

function testSearch(): number {
  if (search("hello world", "world") != 6) { return 1; }
  if (search("hello world", "xyz") != -1) { return 2; }
  return 0;
}

function testMatch(): number {
  if (!match("hello", "ell")) { return 1; }
  if (match("hello", "xyz")) { return 2; }
  return 0;
}
