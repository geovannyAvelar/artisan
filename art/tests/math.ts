import { PI, E, abs, min, max, minArray, maxArray, clamp, sign, sum, average, product, floor, ceil, round, trunc, sqrt, pow, frac, remainder, gcd, lcm, isInteger, isEven, isOdd, lerp, inverseLerp, map, isPowerOf2, nextPowerOf2, degreesToRadians, radiansToDegrees } from "art/math";

function testConstants(): number {
  if (PI < 3.14 || PI > 3.15) { return 1; }
  if (E < 2.71 || E > 2.72) { return 2; }
  return 0;
}

function testAbs(): number {
  if (abs(5) != 5) { return 1; }
  if (abs(-5) != 5) { return 2; }
  if (abs(0) != 0) { return 3; }
  return 0;
}

function testMinMax(): number {
  if (min(3, 5) != 3) { return 1; }
  if (min(5, 3) != 3) { return 2; }
  if (max(3, 5) != 5) { return 3; }
  if (max(5, 3) != 5) { return 4; }
  return 0;
}

function testMinMaxArray(): number {
  let arr: number[] = [3, 1, 4, 1, 5, 9, 2, 6];
  if (minArray(arr) != 1) { return 1; }
  if (maxArray(arr) != 9) { return 2; }
  if (minArray([42]) != 42) { return 3; }
  if (maxArray([42]) != 42) { return 4; }
  if (minArray([]) != 0) { return 5; }
  return 0;
}

function testClamp(): number {
  if (clamp(5, 1, 10) != 5) { return 1; }
  if (clamp(0, 1, 10) != 1) { return 2; }
  if (clamp(15, 1, 10) != 10) { return 3; }
  if (clamp(-5, -10, 0) != -5) { return 4; }
  return 0;
}

function testSign(): number {
  if (sign(5) != 1) { return 1; }
  if (sign(-5) != -1) { return 2; }
  if (sign(0) != 0) { return 3; }
  if (sign(0.5) != 1) { return 4; }
  if (sign(-0.5) != -1) { return 5; }
  return 0;
}

function testSum(): number {
  if (sum([1, 2, 3, 4, 5]) != 15) { return 1; }
  if (sum([]) != 0) { return 2; }
  if (sum([42]) != 42) { return 3; }
  if (sum([1, -1]) != 0) { return 4; }
  return 0;
}

function testAverage(): number {
  if (average([2, 4, 6]) != 4) { return 1; }
  if (average([1]) != 1) { return 2; }
  if (average([]) != 0) { return 3; }
  if (average([10, 20]) != 15) { return 4; }
  return 0;
}

function testProduct(): number {
  if (product([2, 3, 4]) != 24) { return 1; }
  if (product([1]) != 1) { return 2; }
  if (product([0, 5]) != 0) { return 3; }
  if (product([2, 2, 2]) != 8) { return 4; }
  if (product([]) != 1) { return 5; }
  return 0;
}

function testFloor(): number {
  if (floor(3.7) != 3) { return 1; }
  if (floor(3.2) != 3) { return 2; }
  if (floor(3) != 3) { return 3; }
  if (floor(-3.7) != -4) { return 4; }
  if (floor(-3.2) != -4) { return 5; }
  if (floor(0) != 0) { return 6; }
  return 0;
}

function testCeil(): number {
  if (ceil(3.2) != 4) { return 1; }
  if (ceil(3.7) != 4) { return 2; }
  if (ceil(3) != 3) { return 3; }
  if (ceil(-3.7) != -3) { return 4; }
  if (ceil(-3.2) != -3) { return 5; }
  return 0;
}

function testRound(): number {
  if (round(3.2) != 3) { return 1; }
  if (round(3.5) != 4) { return 2; }
  if (round(3.7) != 4) { return 3; }
  if (round(-3.5) != -3) { return 4; }
  if (round(0) != 0) { return 5; }
  return 0;
}

function testTrunc(): number {
  if (trunc(3.7) != 3) { return 1; }
  if (trunc(3.2) != 3) { return 2; }
  if (trunc(-3.7) != -3) { return 3; }
  if (trunc(-3.2) != -3) { return 4; }
  if (trunc(0) != 0) { return 5; }
  return 0;
}

function testSqrt(): number {
  let sq2: number = sqrt(2);
  if (sq2 < 1.4 || sq2 > 1.5) { return 1; }
  if (sqrt(4) != 2) { return 2; }
  if (sqrt(9) != 3) { return 3; }
  if (sqrt(0) != 0) { return 4; }
  if (sqrt(1) != 1) { return 5; }
  if (sqrt(-1) != 0) { return 6; }
  return 0;
}

