// Bitwise operations for ART.
// Import with: `import { and, or, xor, not, shiftLeft, ... } from "art/bits";`

// Bitwise AND - returns bits that are set in both operands.
export function and(a: number, b: number): number {
  let result: number = 0;
  let bit: number = 1;
  let i: number = 0;
  while (i < 32) {  // 32-bit operations
    let aBit: number = a - ((a / (bit * 2)) * (bit * 2));
    let bBit: number = b - ((b / (bit * 2)) * (bit * 2));
    if (aBit == bit && bBit == bit) {
      result = result + bit;
    }
    bit = bit * 2;
    i = i + 1;
  }
  return result;
}

// Bitwise OR - returns bits that are set in either operand.
export function or(a: number, b: number): number {
  let result: number = 0;
  let bit: number = 1;
  let i: number = 0;
  while (i < 32) {
    let aBit: number = a - ((a / (bit * 2)) * (bit * 2));
    let bBit: number = b - ((b / (bit * 2)) * (bit * 2));
    if (aBit == bit || bBit == bit) {
      result = result + bit;
    }
    bit = bit * 2;
    i = i + 1;
  }
  return result;
}

// Bitwise XOR - returns bits that differ between operands.
export function xor(a: number, b: number): number {
  let result: number = 0;
  let bit: number = 1;
  let i: number = 0;
  while (i < 32) {
    let aBit: number = a - ((a / (bit * 2)) * (bit * 2));
    let bBit: number = b - ((b / (bit * 2)) * (bit * 2));
    if ((aBit == bit && bBit != bit) || (aBit != bit && bBit == bit)) {
      result = result + bit;
    }
    bit = bit * 2;
    i = i + 1;
  }
  return result;
}

// Bitwise NOT - inverts all bits (within 32-bit range).
// Note: Returns the bitwise complement for positive numbers.
export function not(a: number): number {
  let result: number = 0;
  let bit: number = 1;
  let i: number = 0;
  while (i < 32) {
    let aBit: number = a - ((a / (bit * 2)) * (bit * 2));
    if (aBit != bit) {
      result = result + bit;
    }
    bit = bit * 2;
    i = i + 1;
  }
  return result;
}

// Left bit shift - equivalent to multiplying by 2^n.
// Shifts the bits left by n positions, filling with zeros.
export function shiftLeft(a: number, n: number): number {
  let i: number = 0;
  let result: number = a;
  while (i < n) {
    result = result * 2;
    i = i + 1;
  }
  return result;
}

// Right bit shift - equivalent to dividing by 2^n (integer division).
// Shifts the bits right by n positions.
export function shiftRight(a: number, n: number): number {
  let i: number = 0;
  let result: number = a;
  while (i < n) {
    result = result / 2;
    i = i + 1;
  }
  return result;
}

// Check if a specific bit is set (1) at position n (0-indexed from right).
// Returns 1 if set, 0 if not set.
export function isBitSet(value: number, n: number): number {
  let bit: number = 1;
  let i: number = 0;
  while (i < n) {
    bit = bit * 2;
    i = i + 1;
  }
  let extracted: number = value - ((value / (bit * 2)) * (bit * 2));
  if (extracted == bit) {
    return 1;
  }
  return 0;
}

// Set a specific bit to 1 at position n.
export function setBit(value: number, n: number): number {
  let bit: number = 1;
  let i: number = 0;
  while (i < n) {
    bit = bit * 2;
    i = i + 1;
  }
  let extracted: number = value - ((value / (bit * 2)) * (bit * 2));
  if (extracted != bit) {
    return value + bit;
  }
  return value;
}

// Clear a specific bit to 0 at position n.
export function clearBit(value: number, n: number): number {
  let bit: number = 1;
  let i: number = 0;
  while (i < n) {
    bit = bit * 2;
    i = i + 1;
  }
  let extracted: number = value - ((value / (bit * 2)) * (bit * 2));
  if (extracted == bit) {
    return value - bit;
  }
  return value;
}

// Toggle a specific bit (flip between 0 and 1) at position n.
export function toggleBit(value: number, n: number): number {
  let bit: number = 1;
  let i: number = 0;
  while (i < n) {
    bit = bit * 2;
    i = i + 1;
  }
  let extracted: number = value - ((value / (bit * 2)) * (bit * 2));
  if (extracted == bit) {
    return value - bit;
  } else {
    return value + bit;
  }
}

