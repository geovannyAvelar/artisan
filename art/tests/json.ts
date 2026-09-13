import { stringify, parse, stringifyArray, parseArray, isValidJSON } from "art/json";

function testStringifyNumber(): number {
  if (stringify(0) != "0") { return 1; }
  if (stringify(123) != "123") { return 2; }
  if (stringify(-456) != "-456") { return 3; }

  let s: string = stringify(3.14);
  if (s.substring(0, 3) != "3.1") { return 4; }  // Approximate

  return 0;
}

function testParseNumber(): number {
  if (parse("0") != 0) { return 1; }
  if (parse("123") != 123) { return 2; }
  if (parse("-456") != -456) { return 3; }

  let p: number = parse("3.14");
  if (p < 3.1 || p > 3.2) { return 4; }

  if (parse("") != 0) { return 5; }  // Invalid
  if (parse("abc") != 0) { return 6; }  // Invalid
  return 0;
}

function testStringifyArray(): number {
  let result: string = stringifyArray([]);
  if (result != "[]") { return 1; }

  result = stringifyArray([1, 2, 3]);
  if (result != "[1, 2, 3]") { return 2; }

  result = stringifyArray([0]);
  if (result != "[0]") { return 3; }

  return 0;
}

function testParseArray(): number {
  let arr1: number[] = parseArray("");
  if (arr1.length != 0) { return 1; }

  let arr2: number[] = parseArray("[]");
  if (arr2.length != 0) { return 2; }

  let arr3: number[] = parseArray("[1, 2, 3]");
  if (arr3.length != 3) { return 3; }
  if (arr3[0] != 1 || arr3[1] != 2 || arr3[2] != 3) { return 4; }

  let arr4: number[] = parseArray("[0]");
  if (arr4.length != 1 || arr4[0] != 0) { return 5; }

  return 0;
}

function testIsValidJSON(): number {
  if (!isValidJSON("0")) { return 1; }
  if (!isValidJSON("123")) { return 2; }
  if (!isValidJSON("-456")) { return 3; }

  if (!isValidJSON("[]")) { return 4; }
  if (!isValidJSON("[1, 2, 3]")) { return 5; }

  if (isValidJSON("")) { return 6; }
  if (isValidJSON("abc")) { return 7; }
  if (isValidJSON("[")) { return 8; }
  if (isValidJSON("{")) { return 9; }

  return 0;
}

function testRoundTripNumber(): number {
  let orig: number = 42;
  let stringified: string = stringify(orig);
  let parsed: number = parse(stringified);
  if (parsed != orig) { return 1; }

  return 0;
}

function testRoundTripArray(): number {
  let orig: number[] = [1, 2, 3, 4, 5];
  let stringified: string = stringifyArray(orig);
  let parsed: number[] = parseArray(stringified);

  if (parsed.length != orig.length) { return 1; }
  let i: number = 0;
  while (i < orig.length) {
    if (parsed[i] != orig[i]) { return 2; }
    i = i + 1;
  }

  return 0;
}

function testParseArrayWithSpaces(): number {
  let arr: number[] = parseArray("[ 1 , 2 , 3 ]");
  if (arr.length != 3) { return 1; }
  if (arr[0] != 1 || arr[1] != 2 || arr[2] != 3) { return 2; }
  return 0;
}

function testNegativeNumbers(): number {
  let neg: string = stringify(-123);
  if (neg != "-123") { return 1; }

  let parsed: number = parse("-456");
  if (parsed != -456) { return 2; }

  let arr: number[] = parseArray("[-1, -2, -3]");
  if (arr.length != 3 || arr[0] != -1) { return 3; }

  return 0;
}
