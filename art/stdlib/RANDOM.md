# ART Random Number Generation Library Reference

Pseudo-random number generation utilities for ART runtime. Uses a Linear Congruential Generator (LCG) for reproducible randomness. Import with:
```typescript
import { random, randomInt, randomFloat, randomBool, ... } from "art/random";
```

## Seeding and State

### setSeed(seed: number): void
Seeds the random number generator with a specific value. Use the same seed to generate reproducible sequences.
- Seed value 0 is converted to 1 (to avoid degenerate sequences)
- Same seed produces same sequence every time
```typescript
setSeed(42);
let val1: number = random();  // Always produces same value
setSeed(42);
let val2: number = random();  // Same as val1
```

### getSeed(): number
Gets the current seed value. Useful for saving/restoring random state.
```typescript
setSeed(42);
let seed: number = getSeed();  // → 42
```

## Basic Random Numbers

### random(): number
Returns a pseudo-random number in the range [0, 1). Uses Linear Congruential Generator (LCG) algorithm.
- Each call advances the internal seed state
- Suitable for generating uniform random values
```typescript
setSeed(42);
let val: number = random();   // → 0.388... (some value in [0, 1))
let val2: number = random();  // → Different value
```

### randomUniform(): number
Alias for `random()`. Returns a pseudo-random number in [0, 1) following uniform distribution.
```typescript
randomUniform()  // → Value in [0, 1)
```

## Integer and Float Ranges

### randomInt(min: number, max: number): number
Returns a pseudo-random integer in the range [min, max] (both inclusive).
- If min > max, they are automatically swapped
- If min == max, returns that value
```typescript
randomInt(1, 10)      // → Integer between 1 and 10 (inclusive)
randomInt(5, 5)       // → 5
randomInt(10, 1)      // → Integer between 1 and 10 (swapped automatically)
randomInt(-5, 5)      // → Integer between -5 and 5
```

### randomFloat(min: number, max: number): number
Returns a pseudo-random float in the range [min, max). The max value is exclusive.
- If min > max, they are automatically swapped
- If min == max, returns that value
```typescript
randomFloat(0, 1)     // → Float between 0 and 1 (exclusive)
randomFloat(10, 20)   // → Float between 10 and 20
randomFloat(-1, 1)    // → Float between -1 and 1
```

## Boolean and Choice

### randomBool(): boolean
Returns a pseudo-random boolean value. Approximately 50% chance of true, 50% chance of false.
```typescript
if (randomBool()) {
  // Execute roughly 50% of the time
}
```

### randomChoice(arr: number[]): number
Returns a random element from an array. Returns 0 if the array is empty.
```typescript
let arr: number[] = [10, 20, 30, 40, 50];
let chosen: number = randomChoice(arr);  // → One of: 10, 20, 30, 40, 50
let empty: number = randomChoice([]);    // → 0
```

### randomWeighted(weight: number): number
Returns a value (0 or 1) based on a weight in [0, 1].
- weight < weight probability of returning 1
- probability of returning 0 is 1 - weight
```typescript
randomWeighted(0.3)   // → 30% chance of 1, 70% chance of 0
randomWeighted(0.5)   // → 50% chance of 1 or 0
randomWeighted(1)     // → Always 1
randomWeighted(0)     // → Always 0
```

## Array Operations

### shuffleRandom(arr: number[]): number[]
Shuffles an array in place using Fisher-Yates algorithm with random indices. Modifies the array and returns it for chaining.
```typescript
let arr: number[] = [1, 2, 3, 4, 5];
shuffleRandom(arr);
// arr is now randomly reordered, e.g., [3, 1, 5, 2, 4]
```

### randomPermutation(n: number): number[]
Generates a random permutation of the numbers 0 to n-1. Returns a new shuffled array.
```typescript
randomPermutation(5)   // → Random arrangement of [0, 1, 2, 3, 4]
randomPermutation(3)   // → Random arrangement of [0, 1, 2]
```

