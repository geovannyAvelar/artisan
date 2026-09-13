import { setSeed, getSeed, random, randomInt, randomFloat, randomBool, randomChoice, shuffleRandom, randomExponential, randomNormal, randomUniform, randomWeighted, randomPermutation, randomSample, randomChoice } from "art/random";

function testSetAndGetSeed(): number {
  setSeed(42);
  if (getSeed() != 42) { return 1; }
  setSeed(0);
  if (getSeed() != 1) { return 2; }  // 0 becomes 1
  setSeed(12345);
  if (getSeed() != 12345) { return 3; }
  return 0;
}

function testRandomRange(): number {
  setSeed(42);
  let val: number = random();
  if (val < 0 || val >= 1) { return 1; }

  let val2: number = random();
  if (val2 < 0 || val2 >= 1) { return 2; }
  if (val == val2) { return 3; }  // Different values (extremely unlikely to be equal)
  return 0;
}

function testRandomDeterministic(): number {
  setSeed(42);
  let val1: number = random();
  let val2: number = random();

  setSeed(42);
  let val1b: number = random();
  let val2b: number = random();

  if (val1 != val1b) { return 1; }
  if (val2 != val2b) { return 2; }
  return 0;
}

function testRandomInt(): number {
  setSeed(42);
  let val: number = randomInt(1, 10);
  if (val < 1 || val > 10) { return 1; }

  let val2: number = randomInt(5, 5);
  if (val2 != 5) { return 2; }

  let val3: number = randomInt(10, 1);  // Swapped
  if (val3 < 1 || val3 > 10) { return 3; }

  return 0;
}

function testRandomIntRange(): number {
  setSeed(42);
  let count0: number = 0;
  let count1: number = 0;
  let i: number = 0;
  while (i < 100) {
    let val: number = randomInt(0, 1);
    if (val == 0) { count0 = count0 + 1; }
    if (val == 1) { count1 = count1 + 1; }
    i = i + 1;
  }
  if (count0 < 20 || count0 > 80) { return 1; }  // Should be roughly balanced
  if (count1 < 20 || count1 > 80) { return 2; }
  return 0;
}

function testRandomFloat(): number {
  setSeed(42);
  let val: number = randomFloat(0, 1);
  if (val < 0 || val >= 1) { return 1; }

  let val2: number = randomFloat(10, 20);
  if (val2 < 10 || val2 >= 20) { return 2; }

  let val3: number = randomFloat(20, 10);  // Swapped
  if (val3 < 10 || val3 >= 20) { return 3; }

  return 0;
}

function testRandomBool(): number {
  setSeed(42);
  let trueCount: number = 0;
  let falseCount: number = 0;
  let i: number = 0;
  while (i < 100) {
    if (randomBool()) {
      trueCount = trueCount + 1;
    } else {
      falseCount = falseCount + 1;
    }
    i = i + 1;
  }
  if (trueCount < 20 || trueCount > 80) { return 1; }
  if (falseCount < 20 || falseCount > 80) { return 2; }
  return 0;
}

function testRandomChoice(): number {
  setSeed(42);
  let arr: number[] = [10, 20, 30, 40, 50];
  let val: number = randomChoice(arr);
  if (val != 10 && val != 20 && val != 30 && val != 40 && val != 50) { return 1; }

  let empty: number[] = [];
  if (randomChoice(empty) != 0) { return 2; }

  return 0;
}

function testShuffleRandom(): number {
  setSeed(42);
  let arr: number[] = [1, 2, 3, 4, 5];
  shuffleRandom(arr);

  // Check all elements are still present
  let sum: number = 0;
  let i: number = 0;
  while (i < arr.length) {
    sum = sum + arr[i];
    i = i + 1;
  }
  if (sum != 15) { return 1; }

  // Check length unchanged
  if (arr.length != 5) { return 2; }

  return 0;
}

function testRandomExponential(): number {
  setSeed(42);
  let val: number = randomExponential(1);
  if (val < 0) { return 1; }  // Should be non-negative

  let val2: number = randomExponential(0);  // lambda <= 0 becomes 1
  if (val2 < 0) { return 2; }

  return 0;
}

function testRandomNormal(): number {
  setSeed(42);
  let val: number = randomNormal(0, 1);
  if (val < -10 || val > 10) { return 1; }  // Should be reasonably close to mean

  let val2: number = randomNormal(100, 10);
  if (val2 < 50 || val2 > 150) { return 2; }  // Should be roughly in range

  return 0;
}

function testRandomUniform(): number {
  setSeed(42);
  let val: number = randomUniform();
  if (val < 0 || val >= 1) { return 1; }
  return 0;
}

function testRandomWeighted(): number {
  setSeed(42);
  let val1: number = randomWeighted(1);
  if (val1 != 1) { return 1; }  // Weight 1 always returns 1

  let val2: number = randomWeighted(0);
  if (val2 != 0) { return 2; }  // Weight 0 always returns 0

  let val3: number = randomWeighted(0.5);
  if (val3 != 0 && val3 != 1) { return 3; }  // Should be 0 or 1

  return 0;
}

function testRandomPermutation(): number {
  setSeed(42);
  let perm: number[] = randomPermutation(5);

  if (perm.length != 5) { return 1; }

  // Check all values 0-4 are present
  let sum: number = 0;
  let i: number = 0;
  while (i < perm.length) {
    sum = sum + perm[i];
    i = i + 1;
  }
  if (sum != 10) { return 2; }  // 0+1+2+3+4 = 10

  return 0;
}

function testRandomSample(): number {
  setSeed(42);
  let arr: number[] = [10, 20, 30, 40, 50];
  let sample: number[] = randomSample(arr, 3);

  if (sample.length != 3) { return 1; }

  // Check all sampled values are from original
  let i: number = 0;
  while (i < sample.length) {
    let found: boolean = false;
    let j: number = 0;
    while (j < arr.length) {
      if (sample[i] == arr[j]) { found = true; }
      j = j + 1;
    }
    if (!found) { return 2; }
    i = i + 1;
  }

  return 0;
}

function testRandomSampleEdgeCases(): number {
  setSeed(42);
  let arr: number[] = [1, 2, 3];

  let sample1: number[] = randomSample(arr, 0);
  if (sample1.length != 0) { return 1; }

  let sample2: number[] = randomSample(arr, 10);  // More than available
  if (sample2.length != 3) { return 2; }

  let sample3: number[] = randomSample(arr, -5);  // Negative k
  if (sample3.length != 0) { return 3; }

  return 0;
}

function testRandomSequenceValidity(): number {
  setSeed(100);
  let sum: number = 0;
  let i: number = 0;
  while (i < 1000) {
    sum = sum + random();
    i = i + 1;
  }
  // Average should be around 500 (1000 * 0.5)
  if (sum < 400 || sum > 600) { return 1; }
  return 0;
}
