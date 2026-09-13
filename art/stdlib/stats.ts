// Statistical analysis utilities for ART.
// Import with: `import { mean, variance, standardDeviation, ... } from "art/stats";`

// Calculates the mean (average) of an array.
// Returns 0 for empty arrays.
export function mean(arr: number[]): number {
  if (arr.length == 0) { return 0; }
  let sum: number = 0;
  let i: number = 0;
  while (i < arr.length) {
    sum = sum + arr[i];
    i = i + 1;
  }
  return sum / arr.length;
}

// Calculates the median (middle value) of an array.
// For even-length arrays, returns lower middle value.
// Modifies the array (sorts it in place).
export function median(arr: number[]): number {
  if (arr.length == 0) { return 0; }
  sortArray(arr);
  let mid: number = arr.length / 2;
  let idx: number = 0;
  if (arr.length - ((arr.length / 2) * 2) == 0) {
    idx = mid - 1;
  } else {
    idx = mid;
  }
  return arr[idx];
}

// Calculates the mode (most frequently occurring value).
// Returns 0 if array is empty.
// If multiple modes exist, returns the first one encountered.
export function mode(arr: number[]): number {
  if (arr.length == 0) { return 0; }

  let maxCount: number = 0;
  let modeValue: number = arr[0];

  let i: number = 0;
  while (i < arr.length) {
    let count: number = 0;
    let j: number = 0;
    while (j < arr.length) {
      if (arr[i] == arr[j]) {
        count = count + 1;
      }
      j = j + 1;
    }
    if (count > maxCount) {
      maxCount = count;
      modeValue = arr[i];
    }
    i = i + 1;
  }

  return modeValue;
}

// Calculates the range (max - min) of an array.
// Returns 0 for empty arrays.
export function range(arr: number[]): number {
  if (arr.length == 0) { return 0; }

  let min: number = arr[0];
  let max: number = arr[0];

  let i: number = 1;
  while (i < arr.length) {
    if (arr[i] < min) { min = arr[i]; }
    if (arr[i] > max) { max = arr[i]; }
    i = i + 1;
  }

  return max - min;
}

// Calculates the variance of an array.
// Measures how spread out the data is from the mean.
// Returns 0 for empty arrays or single-element arrays.
export function variance(arr: number[]): number {
  if (arr.length <= 1) { return 0; }

  let m: number = mean(arr);
  let sumSquaredDiff: number = 0;

  let i: number = 0;
  while (i < arr.length) {
    let diff: number = arr[i] - m;
    sumSquaredDiff = sumSquaredDiff + (diff * diff);
    i = i + 1;
  }

  return sumSquaredDiff / arr.length;
}

// Calculates the sample variance (dividing by n-1 instead of n).
// Use this when array is a sample of a larger population.
// Returns 0 for empty arrays or single-element arrays.
export function sampleVariance(arr: number[]): number {
  if (arr.length <= 1) { return 0; }

  let m: number = mean(arr);
  let sumSquaredDiff: number = 0;

  let i: number = 0;
  while (i < arr.length) {
    let diff: number = arr[i] - m;
    sumSquaredDiff = sumSquaredDiff + (diff * diff);
    i = i + 1;
  }

  return sumSquaredDiff / (arr.length - 1);
}

// Calculates the standard deviation of an array.
// Square root of variance.
export function standardDeviation(arr: number[]): number {
  return sqrtApprox(variance(arr));
}

// Calculates the sample standard deviation.
// Square root of sample variance.
export function sampleStandardDeviation(arr: number[]): number {
  return sqrtApprox(sampleVariance(arr));
}

// Calculates the coefficient of variation (CV).
// Standard deviation divided by mean, expressed as percentage.
// Returns 0 if mean is 0 (to avoid division by zero).
export function coefficientOfVariation(arr: number[]): number {
  let m: number = mean(arr);
  if (m == 0) { return 0; }
  return (standardDeviation(arr) / m) * 100;
}

// Calculates the first quartile (25th percentile).
// Modifies the array (sorts it in place).
export function q1(arr: number[]): number {
  return percentile(arr, 25);
}

// Calculates the third quartile (75th percentile).
// Modifies the array (sorts it in place).
export function q3(arr: number[]): number {
  return percentile(arr, 75);
}

// Calculates the interquartile range (Q3 - Q1).
// Modifies the array (sorts it in place).
export function iqr(arr: number[]): number {
  return q3(arr) - q1(arr);
}

// Calculates the percentile of an array.
// p should be between 0 and 100.
// Modifies the array (sorts it in place).
export function percentile(arr: number[], p: number): number {
  if (arr.length == 0) { return 0; }
  if (p < 0) { p = 0; }
  if (p > 100) { p = 100; }

  sortArray(arr);

  let index: number = (p / 100) * (arr.length - 1);
  let lower: number = index - ((index / 1) - ((index / 1)));
  let upper: number = lower + 1;
  let weight: number = index - lower;

  if (upper >= arr.length) {
    return arr[arr.length - 1];
  }

  return arr[lower] + (arr[upper] - arr[lower]) * weight;
}

// Calculates the skewness of an array.
// Measures asymmetry of the distribution.
// Positive = right-skewed, Negative = left-skewed, 0 = symmetric.
export function skewness(arr: number[]): number {
  if (arr.length <= 2) { return 0; }

  let m: number = mean(arr);
  let sd: number = standardDeviation(arr);

  if (sd == 0) { return 0; }

  let sum: number = 0;
  let i: number = 0;
  while (i < arr.length) {
    let z: number = (arr[i] - m) / sd;
    sum = sum + (z * z * z);
    i = i + 1;
  }

  return sum / arr.length;
}