function testPow(): number {
  if (pow(2, 3) != 8) { return 1; }
  if (pow(2, 0) != 1) { return 2; }
  if (pow(2, 1) != 2) { return 3; }
  if (pow(3, 2) != 9) { return 4; }
  if (pow(0, 5) != 0) { return 5; }
  if (pow(1, 100) != 1) { return 6; }
  if (pow(2, -1) != 0.5) { return 7; }
  return 0;
}

function testFrac(): number {
  if (frac(3.7) < 0.69 || frac(3.7) > 0.71) { return 1; }
  if (frac(3) != 0) { return 2; }
  if (frac(-3.7) < -0.71 || frac(-3.7) > -0.69) { return 3; }
  return 0;
}

function testRemainder(): number {
  if (remainder(7, 3) != 1) { return 1; }
  if (remainder(10, 2) != 0) { return 2; }
  if (remainder(5, 5) != 0) { return 3; }
  if (remainder(3, 0) != 0) { return 4; }
  return 0;
}

function testGcd(): number {
  if (gcd(12, 8) != 4) { return 1; }
  if (gcd(17, 19) != 1) { return 2; }
  if (gcd(100, 50) != 50) { return 3; }
  if (gcd(0, 5) != 5) { return 4; }
  return 0;
}

function testLcm(): number {
  if (lcm(4, 6) != 12) { return 1; }
  if (lcm(3, 5) != 15) { return 2; }
  if (lcm(0, 5) != 0) { return 3; }
  return 0;
}

function testIsInteger(): number {
  if (!isInteger(5)) { return 1; }
  if (isInteger(5.5)) { return 2; }
  if (!isInteger(0)) { return 3; }
  if (!isInteger(-3)) { return 4; }
  return 0;
}

function testIsEvenOdd(): number {
  if (!isEven(4)) { return 1; }
  if (isEven(5)) { return 2; }
  if (isOdd(5)) { return 0; }
  return 3;
}

function testLerp(): number {
  if (lerp(0, 10, 0) != 0) { return 1; }
  if (lerp(0, 10, 1) != 10) { return 2; }
  if (lerp(0, 10, 0.5) != 5) { return 3; }
  if (lerp(5, 15, 0.5) != 10) { return 4; }
  return 0;
}

function testInverseLerp(): number {
  if (inverseLerp(0, 10, 0) != 0) { return 1; }
  if (inverseLerp(0, 10, 10) != 1) { return 2; }
  if (inverseLerp(0, 10, 5) != 0.5) { return 3; }
  if (inverseLerp(0, 10, 0) != inverseLerp(0, 10, 0)) { return 4; }
  return 0;
}

function testMap(): number {
  if (map(5, 0, 10, 0, 100) != 50) { return 1; }
  if (map(0, 0, 10, 100, 200) != 100) { return 2; }
  if (map(10, 0, 10, 100, 200) != 200) { return 3; }
  return 0;
}

function testIsPowerOf2(): number {
  if (!isPowerOf2(1)) { return 1; }
  if (!isPowerOf2(2)) { return 2; }
  if (!isPowerOf2(4)) { return 3; }
  if (!isPowerOf2(8)) { return 4; }
  if (isPowerOf2(3)) { return 5; }
  if (isPowerOf2(0)) { return 6; }
  if (isPowerOf2(-4)) { return 7; }
  return 0;
}

function testNextPowerOf2(): number {
  if (nextPowerOf2(1) != 1) { return 1; }
  if (nextPowerOf2(2) != 2) { return 2; }
  if (nextPowerOf2(3) != 4) { return 3; }
  if (nextPowerOf2(5) != 8) { return 4; }
  if (nextPowerOf2(8) != 8) { return 5; }
  if (nextPowerOf2(9) != 16) { return 6; }
  return 0;
}

function testAngleConversion(): number {
  if (degreesToRadians(0) != 0) { return 1; }
  if (degreesToRadians(180) < 3.14 || degreesToRadians(180) > 3.15) { return 2; }
  if (radiansToDegrees(0) != 0) { return 3; }
  let halfPi: number = PI / 2;
  if (radiansToDegrees(halfPi) < 89 || radiansToDegrees(halfPi) > 91) { return 4; }
  return 0;
}
