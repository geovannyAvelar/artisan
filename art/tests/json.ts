import { stringify, parse } from "art/json";

function testStringifyString(): number {
  let result: string = stringify("hello");
  if (result != "\"hello\"") { return 1; }
  return 0;
}

function testStringifyNumber(): number {
  let result: string = stringify(42);
  if (result != "42") { return 1; }
  return 0;
}

function testStringifyBoolean(): number {
  let result: string = stringify(true);
  if (result != "true") { return 1; }
  return 0;
}

function testStringifyNull(): number {
  let result: string = stringify(null);
  if (result != "null") { return 1; }
  return 0;
}

function testStringifyArray(): number {
  let arr: number[] = [1, 2, 3];
  let result: string = stringify(arr);
  if (result != "[1,2,3]") { return 1; }
  return 0;
}

function testStringifyEmptyArray(): number {
  let arr: number[] = [];
  let result: string = stringify(arr);
  if (result != "[]") { return 1; }
  return 0;
}

function testStringifyStringEscape(): number {
  let result: string = stringify("hello\nworld");
  if (result != "\"hello\\nworld\"") { return 1; }
  return 0;
}

function testParseString(): number {
  let result: string = parse("\"hello\"") as string;
  if (result != "hello") { return 1; }
  return 0;
}

function testParseNumber(): number {
  let result: number = parse("42") as number;
  if (result != 42) { return 1; }
  return 0;
}

function testParseBoolean(): number {
  let result: boolean = parse("true") as boolean;
  if (!result) { return 1; }
  return 0;
}

function testParseNull(): number {
  let result: any = parse("null");
  if (result != null) { return 1; }
  return 0;
}

function testParseArray(): number {
  let result: any[] = parse("[1,2,3]") as any[];
  if (result.length != 3) { return 1; }
  if (result[0] != 1) { return 2; }
  return 0;
}

function testParseNegativeNumber(): number {
  let result: number = parse("-42") as number;
  if (result != -42) { return 1; }
  return 0;
}

function testRoundtrip(): number {
  let arr: number[] = [1, 2, 3];
  let stringified: string = stringify(arr);
  let parsed: any[] = parse(stringified) as any[];
  if (parsed.length != 3) { return 1; }
  if (parsed[0] != 1) { return 2; }
  return 0;
}

function testStringifyNestedArray(): number {
  let arr: number[] = [1, 2];
  let result: string = stringify([arr, arr]);
  if (result.length == 0) { return 1; }
  return 0;
}

function testParseEmptyArray(): number {
  let result: any[] = parse("[]") as any[];
  if (result.length != 0) { return 1; }
  return 0;
}

function testParseWhitespace(): number {
  let result: number = parse("  42  ") as number;
  if (result != 42) { return 1; }
  return 0;
}

function testStringifyQuote(): number {
  let result: string = stringify("\"quote\"");
  if (result != "\"\\\"quote\\\"\"") { return 1; }
  return 0;
}

function testParseStringWithSpace(): number {
  let result: string = parse("\"hello world\"") as string;
  if (result != "hello world") { return 1; }
  return 0;
}
