import { mean, median, mode, range, variance, standardDeviation, coefficientOfVariation, q1, q3, iqr, percentile, skewness, kurtosis, meanAbsoluteDeviation, zScores, covariance, correlation, outliers, summary, sampleVariance, sampleStandardDeviation } from "art/stats";

function testMean(): number {
  if (mean([1, 2, 3, 4, 5]) != 3) { return 1; }
  if (mean([10]) != 10) { return 2; }
  if (mean([]) != 0) { return 3; }
  if (mean([0, 0, 0]) != 0) { return 4; }
  return 0;
}

function testMedian(): number {
  let arr1: number[] = [1, 2, 3, 4, 5];
  if (median(arr1) != 3) { return 1; }

  let arr2: number[] = [1, 2, 3, 4];
  if (median(arr2) != 2) { return 2; }

  if (median([]) != 0) { return 3; }
  return 0;
}

function testMode(): number {
  let arr: number[] = [1, 2, 2, 3, 3, 3, 4];
  if (mode(arr) != 3) { return 1; }

  let arr2: number[] = [1, 1, 2, 2, 3];
  let m: number = mode(arr2);
  if (m != 1 && m != 2) { return 2; }  // First mode

  if (mode([]) != 0) { return 3; }
  return 0;
}

function testRange(): number {
  if (range([1, 2, 3, 4, 5]) != 4) { return 1; }
  if (range([10, 5, 20]) != 15) { return 2; }
  if (range([5]) != 0) { return 3; }
  if (range([]) != 0) { return 4; }
  return 0;
}

function testVariance(): number {
  let arr: number[] = [1, 2, 3, 4, 5];
  let v: number = variance(arr);
  if (v < 1.9 || v > 2.1) { return 1; }  // Should be 2.0

  if (variance([5, 5, 5]) != 0) { return 2; }
  if (variance([]) != 0) { return 3; }
  return 0;
}

function testStandardDeviation(): number {
  let arr: number[] = [1, 2, 3, 4, 5];
  let sd: number = standardDeviation(arr);
  if (sd < 1.4 || sd > 1.5) { return 1; }  // Should be ~1.414

  if (standardDeviation([5, 5, 5]) != 0) { return 2; }
  return 0;
}

function testSampleVariance(): number {
  let arr: number[] = [1, 2, 3, 4, 5];
  let sv: number = sampleVariance(arr);
  if (sv < 2.4 || sv > 2.6) { return 1; }  // Should be 2.5

  return 0;
}

function testCoefficientOfVariation(): number {
  let arr: number[] = [1, 2, 3, 4, 5];
  let cv: number = coefficientOfVariation(arr);
  if (cv < 40 || cv > 50) { return 1; }  // Should be ~47%

  if (coefficientOfVariation([5, 5, 5]) != 0) { return 2; }
  return 0;
}

function testQuartiles(): number {
  let arr: number[] = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];
  let q1Val: number = q1(arr);
  let q3Val: number = q3(arr);

  if (q1Val < 2 || q1Val > 3) { return 1; }
  if (q3Val < 8 || q3Val > 9) { return 2; }

  return 0;
}

function testPercentile(): number {
  let arr: number[] = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];

  let p0: number = percentile(arr, 0);
  if (p0 != 1) { return 1; }

  let p100: number = percentile(arr, 100);
  if (p100 != 10) { return 2; }

  let p50: number = percentile(arr, 50);  // Median
  if (p50 < 5 || p50 > 6) { return 3; }

  return 0;
}

function testSkewness(): number {
  let symmetric: number[] = [1, 2, 3, 4, 5];
  let skew: number = skewness(symmetric);
  if (skew < -0.5 || skew > 0.5) { return 1; }  // Should be ~0

  return 0;
}

function testKurtosis(): number {
  let arr: number[] = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];
  let k: number = kurtosis(arr);
  if (k < -2 || k > 0) { return 1; }  // Uniform has negative kurtosis

  return 0;
}

function testMeanAbsoluteDeviation(): number {
  let arr: number[] = [1, 2, 3, 4, 5];
  let mad: number = meanAbsoluteDeviation(arr);
  if (mad < 1.1 || mad > 1.3) { return 1; }  // Should be 1.2

  return 0;
}

function testZScores(): number {
  let arr: number[] = [1, 2, 3, 4, 5];
  let z: number[] = zScores(arr);

  if (z.length != 5) { return 1; }
  // Mean of z-scores should be ~0
  let sum: number = 0;
  let i: number = 0;
  while (i < z.length) {
    sum = sum + z[i];
    i = i + 1;
  }
  if (sum < -0.1 || sum > 0.1) { return 2; }

  return 0;
}

function testCovariance(): number {
  let arr1: number[] = [1, 2, 3, 4, 5];
  let arr2: number[] = [2, 4, 6, 8, 10];
  let cov: number = covariance(arr1, arr2);
  if (cov < 1.9 || cov > 2.1) { return 1; }  // Should be 2.0

  return 0;
}

function testCorrelation(): number {
  let arr1: number[] = [1, 2, 3, 4, 5];
  let arr2: number[] = [2, 4, 6, 8, 10];
  let corr: number = correlation(arr1, arr2);
  if (corr < 0.99) { return 1; }  // Should be 1.0 (perfect correlation)

  return 0;
}

function testOutliers(): number {
  let arr: number[] = [1, 2, 3, 4, 5, 6, 7, 8, 9, 100];
  let outs: number[] = outliers(arr);

  if (outs.length == 0) { return 1; }  // 100 should be an outlier

  return 0;
}

function testSummary(): number {
  let arr: number[] = [1, 2, 3, 4, 5];
  let s: number[] = summary(arr);

  if (s.length != 5) { return 1; }
  if (s[0] != 3) { return 2; }  // Mean
  if (s[3] != 1) { return 3; }  // Min
  if (s[4] != 5) { return 4; }  // Max

  return 0;
}

function testStatisticsWithNegatives(): number {
  let arr: number[] = [-5, -3, -1, 1, 3, 5];
  if (mean(arr) != 0) { return 1; }
  if (median(arr) < -0.1 || median(arr) > 0.1) { return 2; }
  return 0;
}

function testStatisticsWithDuplicates(): number {
  let arr: number[] = [1, 1, 1, 2, 2, 3];
  if (mean(arr) < 1.6 || mean(arr) > 1.7) { return 1; }
  if (mode(arr) != 1) { return 2; }
  return 0;
}