// Count the number of bits set to 1 (population count, Hamming weight).
export function popcount(value: number): number {
  let count: number = 0;
  let bit: number = 1;
  let i: number = 0;
  while (i < 32) {
    let extracted: number = value - ((value / (bit * 2)) * (bit * 2));
    if (extracted == bit) {
      count = count + 1;
    }
    bit = bit * 2;
    i = i + 1;
  }
  return count;
}

// Find the position of the least significant bit (rightmost 1).
// Returns -1 if the value is 0.
export function lsb(value: number): number {
  if (value == 0) { return -1; }
  let bit: number = 1;
  let i: number = 0;
  while (i < 32) {
    let extracted: number = value - ((value / (bit * 2)) * (bit * 2));
    if (extracted == bit) {
      return i;
    }
    bit = bit * 2;
    i = i + 1;
  }
  return -1;
}

// Find the position of the most significant bit (leftmost 1).
// Returns -1 if the value is 0.
export function msb(value: number): number {
  if (value == 0) { return -1; }
  let bit: number = 1;
  let i: number = 0;
  let lastPos: number = -1;
  while (i < 32) {
    let extracted: number = value - ((value / (bit * 2)) * (bit * 2));
    if (extracted == bit) {
      lastPos = i;
    }
    bit = bit * 2;
    i = i + 1;
  }
  return lastPos;
}

// Reverse the bits of a value (only first 32 bits).
export function reverseBits(value: number): number {
  let result: number = 0;
  let bit: number = 1;
  let i: number = 0;
  while (i < 32) {
    let extracted: number = value - ((value / (bit * 2)) * (bit * 2));
    if (extracted == bit) {
      let resultBit: number = 1;
      let j: number = 0;
      while (j < (31 - i)) {
        resultBit = resultBit * 2;
        j = j + 1;
      }
      result = result + resultBit;
    }
    bit = bit * 2;
    i = i + 1;
  }
  return result;
}

// Extract a range of bits from value, starting at position 'start' and spanning 'length' bits.
// Returns the extracted bits shifted right to start at bit 0.
export function extractBits(value: number, start: number, length: number): number {
  let bit: number = 1;
  let i: number = 0;
  while (i < start) {
    bit = bit * 2;
    i = i + 1;
  }

  let result: number = 0;
  let mask: number = 0;
  i = 0;
  while (i < length) {
    mask = mask + bit;
    i = i + 1;
    if (i < length) {
      bit = bit * 2;
    }
  }

  let masked: number = and(value, mask);
  i = 0;
  while (i < start) {
    masked = shiftRight(masked, 1);
    i = i + 1;
  }
  return masked;
}

// Rotate bits left by n positions.
export function rotateLeft(value: number, n: number): number {
  let msb: number = msb(value);
  if (msb == -1) { return 0; }

  let shifted: number = shiftLeft(value, n);
  let overflow: number = extractBits(value, msb - n + 1, n);
  return shifted + overflow;
}

// Rotate bits right by n positions.
export function rotateRight(value: number, n: number): number {
  let msb: number = msb(value);
  if (msb == -1) { return 0; }

  let shifted: number = shiftRight(value, n);
  let overflow: number = and(value, ((1) - ((1) / (2))));  // Extract lowest n bits
  let i: number = 0;
  let overflowShifted: number = overflow;
  while (i < (msb + 1 - n)) {
    overflowShifted = shiftLeft(overflowShifted, 1);
    i = i + 1;
  }
  return shifted + overflowShifted;
}

// Get a bitmask with n bits set to 1 (starting from bit 0).
export function mask(n: number): number {
  if (n == 0) { return 0; }
  let result: number = 0;
  let bit: number = 1;
  let i: number = 0;
  while (i < n) {
    result = result + bit;
    bit = bit * 2;
    i = i + 1;
  }
  return result;
}

// Check if a number is a power of 2 using bit operations.
// A power of 2 has only one bit set: n & (n-1) == 0
export function isPowerOf2Bits(value: number): number {
  if (value <= 0) { return 0; }
  let result: number = and(value, value - 1);
  if (result == 0) { return 1; }
  return 0;
}

// Check if all bits in mask are set in value.
export function hasMask(value: number, mask: number): number {
  let masked: number = and(value, mask);
  if (masked == mask) { return 1; }
  return 0;
}