### randomSample(arr: number[], k: number): number[]
Returns a random sample of size k from the array (without replacement).
- If k > array length, returns shuffled copy of entire array
- If k ≤ 0, returns empty array
```typescript
let arr: number[] = [10, 20, 30, 40, 50];
randomSample(arr, 3)   // → Random 3 elements, e.g., [30, 10, 50]
randomSample(arr, 0)   // → []
randomSample(arr, 10)  // → Shuffled copy of entire array
```

## Probability Distributions

### randomExponential(lambda: number): number
Returns a pseudo-random number following an exponential distribution.
- lambda > 0 controls the decay rate (higher = steeper decay)
- lambda ≤ 0 uses lambda = 1
- Result is always non-negative
```typescript
randomExponential(1)    // → Exponential with λ=1 (mean=1)
randomExponential(0.5)  // → Exponential with λ=0.5 (mean=2)
randomExponential(0)    // → Uses λ=1 (default)
```

### randomNormal(mean: number, stddev: number): number
Returns a pseudo-random number following a normal (Gaussian) distribution using Box-Muller transform.
- mean: the center of the distribution
- stddev: standard deviation (spread)
- Most values fall within [mean-3*stddev, mean+3*stddev]
```typescript
randomNormal(0, 1)      // → Normal distribution (mean=0, std=1)
randomNormal(100, 10)   // → Normal distribution (mean=100, std=10)
randomNormal(50, 5)     // → Most values between 35 and 65
```

## Common Patterns

### Generate Random Array
```typescript
let random_nums: number[] = [];
let i: number = 0;
while (i < 10) {
  random_nums = random_nums + [randomInt(1, 100)];
  i = i + 1;
}
// random_nums now contains 10 random integers between 1 and 100
```

### Reproducible Randomness (Deterministic)
```typescript
setSeed(12345);
let sequence1: number[] = [random(), random(), random()];

setSeed(12345);
let sequence2: number[] = [random(), random(), random()];

// sequence1 == sequence2 (same seed = same sequence)
```

### Random Lottery
```typescript
let winners: number[] = randomSample([0, 1, 2, 3, 4, 5, 6, 7, 8, 9], 3);
// Select 3 random winning numbers without duplicates
```

### Random Game Events
```typescript
if (randomBool()) {
  // 50% chance event
}

if (randomWeighted(0.2)) {
  // 20% chance event
}

let damage: number = randomInt(10, 20);
// Random damage between 10 and 20
```

### Monte Carlo Simulation
```typescript
setSeed(42);
let hits: number = 0;
let trials: number = 10000;
let i: number = 0;
while (i < trials) {
  let x: number = randomFloat(-1, 1);
  let y: number = randomFloat(-1, 1);
  if (x * x + y * y <= 1) {
    hits = hits + 1;
  }
  i = i + 1;
}
let piEstimate: number = (hits / trials) * 4;
// Estimate π ≈ 3.14...
```

## Implementation Notes

### Algorithm
Uses **Linear Congruential Generator (LCG)** algorithm:
- Formula: next_seed = (a * seed + c) mod m
- Parameters: a=1664525, c=1013904223, m=2^32
- Period: ~2^32 (4 billion values before repeating)
- Fast and deterministic

### Reproducibility
- Set seed with `setSeed()` to get reproducible sequences
- Same seed always produces same sequence
- Default seed is 1 if not explicitly set
- Useful for testing, debugging, and reproducible simulations

### Distribution Functions
- **Normal**: Uses Box-Muller transform (two uniform → one normal)
- **Exponential**: Uses inverse transform sampling
- Accuracy depends on underlying LCG quality

### Performance
- `random()`: O(1) - very fast
- `randomInt()`: O(1)
- `randomFloat()`: O(1)
- `randomBool()`: O(1)
- `shuffleRandom()`: O(n) - linear in array length
- `randomNormal()`: O(1) - constant time approximation

### Limitations
- LCG has lower quality randomness than cryptographic generators
- Not suitable for cryptography or security-sensitive applications
- Works best for games, simulations, and general applications
- Period is finite (~2^32) before sequence repeats

### Seeding Advice
- For testing: use `setSeed(42)` or any fixed value for reproducibility
- For variety: seed with system time or initialization timestamp
- Default seed (1) works fine if only one sequence is needed