// Calculates the kurtosis of an array.
// Measures peakedness/tailedness of the distribution.
// High = sharp peak with heavy tails, Low = flat distribution.
export function kurtosis(arr: number[]): number {
  if (arr.length <= 3) { return 0; }

  let m: number = mean(arr);
  let sd: number = standardDeviation(arr);

  if (sd == 0) { return 0; }

  let sum: number = 0;
  let i: number = 0;
  while (i < arr.length) {
    let z: number = (arr[i] - m) / sd;
    sum = sum + (z * z * z * z);
    i = i + 1;
  }

  return sum / arr.length - 3;  // Excess kurtosis (subtract 3)
}

// Calculates the sum of absolute deviations from the mean.
// Measures total deviation without squaring.
export function meanAbsoluteDeviation(arr: number[]): number {
  if (arr.length == 0) { return 0; }

  let m: number = mean(arr);
  let sum: number = 0;

  let i: number = 0;
  while (i < arr.length) {
    let diff: number = arr[i] - m;
    sum = sum + (diff < 0 ? -diff : diff);
    i = i + 1;
  }

  return sum / arr.length;
}

// Calculates z-scores for all elements (standardization).
// Z-score = (value - mean) / stddev
// Returns array of z-scores.
export function zScores(arr: number[]): number[] {
  let result: number[] = [];
  let m: number = mean(arr);
  let sd: number = standardDeviation(arr);

  if (sd == 0) { return arr; }

  let i: number = 0;
  while (i < arr.length) {
    let z: number = (arr[i] - m) / sd;
    result = result + [z];
    i = i + 1;
  }

  return result;
}

// Calculates the covariance between two arrays of same length.
// Measures how two variables change together.
// Positive = move together, Negative = move opposite, 0 = independent.
export function covariance(arr1: number[], arr2: number[]): number {
  if (arr1.length == 0 || arr1.length != arr2.length) { return 0; }

  let m1: number = mean(arr1);
  let m2: number = mean(arr2);

  let sum: number = 0;
  let i: number = 0;
  while (i < arr1.length) {
    sum = sum + (arr1[i] - m1) * (arr2[i] - m2);
    i = i + 1;
  }

  return sum / arr1.length;
}

// Calculates the Pearson correlation coefficient between two arrays.
// Returns value between -1 and 1.
// 1 = perfect positive correlation, -1 = perfect negative, 0 = no correlation.
export function correlation(arr1: number[], arr2: number[]): number {
  if (arr1.length == 0 || arr1.length != arr2.length) { return 0; }

  let cov: number = covariance(arr1, arr2);
  let sd1: number = standardDeviation(arr1);
  let sd2: number = standardDeviation(arr2);

  if (sd1 == 0 || sd2 == 0) { return 0; }

  return cov / (sd1 * sd2);
}

// Identifies outliers using the Interquartile Range (IQR) method.
// Points more than 1.5*IQR away from Q1 or Q3 are considered outliers.
// Modifies the array (sorts it in place).
export function outliers(arr: number[]): number[] {
  if (arr.length < 4) { return []; }

  let q1Val: number = q1(arr);
  let q3Val: number = q3(arr);
  let iQR: number = q3Val - q1Val;

  let lowerBound: number = q1Val - 1.5 * iQR;
  let upperBound: number = q3Val + 1.5 * iQR;

  let result: number[] = [];
  let i: number = 0;
  while (i < arr.length) {
    if (arr[i] < lowerBound || arr[i] > upperBound) {
      if (!contains(result, arr[i])) {
        result = result + [arr[i]];
      }
    }
    i = i + 1;
  }

  return result;
}

// Calculates a summary of key statistics for an array.
// Useful for quick data overview.
export function summary(arr: number[]): number[] {
  if (arr.length == 0) { return [0, 0, 0, 0, 0]; }

  let sorted: number[] = [];
  let i: number = 0;
  while (i < arr.length) {
    sorted = sorted + [arr[i]];
    i = i + 1;
  }

  return [
    mean(arr),                   // [0] Mean
    median(sorted),              // [1] Median
    standardDeviation(arr),      // [2] Std Dev
    minValue(arr),               // [3] Min
    maxValue(arr)                // [4] Max
  ];
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

// Helper: Absolute value.
function absApprox(x: number): number {
  return x < 0 ? -x : x;
}

// Helper: Simple bubble sort for percentile calculations.
function sortArray(arr: number[]): void {
  let i: number = 0;
  while (i < arr.length) {
    let j: number = i + 1;
    while (j < arr.length) {
      if (arr[j] < arr[i]) {
        let temp: number = arr[i];
        arr[i] = arr[j];
        arr[j] = temp;
      }
      j = j + 1;
    }
    i = i + 1;
  }
}

// Helper: Check if array contains value.
function contains(arr: number[], value: number): boolean {
  let i: number = 0;
  while (i < arr.length) {
    if (arr[i] == value) {
      return true;
    }
    i = i + 1;
  }
  return false;
}

// Helper: Find minimum value.
function minValue(arr: number[]): number {
  if (arr.length == 0) { return 0; }
  let min: number = arr[0];
  let i: number = 1;
  while (i < arr.length) {
    if (arr[i] < min) { min = arr[i]; }
    i = i + 1;
  }
  return min;
}

// Helper: Find maximum value.
function maxValue(arr: number[]): number {
  if (arr.length == 0) { return 0; }
  let max: number = arr[0];
  let i: number = 1;
  while (i < arr.length) {
    if (arr[i] > max) { max = arr[i]; }
    i = i + 1;
  }
  return max;
}
