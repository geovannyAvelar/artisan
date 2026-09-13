# ART Statistics Library Reference

Statistical analysis utilities for data analysis. Import with:
```typescript
import { mean, variance, standardDeviation, percentile, ... } from "art/stats";
```

Extends the Math module with comprehensive statistical functions for data analysis, distribution analysis, and correlation measurement.

## Central Tendency

### mean(arr: number[]): number
Calculates the arithmetic mean (average) of an array.
- Returns 0 for empty arrays
- Time complexity: O(n)
```typescript
mean([1, 2, 3, 4, 5])        // → 3
mean([10, 20, 30])           // → 20
mean([])                     // → 0
```

### median(arr: number[]): number
Calculates the median (middle value) of an array.
- For even-length arrays, returns lower middle value
- Modifies the array (sorts it in place)
- Time complexity: O(n log n) due to sorting
```typescript
let arr: number[] = [1, 2, 3, 4, 5];
median(arr)                  // → 3

let arr2: number[] = [1, 2, 3, 4];
median(arr2)                 // → 2 (lower middle value)
```

### mode(arr: number[]): number
Calculates the mode (most frequently occurring value).
- Returns 0 if array is empty
- If multiple modes exist, returns the first one encountered
- Time complexity: O(n²)
```typescript
mode([1, 2, 2, 3, 3, 3, 4])    // → 3
mode([1, 2, 3, 4, 5])          // → 1 (all have equal frequency)
```

## Dispersion and Spread

### range(arr: number[]): number
Calculates the range (max - min) of an array.
- Simplest measure of spread
- Returns 0 for empty arrays
```typescript
range([1, 2, 3, 4, 5])       // → 4
range([10, 5, 20])           // → 15
range([5])                   // → 0
```

### variance(arr: number[]): number
Calculates the population variance (average squared deviation from mean).
- Measures how spread out the data is
- Returns 0 for arrays with ≤1 element
- Time complexity: O(n)
```typescript
variance([1, 2, 3, 4, 5])    // → 2.0
variance([5, 5, 5])          // → 0 (no spread)
```

### sampleVariance(arr: number[]): number
Calculates the sample variance (dividing by n-1 instead of n).
- Use when array represents a sample of larger population
- Provides unbiased estimate
```typescript
sampleVariance([1, 2, 3, 4, 5])    // → 2.5 (larger than variance)
```

### standardDeviation(arr: number[]): number
Calculates the population standard deviation (square root of variance).
- Most commonly used measure of spread
- Same units as original data
```typescript
standardDeviation([1, 2, 3, 4, 5])    // → ~1.414
standardDeviation([5, 5, 5])          // → 0
```

### sampleStandardDeviation(arr: number[]): number
Calculates the sample standard deviation (square root of sample variance).
- Use for sample data
```typescript
sampleStandardDeviation([1, 2, 3, 4, 5])    // → ~1.581
```

### coefficientOfVariation(arr: number[]): number
Calculates the coefficient of variation (CV) as percentage.
- Formula: (stddev / mean) * 100
- Standardized measure of dispersion
- Returns 0 if mean is 0
```typescript
coefficientOfVariation([1, 2, 3, 4, 5])    // → ~47%
coefficientOfVariation([100, 200, 300])    // → ~47% (same as above)
```

### meanAbsoluteDeviation(arr: number[]): number
Calculates the average absolute deviation from mean.
- Less sensitive to outliers than standard deviation
```typescript
meanAbsoluteDeviation([1, 2, 3, 4, 5])    // → 1.2
```

## Quantiles and Percentiles

### percentile(arr: number[], p: number): number
Calculates the pth percentile of an array (p in 0-100).
- p=0 returns minimum, p=100 returns maximum, p=50 returns median
- Modifies the array (sorts it in place)
```typescript
let arr: number[] = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];
percentile(arr, 0)     // → 1 (minimum)
percentile(arr, 25)    // → ~3.25 (first quartile)
percentile(arr, 50)    // → ~5.5 (median)
percentile(arr, 75)    // → ~7.75 (third quartile)
percentile(arr, 100)   // → 10 (maximum)
```

### q1(arr: number[]): number
Calculates the first quartile (25th percentile).
- Modifies the array (sorts it in place)
```typescript
q1([1, 2, 3, 4, 5, 6, 7, 8, 9, 10])    // → ~3.25
```

### q3(arr: number[]): number
Calculates the third quartile (75th percentile).
- Modifies the array (sorts it in place)
```typescript
q3([1, 2, 3, 4, 5, 6, 7, 8, 9, 10])    // → ~7.75
```

### iqr(arr: number[]): number
Calculates the interquartile range (Q3 - Q1).
- Contains the middle 50% of data
- Robust measure of spread (less affected by outliers)
- Modifies the array (sorts it in place)
```typescript
iqr([1, 2, 3, 4, 5, 6, 7, 8, 9, 10])    // → ~4.5
```

## Distribution Shape

