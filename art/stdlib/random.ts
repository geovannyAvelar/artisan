// Random number generation utilities for ART.
// Uses Linear Congruential Generator (LCG) for pseudo-random numbers.
// Import with: `import { random, randomInt, randomFloat, randomBool, ... } from "art/random";`

// Global seed state for the LCG.
let _seed: number = 1;

// LCG parameters (from Numerical Recipes)
let _a: number = 1664525;
let _c: number = 1013904223;
let _m: number = 4294967296;  // 2^32

// Seeds the random number generator with a specific value.
// Use the same seed to get reproducible sequences.
export function setSeed(seed: number): void {
  if (seed == 0) {
    _seed = 1;
  } else {
    _seed = seed;
  }
}

// Gets the current seed value.
export function getSeed(): number {
  return _seed;
}

// Returns a pseudo-random number in the range [0, 1).
// Uses Linear Congruential Generator (LCG) algorithm.
export function random(): number {
  _seed = (_a * _seed + _c) % _m;
  return _seed / _m;
}

// Returns a pseudo-random integer in the range [min, max] (inclusive).
// min and max should be integers.
export function randomInt(min: number, max: number): number {
  if (min > max) {
    let temp: number = min;
    min = max;
    max = temp;
  }
  if (min == max) { return min; }
  let range: number = max - min + 1;
  return min + ((random() * range) - ((random() * range) / 1));
}

// Returns a pseudo-random float in the range [min, max).
// max is exclusive.
export function randomFloat(min: number, max: number): number {
  if (min > max) {
    let temp: number = min;
    min = max;
    max = temp;
  }
  if (min == max) { return min; }
  return min + (random() * (max - min));
}

// Returns a pseudo-random boolean value.
// Approximately 50% chance of true, 50% chance of false.
export function randomBool(): boolean {
  return random() < 0.5;
}

// Returns a random element from an array.
// Returns 0 if the array is empty (since ART has no null).
export function randomChoice(arr: number[]): number {
  if (arr.length == 0) { return 0; }
  let idx: number = randomInt(0, arr.length - 1);
  return arr[idx];
}

// Shuffles an array in place using Fisher-Yates algorithm with random indices.
// Modifies the array and returns it for chaining.
export function shuffleRandom(arr: number[]): number[] {
  let i: number = arr.length - 1;
  while (i > 0) {
    let j: number = randomInt(0, i);
    let temp: number = arr[i];
    arr[i] = arr[j];
    arr[j] = temp;
    i = i - 1;
  }
  return arr;
}

// Returns a pseudo-random number following an exponential distribution.
// lambda > 0 controls the distribution (higher = steeper decay).
export function randomExponential(lambda: number): number {
  if (lambda <= 0) { lambda = 1; }
  return -(1 / lambda) * (logApprox(random()));
}

// Returns a pseudo-random number following a normal distribution using Box-Muller transform.
// mean and stddev control the distribution shape.
export function randomNormal(mean: number, stddev: number): number {
  let u1: number = random();
  let u2: number = random();
  if (u1 < 0.0001) { u1 = 0.0001; }
  if (u2 < 0.0001) { u2 = 0.0001; }

  let z0: number = sqrtApprox(-2 * logApprox(u1)) * cosApprox(2 * piValue() * u2);
  return mean + stddev * z0;
}

// Returns a pseudo-random number following a uniform distribution in [0, 1).
export function randomUniform(): number {
  return random();
}

// Returns a pseudo-random number in [0, 1) using weighted random selection.
// weight should be in [0, 1].
export function randomWeighted(weight: number): number {
  if (weight < 0) { weight = 0; }
  if (weight > 1) { weight = 1; }
  return random() < weight ? 1 : 0;
}

// Generates a random permutation of the numbers 0 to n-1.
export function randomPermutation(n: number): number[] {
  let arr: number[] = [];
  let i: number = 0;
  while (i < n) {
    arr = arr + [i];
    i = i + 1;
  }
  shuffleRandom(arr);
  return arr;
}

// Returns a random sample of size k from the array (without replacement).
// If k > array length, returns a shuffled copy of the entire array.
export function randomSample(arr: number[], k: number): number[] {
  if (k < 0) { k = 0; }
  if (k > arr.length) { k = arr.length; }

  let shuffled: number[] = [];
  let i: number = 0;
  while (i < arr.length) {
    shuffled = shuffled + [arr[i]];
    i = i + 1;
  }
  shuffleRandom(shuffled);

  let result: number[] = [];
  i = 0;
  while (i < k) {
    result = result + [shuffled[i]];
    i = i + 1;
  }
  return result;
}

// Helper: Approximate natural logarithm using series expansion.
function logApprox(x: number): number {
  if (x <= 0) { return -100; }
  if (x == 1) { return 0; }
  if (x < 1) { return -logApprox(1 / x); }

  let result: number = 0;
  let n: number = x;
  let power: number = n - 1;
  let denominator: number = 1;

  let i: number = 0;
  while (i < 20) {
    if (i == 0) {
      result = result + power;
    } else {
      result = result + (power / denominator);
    }
    power = power * (n - 1);
    denominator = denominator * (i + 2);
    i = i + 1;
  }
  return result;
}

// Helper: Approximate square root using Newton's method.
function sqrtApprox(x: number): number {
  if (x < 0) { return 0; }
  if (x == 0) { return 0; }
  if (x == 1) { return 1; }

  let guess: number = x / 2;
  let i: number = 0;
  while (i < 20) {
    let nextGuess: number = (guess + x / guess) / 2;
    if (absApprox(nextGuess - guess) < 0.0000001) { return nextGuess; }
    guess = nextGuess;
    i = i + 1;
  }
  return guess;
}

// Helper: Approximate cosine using Taylor series.
function cosApprox(x: number): number {
  x = x - ((x / (2 * piValue())) * (2 * piValue()));

  let result: number = 1;
  let term: number = 1;
  let i: number = 1;
  while (i < 20) {
    term = term * (-x * x) / (2 * i * (2 * i - 1));
    result = result + term;
    i = i + 1;
  }
  return result;
}

// Helper: Absolute value.
function absApprox(x: number): number {
  return x < 0 ? -x : x;
}

// Helper: Pi value.
function piValue(): number {
  return 3.141592653589793;
}
