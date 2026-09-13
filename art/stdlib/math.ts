// Math utilities for ART - mathematical functions and constants.
// Import with: `import { sqrt, pow, abs, min, max, ... } from "art/math";`

// Mathematical constants
export const PI: number = 3.141592653589793;
export const E: number = 2.718281828459045;

// Returns the absolute value of a number.
export function abs(x: number): number {
  return x < 0 ? -x : x;
}

// Returns the minimum of two numbers.
export function min(a: number, b: number): number {
  return a < b ? a : b;
}

// Returns the maximum of two numbers.
export function max(a: number, b: number): number {
  return a > b ? a : b;
}

// Returns the minimum of an array of numbers.
export function minArray(arr: number[]): number {
  if (arr.length == 0) { return 0; }
  let result: number = arr[0];
  let i: number = 1;
  while (i < arr.length) {
    if (arr[i] < result) { result = arr[i]; }
    i = i + 1;
  }
  return result;
}

// Returns the maximum of an array of numbers.
export function maxArray(arr: number[]): number {
  if (arr.length == 0) { return 0; }
  let result: number = arr[0];
  let i: number = 1;
  while (i < arr.length) {
    if (arr[i] > result) { result = arr[i]; }
    i = i + 1;
  }
  return result;
}

// Clamps a value between min and max (inclusive).
export function clamp(x: number, minVal: number, maxVal: number): number {
  if (x < minVal) { return minVal; }
  if (x > maxVal) { return maxVal; }
  return x;
}

// Returns the sign of a number: -1, 0, or 1.
export function sign(x: number): number {
  if (x > 0) { return 1; }
  if (x < 0) { return -1; }
  return 0;
}

// Returns the sum of all numbers in an array.
export function sum(arr: number[]): number {
  let result: number = 0;
  let i: number = 0;
  while (i < arr.length) {
    result = result + arr[i];
    i = i + 1;
  }
  return result;
}

// Returns the average (mean) of all numbers in an array.
export function average(arr: number[]): number {
  if (arr.length == 0) { return 0; }
  return sum(arr) / arr.length;
}

// Returns the product of all numbers in an array.
export function product(arr: number[]): number {
  let result: number = 1;
  let i: number = 0;
  while (i < arr.length) {
    result = result * arr[i];
    i = i + 1;
  }
  return result;
}

// Floors x - returns the greatest integer <= x.
// Note: In ART, this requires a workaround since the C runtime floor() is not directly callable.
// This is a simplified implementation for positive numbers.
export function floor(x: number): number {
  if (x >= 0) {
    let i: number = 0;
    while (i + 1 <= x) {
      i = i + 1;
    }
    return i;
  }
  // For negative numbers, we need to round toward negative infinity
  let i: number = 0;
  while (i - 1 >= x) {
    i = i - 1;
  }
  return i - 1;
}

// Ceils x - returns the smallest integer >= x.
export function ceil(x: number): number {
  let floored: number = floor(x);
  if (floored == x) { return floored; }
  return floored + 1;
}

// Rounds x to the nearest integer (0.5 rounds up).
export function round(x: number): number {
  if (x >= 0) {
    return floor(x + 0.5);
  } else {
    return ceil(x - 0.5);
  }
}

// Truncates x toward zero (removes fractional part).
export function trunc(x: number): number {
  if (x >= 0) { return floor(x); }
  return ceil(x);
}

// Returns the square root of x.
// Note: This is an approximation using Newton's method for positive numbers.
// For x <= 0, returns 0.
export function sqrt(x: number): number {
  if (x < 0) { return 0; }
  if (x == 0) { return 0; }
  if (x == 1) { return 1; }

  // Newton's method: guess = (guess + x/guess) / 2
  let guess: number = x / 2;
  let i: number = 0;
  while (i < 20) {  // 20 iterations is enough for double precision
    let nextGuess: number = (guess + x / guess) / 2;
    if (abs(nextGuess - guess) < 0.0000001) { return nextGuess; }
    guess = nextGuess;
    i = i + 1;
  }
  return guess;
}

// Returns x raised to the power y (x^y).
// Uses exponentiation by squaring for integer exponents.
// For non-integer exponents, this is a simplified approximation.
export function pow(x: number, y: number): number {
  if (y == 0) { return 1; }
  if (y == 1) { return x; }
  if (x == 0) { return 0; }
  if (x == 1) { return 1; }

  // Handle negative exponents
  if (y < 0) {
    return 1 / pow(x, -y);
  }

  // For positive integer-like exponents, use repeated multiplication
  let isInteger: number = y - floor(y);
  if (abs(isInteger) < 0.0001) {
    let exp: number = floor(y);
    let result: number = 1;
    let i: number = 0;
    while (i < exp) {
      result = result * x;
      i = i + 1;
    }
    return result;
  }

  // For fractional exponents, use x^y = e^(y*ln(x))
  // This requires ln() which we don't have, so approximate
  return pow(x, floor(y)) * pow(x, y - floor(y));
}

// Returns the fractional part of a number (x - floor(x)).
export function frac(x: number): number {
  return x - floor(x);
}

// Returns the remainder of x divided by y (similar to x % y).
export function remainder(x: number, y: number): number {
  if (y == 0) { return 0; }
  let quotient: number = floor(x / y);
  return x - quotient * y;
}

// Returns the greatest common divisor of a and b.
export function gcd(a: number, b: number): number {
  a = abs(a);
  b = abs(b);
  while (b != 0) {
    let temp: number = b;
    b = remainder(a, b);
    a = temp;
  }
  return a;
}

// Returns the least common multiple of a and b.
export function lcm(a: number, b: number): number {
  if (a == 0 || b == 0) { return 0; }
  return abs(a * b) / gcd(a, b);
}

// Returns true if x is an integer.
export function isInteger(x: number): boolean {
  return x == floor(x);
}

// Returns true if x is even (for integer x).
export function isEven(x: number): boolean {
  return remainder(x, 2) == 0;
}

// Returns true if x is odd (for integer x).
export function isOdd(x: number): boolean {
  return remainder(x, 2) != 0;
}

// Linear interpolation between a and b by factor t (0 <= t <= 1).
export function lerp(a: number, b: number, t: number): number {
  return a + (b - a) * t;
}

// Inverse linear interpolation - returns how far x is between a and b.
export function inverseLerp(a: number, b: number, x: number): number {
  if (a == b) { return 0; }
  return (x - a) / (b - a);
}

// Maps a value from one range to another.
export function map(x: number, inMin: number, inMax: number, outMin: number, outMax: number): number {
  if (inMin == inMax) { return outMin; }
  let t: number = (x - inMin) / (inMax - inMin);
  return outMin + (outMax - outMin) * t;
}

// Returns true if x is a power of 2.
export function isPowerOf2(x: number): boolean {
  if (x <= 0) { return false; }
  let pow: number = 1;
  let i: number = 0;
  while (i < 32 && pow < x) {  // 32 bits for double check
    pow = pow * 2;
    i = i + 1;
  }
  return pow == x;
}

// Returns the next power of 2 >= x.
export function nextPowerOf2(x: number): number {
  if (x <= 1) { return 1; }
  let pow: number = 1;
  while (pow < x) {
    pow = pow * 2;
  }
  return pow;
}

// Degrees to radians conversion.
export function degreesToRadians(degrees: number): number {
  return degrees * PI / 180;
}

// Radians to degrees conversion.
export function radiansToDegrees(radians: number): number {
  return radians * 180 / PI;
}