### skewness(arr: number[]): number
Calculates the skewness (asymmetry) of the distribution.
- Positive = right-skewed (tail on right)
- Negative = left-skewed (tail on left)
- Zero = symmetric
```typescript
skewness([1, 2, 3, 4, 5])              // → ~0 (symmetric)
skewness([1, 1, 1, 2, 3, 10])          // → Positive (right-skewed)
```

### kurtosis(arr: number[]): number
Calculates the excess kurtosis (peakedness) of the distribution.
- Positive = sharp peak with heavy tails
- Negative = flat distribution
- Zero = normal distribution
```typescript
kurtosis([1, 2, 3, 4, 5])              // → Slightly negative (uniform)
```

## Standardization and Scaling

### zScores(arr: number[]): number[]
Calculates z-scores for all elements (standardization).
- z-score = (value - mean) / stddev
- Useful for comparing values on different scales
- Returns array of z-scores
```typescript
let scores: number[] = zScores([60, 70, 80, 90, 100]);
// Each value converted to how many standard deviations from mean
```

## Relationships Between Variables

### covariance(arr1: number[], arr2: number[]): number
Calculates covariance between two arrays of same length.
- Positive = variables move together
- Negative = variables move opposite
- Zero = independent
- Arrays must have same length
```typescript
covariance([1, 2, 3, 4, 5], [2, 4, 6, 8, 10])    // → 2.0 (positive)
```

### correlation(arr1: number[], arr2: number[]): number
Calculates Pearson correlation coefficient between two arrays.
- Returns value between -1 and 1
- 1 = perfect positive correlation
- -1 = perfect negative correlation
- 0 = no correlation
```typescript
correlation([1, 2, 3, 4, 5], [2, 4, 6, 8, 10])    // → 1.0 (perfect)
correlation([1, 2, 3, 4, 5], [5, 4, 3, 2, 1])     // → -1.0 (inverse)
```

## Outlier Detection

### outliers(arr: number[]): number[]
Identifies outliers using the Interquartile Range (IQR) method.
- Points > Q3 + 1.5*IQR or < Q1 - 1.5*IQR are outliers
- Standard method in statistical analysis
- Modifies the array (sorts it in place)
- Returns unique outlier values
```typescript
let arr: number[] = [1, 2, 3, 4, 5, 6, 7, 8, 9, 100];
let outs: number[] = outliers(arr);    // → [100]
```

## Summary Statistics

### summary(arr: number[]): number[]
Calculates a summary of key statistics.
- Returns array: [mean, median, stddev, min, max]
- Useful for quick data overview
```typescript
let s: number[] = summary([1, 2, 3, 4, 5]);
// s[0] = 3      (mean)
// s[1] = 3      (median)
// s[2] ≈ 1.414  (stddev)
// s[3] = 1      (min)
// s[4] = 5      (max)
```

## Common Patterns

### Describe a Dataset
```typescript
let data: number[] = [10, 20, 30, 40, 50, 60, 70, 80, 90, 100];
let summary_stats: number[] = summary(data);
// Quick overview: mean, median, spread, range
```

### Detect Outliers
```typescript
let measurements: number[] = [1.0, 1.1, 1.2, 1.3, 50.0];  // 50.0 seems wrong
let outs: number[] = outliers(measurements);
if (outs.length > 0) {
  // Investigate outliers
}
```

### Normalize Data (Z-Score Normalization)
```typescript
let raw_scores: number[] = [60, 70, 80, 90, 100];
let normalized: number[] = zScores(raw_scores);
// Normalized scores centered at 0 with stddev 1
```

### Compare Two Variables
```typescript
let height: number[] = [170, 175, 180, 185, 190];
let weight: number[] = [70, 75, 80, 85, 90];
let corr: number = correlation(height, weight);
if (corr > 0.7) {
  // Strong positive relationship
}
```

### Statistical Consistency
```typescript
let group1: number[] = [90, 92, 91, 93, 92];
let group2: number[] = [50, 100, 75, 80, 95];

if (standardDeviation(group1) < standardDeviation(group2)) {
  // Group1 is more consistent
}
```

## Implementation Notes

### Performance
- **O(n)**: mean, variance, stddev, covariance, zScores
- **O(n log n)**: median, percentile, quartiles (require sorting)
- **O(n²)**: mode, correlation, outliers (multiple passes)

### Sampling vs Population
- Use regular functions (variance, stddev) for population data
- Use sample versions (sampleVariance, sampleStandardDeviation) for sample data
- Sample statistics are less biased when extrapolating to populations

### Handling Edge Cases
- Empty arrays: return 0 (to avoid errors)
- Single element: variance and stddev return 0
- All same values: stddev and cv return 0
- Division by zero: handled gracefully (returns 0)

### Array Modification
These functions modify the input array (sort in place):
- median, percentile, q1, q3, iqr, outliers

Call these with a copy if you need original order preserved:
```typescript
let original: number[] = [3, 1, 4, 1, 5, 9];
let copy: number[] = original;  // Create your own copy if needed
let med: number = median(copy);
```

### Statistical Assumptions
- Functions assume normally distributed or unimodal data
- Skewness and kurtosis assume sufficient sample size (n > 3)
- Correlation assumes linear relationship
- All functions work with any numerical values (positive, negative, decimals)
