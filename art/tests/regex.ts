import { RegExp, compile, compileSimple, test, search, match, replace, split, startsWith, contains, getPattern, getFlags, literal, digit, notDigit, whitespace, notWhitespace, word, notWord, anyChar, startOfString, endOfString, digits, wordChars, email, url, hexColor, GLOBAL, IGNORE_CASE, MULTILINE, clearAllRegex } from "art/regex";

function testCompileSimple(): number {
  let regex: RegExp = compileSimple("hello");
  if (test(regex, "hello world")) { return 0; }
  return 1;
}

function testTestBasic(): number {
  let regex: RegExp = compile("test", 0);
  if (!test(regex, "test")) { return 1; }
  if (test(regex, "rest")) { return 2; }
  return 0;
}

function testSearchBasic(): number {
  let regex: RegExp = compile("world", 0);
  let pos: number = search(regex, "hello world");
  if (pos != 6) { return 1; }

  let notFound: number = search(regex, "hello");
  if (notFound != -1) { return 2; }
  return 0;
}

function testMatchBasic(): number {
  let regex: RegExp = compile("a", 0);
  let results: string[] = match(regex, "banana");
  if (results.length != 3) { return 1; }
  if (results[0] != "a") { return 2; }
  return 0;
}

function testReplaceFirst(): number {
  let regex: RegExp = compile("o", 0);
  let result: string = replace(regex, "hello world", "O");
  if (result != "hellO world") { return 1; }
  return 0;
}

function testReplaceGlobal(): number {
  let regex: RegExp = compile("o", GLOBAL);
  let result: string = replace(regex, "hello world", "O");
  if (result != "hellO wOrld") { return 1; }
  return 0;
}

function testSplitBasic(): number {
  let regex: RegExp = compile(",", 0);
  let parts: string[] = split(regex, "a,b,c");
  if (parts.length != 3) { return 1; }
  if (parts[0] != "a") { return 2; }
  if (parts[1] != "b") { return 3; }
  if (parts[2] != "c") { return 4; }
  return 0;
}

function testStartsWithPattern(): number {
  let regex: RegExp = compile("hello", 0);
  if (!startsWith(regex, "hello world")) { return 1; }
  if (startsWith(regex, "world hello")) { return 2; }
  return 0;
}

function testContainsPattern(): number {
  let regex: RegExp = compile("world", 0);
  if (!contains(regex, "hello world")) { return 1; }
  if (contains(regex, "hello")) { return 2; }
  return 0;
}

function testGetPatternAndFlags(): number {
  let regex: RegExp = compile("test", GLOBAL);
  let pat: string = getPattern(regex);
  if (pat != "test") { return 1; }

  let flags: number = getFlags(regex);
  if (flags != GLOBAL) { return 2; }
  return 0;
}

function testDigitPattern(): number {
  let regex: RegExp = digit();
  if (!test(regex, "5")) { return 1; }
  if (test(regex, "a")) { return 2; }
  return 0;
}

function testNotDigitPattern(): number {
  let regex: RegExp = notDigit();
  if (!test(regex, "a")) { return 1; }
  if (test(regex, "5")) { return 2; }
  return 0;
}

function testWhitespacePattern(): number {
  let regex: RegExp = whitespace();
  if (!test(regex, " ")) { return 1; }
  if (!test(regex, "\t")) { return 2; }
  if (!test(regex, "\n")) { return 3; }
  if (test(regex, "a")) { return 4; }
  return 0;
}

function testNotWhitespacePattern(): number {
  let regex: RegExp = notWhitespace();
  if (!test(regex, "a")) { return 1; }
  if (test(regex, " ")) { return 2; }
  return 0;
}

function testWordPattern(): number {
  let regex: RegExp = word();
  if (!test(regex, "a")) { return 1; }
  if (!test(regex, "5")) { return 2; }
  if (!test(regex, "_")) { return 3; }
  if (test(regex, " ")) { return 4; }
  return 0;
}

function testNotWordPattern(): number {
  let regex: RegExp = notWord();
  if (!test(regex, " ")) { return 1; }
  if (test(regex, "a")) { return 2; }
  return 0;
}

function testAnyCharPattern(): number {
  let regex: RegExp = anyChar();
  if (!test(regex, "a")) { return 1; }
  if (!test(regex, "5")) { return 2; }
  if (!test(regex, " ")) { return 3; }
  return 0;
}

function testStartOfStringPattern(): number {
  let regex: RegExp = compile("^hello", 0);
  if (!test(regex, "hello world")) { return 1; }
  if (test(regex, "say hello")) { return 2; }
  return 0;
}

function testEndOfStringPattern(): number {
  let regex: RegExp = compile("world$", 0);
  if (!test(regex, "hello world")) { return 1; }
  if (test(regex, "world hello")) { return 2; }
  return 0;
}

function testDigitsPattern(): number {
  let results: string[] = match(digits(), "abc123def456");
  if (results.length != 2) { return 1; }
  if (results[0] != "123") { return 2; }
  if (results[1] != "456") { return 3; }
  return 0;
}

function testWordCharsPattern(): number {
  let results: string[] = match(wordChars(), "hello_123 world");
  if (results.length != 2) { return 1; }
  if (results[0] != "hello_123") { return 2; }
  return 0;
}

function testLiteralPattern(): number {
  let regex: RegExp = literal("a.b");
  if (!test(regex, "a.b")) { return 1; }
  if (test(regex, "axb")) { return 2; }
  return 0;
}

