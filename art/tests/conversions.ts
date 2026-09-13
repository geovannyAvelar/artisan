import { toString, parseNumber, toBoolean, stringToBoolean, toLower, toUpper, toHex, parseHex, toBinary, parseBinary } from "art/conversions";

function testToString(): number {
  if (toString(0) != "0") { return 1; }
  if (toString(123) != "123") { return 2; }
  if (toString(-456) != "-456") { return 3; }
  if (toString(1) != "1") { return 4; }
  if (toString(9) != "9") { return 5; }
  return 0;
}

function testParseNumber(): number {
  if (parseNumber("0") != 0) { return 1; }
  if (parseNumber("123") != 123) { return 2; }
  if (parseNumber("-456") != -456) { return 3; }
  if (parseNumber("") != 0) { return 4; }
  if (parseNumber("+789") != 789) { return 5; }
  return 0;
}

function testParseNumberDecimal(): number {
  let val: number = parseNumber("3.5");
  if (val < 3.4 || val > 3.6) { return 1; }

  val = parseNumber("-2.5");
  if (val > -2.4 || val < -2.6) { return 2; }

  if (parseNumber("0.0") != 0) { return 3; }
  return 0;
}

function testToBoolean(): number {
  if (toBoolean(0)) { return 1; }
  if (!toBoolean(1)) { return 2; }
  if (!toBoolean(5)) { return 3; }
  if (!toBoolean(-1)) { return 4; }
  if (toBoolean(0.0)) { return 5; }
  return 0;
}

function testStringToBoolean(): number {
  if (stringToBoolean("")) { return 1; }
  if (stringToBoolean("0")) { return 2; }
  if (stringToBoolean("false")) { return 3; }
  if (stringToBoolean("FALSE")) { return 4; }
  if (stringToBoolean("no")) { return 5; }
  if (stringToBoolean("off")) { return 6; }
  if (!stringToBoolean("true")) { return 7; }
  if (!stringToBoolean("yes")) { return 8; }
  if (!stringToBoolean("1")) { return 9; }
  return 0;
}

function testToLower(): number {
  if (toLower("ABC") != "abc") { return 1; }
  if (toLower("Hello") != "hello") { return 2; }
  if (toLower("123") != "123") { return 3; }
  if (toLower("") != "") { return 4; }
  if (toLower("MiXeD") != "mixed") { return 5; }
  return 0;
}

function testToUpper(): number {
  if (toUpper("abc") != "ABC") { return 1; }
  if (toUpper("hello") != "HELLO") { return 2; }
  if (toUpper("123") != "123") { return 3; }
  if (toUpper("") != "") { return 4; }
  if (toUpper("MiXeD") != "MIXED") { return 5; }
  return 0;
}

function testToHex(): number {
  if (toHex(0) != "0") { return 1; }
  if (toHex(15) != "f") { return 2; }
  if (toHex(16) != "10") { return 3; }
  if (toHex(255) != "ff") { return 4; }
  if (toHex(256) != "100") { return 5; }
  if (toHex(-15) != "-f") { return 6; }
  return 0;
}

function testParseHex(): number {
  if (parseHex("0") != 0) { return 1; }
  if (parseHex("f") != 15) { return 2; }
  if (parseHex("F") != 15) { return 3; }
  if (parseHex("10") != 16) { return 4; }
  if (parseHex("0x10") != 16) { return 5; }
  if (parseHex("0X10") != 16) { return 6; }
  if (parseHex("ff") != 255) { return 7; }
  if (parseHex("FF") != 255) { return 8; }
  if (parseHex("-ff") != -255) { return 9; }
  return 0;
}

function testToBinary(): number {
  if (toBinary(0) != "0") { return 1; }
  if (toBinary(1) != "1") { return 2; }
  if (toBinary(2) != "10") { return 3; }
  if (toBinary(3) != "11") { return 4; }
  if (toBinary(4) != "100") { return 5; }
  if (toBinary(8) != "1000") { return 6; }
  if (toBinary(15) != "1111") { return 7; }
  if (toBinary(-8) != "-1000") { return 8; }
  return 0;
}

function testParseBinary(): number {
  if (parseBinary("0") != 0) { return 1; }
  if (parseBinary("1") != 1) { return 2; }
  if (parseBinary("10") != 2) { return 3; }
  if (parseBinary("11") != 3) { return 4; }
  if (parseBinary("100") != 4) { return 5; }
  if (parseBinary("1000") != 8) { return 6; }
  if (parseBinary("0b1010") != 10) { return 7; }
  if (parseBinary("0B1010") != 10) { return 8; }
  if (parseBinary("-1010") != -10) { return 9; }
  return 0;
}

function testRoundTrips(): number {
  let n: number = 42;
  if (parseNumber(toString(n)) != n) { return 1; }

  let h: string = toHex(255);
  if (parseHex(h) != 255) { return 2; }

  let b: string = toBinary(7);
  if (parseBinary(b) != 7) { return 3; }

  return 0;
}
