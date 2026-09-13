import { parseInt, parseFloat, isNaN, isFinite, typeOf, isInteger, isSafeInteger, toNumber, isPositive, isNegative, isZero, abs, round, trunc, floor, ceil, sign, clamp, isBetween, min, max } from "art/globals";

function testParseInt(): number {
  if (parseInt("123", 10) != 123) { return 1; }
  if (parseInt("-456", 10) != -456) { return 2; }
  if (parseInt("0", 10) != 0) { return 3; }
  if (parseInt("1010", 2) != 10) { return 4; }  // Binary
  if (parseInt("FF", 16) != 255) { return 5; }  // Hex
  if (parseInt("0x10", 10) != 16) { return 6; }  // Auto-detect hex
  if (parseInt("", 10) != 0) { return 7; }  // Empty string
  if (parseInt("abc", 10) != 0) { return 8; }  // Invalid
  return 0;
}

function testParseFloat(): number {
  let f1: number = parseFloat("3.14");
  if (f1 < 3.1 || f1 > 3.2) { return 1; }

  let f2: number = parseFloat("-2.5");
  if (f2 > -2.4 || f2 < -2.6) { return 2; }

  if (parseFloat("0") != 0) { return 3; }
  if (parseFloat("") != 0) { return 4; }
  if (parseFloat("abc") != 0) { return 5; }
  return 0;
}

function testIsNaN(): number {
  // NaN is the only value not equal to itself in ART
  let nan: number = 0 / 0;
  if (!isNaN(nan)) { return 1; }

  if (isNaN(5)) { return 2; }
  if (isNaN(0)) { return 3; }
  if (isNaN(-1)) { return 4; }
  return 0;
}

function testIsFinite(): number {
  if (!isFinite(0)) { return 1; }
  if (!isFinite(123.456)) { return 2; }
  if (!isFinite(-999)) { return 3; }

  // Very large numbers considered infinite
  if (isFinite(999999999999999)) { return 4; }
  if (isFinite(-999999999999999)) { return 5; }

  let nan: number = 0 / 0;
  if (isFinite(nan)) { return 6; }
  return 0;
}

function testTypeOf(): number {
  if (typeOf(0) != "number") { return 1; }
  if (typeOf(123.456) != "number") { return 2; }
  if (typeOf(-999) != "number") { return 3; }
  return 0;
}

function testIsInteger(): number {
  if (!isInteger(5)) { return 1; }
  if (!isInteger(0)) { return 2; }
  if (!isInteger(-10)) { return 3; }
  if (isInteger(3.14)) { return 4; }
  if (isInteger(0.5)) { return 5; }
  return 0;
}

function testIsSafeInteger(): number {
  if (!isSafeInteger(0)) { return 1; }
  if (!isSafeInteger(100)) { return 2; }
  if (!isSafeInteger(-999)) { return 3; }

  if (isSafeInteger(3.14)) { return 4; }
  if (isSafeInteger(10000000000000000)) { return 5; }  // Too large
  return 0;
}

function testIsPositive(): number {
  if (!isPositive(1)) { return 1; }
  if (!isPositive(0.001)) { return 2; }
  if (isPositive(0)) { return 3; }
  if (isPositive(-1)) { return 4; }
  return 0;
}

function testIsNegative(): number {
  if (!isNegative(-1)) { return 1; }
  if (!isNegative(-0.001)) { return 2; }
  if (isNegative(0)) { return 3; }
  if (isNegative(1)) { return 4; }
  return 0;
}

function testIsZero(): number {
  if (!isZero(0)) { return 1; }
  if (isZero(0.0001)) { return 2; }
  if (isZero(-1)) { return 3; }
  if (isZero(1)) { return 4; }
  return 0;
}

function testAbs(): number {
  if (abs(5) != 5) { return 1; }
  if (abs(-5) != 5) { return 2; }
  if (abs(0) != 0) { return 3; }
  if (abs(-3.14) < 3.1 || abs(-3.14) > 3.2) { return 4; }
  return 0;
}

function testRound(): number {
  if (round(3.2) != 3) { return 1; }
  if (round(3.7) != 4) { return 2; }
  if (round(-2.3) != -2) { return 3; }
  if (round(-2.7) != -3) { return 4; }
  if (round(5) != 5) { return 5; }
  return 0;
}

function testTrunc(): number {
  if (trunc(3.9) != 3) { return 1; }
  if (trunc(-3.9) != -3) { return 2; }
  if (trunc(5) != 5) { return 3; }
  return 0;
}

function testFloor(): number {
  if (floor(3.9) != 3) { return 1; }
  if (floor(-3.1) != -4) { return 2; }
  if (floor(5) != 5) { return 3; }
  return 0;
}

function testCeil(): number {
  if (ceil(3.1) != 4) { return 1; }
  if (ceil(-3.9) != -3) { return 2; }
  if (ceil(5) != 5) { return 3; }
  return 0;
}

function testSign(): number {
  if (sign(5) != 1) { return 1; }
  if (sign(-5) != -1) { return 2; }
  if (sign(0) != 0) { return 3; }
  return 0;
}

function testClamp(): number {
  if (clamp(5, 0, 10) != 5) { return 1; }
  if (clamp(-5, 0, 10) != 0) { return 2; }
  if (clamp(15, 0, 10) != 10) { return 3; }
  return 0;
}

function testIsBetween(): number {
  if (!isBetween(5, 0, 10)) { return 1; }
  if (!isBetween(0, 0, 10)) { return 2; }
  if (!isBetween(10, 0, 10)) { return 3; }
  if (isBetween(-1, 0, 10)) { return 4; }
  if (isBetween(11, 0, 10)) { return 5; }
  return 0;
}

function testMin(): number {
  if (min(5, 10) != 5) { return 1; }
  if (min(-5, -10) != -10) { return 2; }
  if (min(0, 0) != 0) { return 3; }
  return 0;
}

function testMax(): number {
  if (max(5, 10) != 10) { return 1; }
  if (max(-5, -10) != -5) { return 2; }
  if (max(0, 0) != 0) { return 3; }
  return 0;
}

function testToNumber(): number {
  if (toNumber(5) != 5) { return 1; }
  if (toNumber(-3.14) < -3.2 || toNumber(-3.14) > -3.0) { return 2; }
  return 0;
}