function testEmailPattern(): number {
  let regex: RegExp = email();
  if (!contains(regex, "user@example.com")) { return 1; }
  if (contains(regex, "invalid.email")) { return 2; }
  return 0;
}

function testUrlPattern(): number {
  let regex: RegExp = url();
  if (!contains(regex, "https://example.com")) { return 1; }
  if (!contains(regex, "http://test.org")) { return 2; }
  if (contains(regex, "ftp://example.com")) { return 3; }
  return 0;
}

function testHexColorPattern(): number {
  let regex: RegExp = hexColor();
  if (!contains(regex, "#abc")) { return 1; }
  if (!contains(regex, "#123")) { return 2; }
  if (contains(regex, "#12")) { return 3; }
  return 0;
}

function testIgnoreCaseFlag(): number {
  let regex: RegExp = compile("hello", IGNORE_CASE);
  if (!test(regex, "HELLO")) { return 1; }
  if (!test(regex, "Hello")) { return 2; }
  if (!test(regex, "hello")) { return 3; }
  return 0;
}

function testCharacterClass(): number {
  let regex: RegExp = compile("[abc]", 0);
  if (!test(regex, "a")) { return 1; }
  if (!test(regex, "b")) { return 2; }
  if (!test(regex, "c")) { return 3; }
  if (test(regex, "d")) { return 4; }
  return 0;
}

function testNegatedCharClass(): number {
  let regex: RegExp = compile("[^abc]", 0);
  if (!test(regex, "d")) { return 1; }
  if (!test(regex, "x")) { return 2; }
  if (test(regex, "a")) { return 3; }
  if (test(regex, "b")) { return 4; }
  return 0;
}

function testCharRangePattern(): number {
  let regex: RegExp = compile("[a-z]", 0);
  if (!test(regex, "a")) { return 1; }
  if (!test(regex, "m")) { return 2; }
  if (!test(regex, "z")) { return 3; }
  if (test(regex, "A")) { return 4; }
  if (test(regex, "1")) { return 5; }
  return 0;
}

function testQuantifierStar(): number {
  let regex: RegExp = compile("ab*c", 0);
  if (!test(regex, "ac")) { return 1; }
  if (!test(regex, "abc")) { return 2; }
  if (!test(regex, "abbc")) { return 3; }
  if (test(regex, "ac")) { return 4; }
  return 0;
}

function testQuantifierPlus(): number {
  let regex: RegExp = compile("a+", 0);
  let results: string[] = match(regex, "aaa baa a");
  if (results.length != 3) { return 1; }
  return 0;
}

function testQuantifierQuestion(): number {
  let regex: RegExp = compile("colou?r", 0);
  if (!test(regex, "color")) { return 1; }
  if (!test(regex, "colour")) { return 2; }
  return 0;
}

function testEmptyPattern(): number {
  let regex: RegExp = compile("", 0);
  if (!test(regex, "")) { return 1; }
  if (!test(regex, "hello")) { return 2; }
  return 0;
}

function testEmptyString(): number {
  let regex: RegExp = compile("a", 0);
  if (test(regex, "")) { return 1; }
  return 0;
}

function testSpecialCharEscape(): number {
  let regex: RegExp = literal("[abc]");
  if (!test(regex, "[abc]")) { return 1; }
  if (test(regex, "a")) { return 2; }
  return 0;
}

function testReplaceEmpty(): number {
  let regex: RegExp = compile("a", GLOBAL);
  let result: string = replace(regex, "banana", "");
  if (result != "bnn") { return 1; }
  return 0;
}

function testSplitWithEmpty(): number {
  let regex: RegExp = compile("", 0);
  let parts: string[] = split(regex, "abc");
  if (parts.length != 1) { return 1; }
  return 0;
}

function testMatchWithGlobal(): number {
  let regex: RegExp = compile("l", GLOBAL);
  let results: string[] = match(regex, "hello");
  if (results.length != 2) { return 1; }
  return 0;
}

function testSearchReturnsFirstMatch(): number {
  let regex: RegExp = compile("l", 0);
  let pos: number = search(regex, "hello");
  if (pos != 2) { return 1; }
  return 0;
}

function testInvalidRegexId(): number {
  let regex: RegExp = 999;
  if (test(regex, "test")) { return 1; }
  if (search(regex, "test") != -1) { return 2; }
  if (match(regex, "test").length != 0) { return 3; }
  return 0;
}

function testMultipleRegexes(): number {
  let regex1: RegExp = compile("a", 0);
  let regex2: RegExp = compile("b", 0);

  if (!test(regex1, "abc")) { return 1; }
  if (!test(regex2, "abc")) { return 2; }
  if (test(regex1, "bcd")) { return 3; }

  return 0;
}

function testCaseInsensitiveReplace(): number {
  let regex: RegExp = compile("hello", IGNORE_CASE + GLOBAL);
  let result: string = replace(regex, "Hello HELLO hello", "HI");
  if (result != "HI HI HI") { return 1; }
  return 0;
}

function testComplexPattern(): number {
  let regex: RegExp = compile("[0-9]+", 0);
  let results: string[] = match(regex, "x1y22z333");
  if (results.length != 3) { return 1; }
  if (results[0] != "1") { return 2; }
  if (results[1] != "22") { return 3; }
  if (results[2] != "333") { return 4; }
  return 0;
}

function testGetPatternInvalidId(): number {
  let regex: RegExp = 999;
  if (getPattern(regex) != "") { return 1; }
  if (getFlags(regex) != 0) { return 2; }
  return 0;
}
