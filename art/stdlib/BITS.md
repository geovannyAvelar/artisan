# ART Bitwise Operations Library Reference

Low-level bitwise operation utilities for ART runtime. Import with:
```typescript
import { and, or, xor, not, shiftLeft, shiftRight, ... } from "art/bits";
```

All operations work on 32-bit integer representations within the double precision number type.

## Basic Bitwise Operations

### and(a: number, b: number): number
Bitwise AND - returns bits that are set (1) in both operands.
```typescript
and(5, 3)      // → 1      (101 & 011 = 001)
and(15, 7)     // → 7      (1111 & 0111 = 0111)
and(8, 4)      // → 0      (1000 & 0100 = 0000)
and(255, 255)  // → 255
```

### or(a: number, b: number): number
Bitwise OR - returns bits that are set in either operand.
```typescript
or(5, 3)       // → 7      (101 | 011 = 111)
or(8, 4)       // → 12     (1000 | 0100 = 1100)
or(0, 0)       // → 0
or(255, 0)     // → 255
```

### xor(a: number, b: number): number
Bitwise XOR (exclusive OR) - returns bits that differ between operands.
```typescript
xor(5, 3)      // → 6      (101 ^ 011 = 110)
xor(15, 15)    // → 0      (1111 ^ 1111 = 0000)
xor(8, 4)      // → 12     (1000 ^ 0100 = 1100)
xor(255, 0)    // → 255
```

### not(a: number): number
Bitwise NOT - inverts all bits within 32-bit range.
```typescript
not(0)         // → 4294967295  (all 32 bits set)
not(1)         // → 4294967294  (all bits except bit 0)
```

## Bit Shifting

### shiftLeft(a: number, n: number): number
Left bit shift - equivalent to multiplying by 2^n. Shifts bits left by n positions, filling with zeros on the right.
```typescript
shiftLeft(1, 0)   // → 1        (no shift)
shiftLeft(1, 1)   // → 2        (1 << 1 = 2)
shiftLeft(1, 3)   // → 8        (1 << 3 = 8)
shiftLeft(5, 2)   // → 20       (101 << 2 = 10100)
```

### shiftRight(a: number, n: number): number
Right bit shift - equivalent to dividing by 2^n (integer division). Shifts bits right by n positions.
```typescript
shiftRight(1, 0)    // → 1       (no shift)
shiftRight(2, 1)    // → 1       (10 >> 1 = 1)
shiftRight(8, 3)    // → 1       (1000 >> 3 = 1)
shiftRight(20, 2)   // → 5       (10100 >> 2 = 101)
```

## Individual Bit Operations

### isBitSet(value: number, n: number): number
Check if a specific bit is set (1) at position n (0-indexed from right).
- Returns 1 if bit is set
- Returns 0 if bit is not set
```typescript
isBitSet(5, 0)    // → 1   (101, bit 0 is set)
isBitSet(5, 1)    // → 0   (101, bit 1 is not set)
isBitSet(5, 2)    // → 1   (101, bit 2 is set)
isBitSet(8, 3)    // → 1   (1000, bit 3 is set)
```

### setBit(value: number, n: number): number
Set a specific bit to 1 at position n. If already set, returns value unchanged.
```typescript
setBit(0, 0)      // → 1    (0000 -> 0001)
setBit(0, 2)      // → 4    (0000 -> 0100)
setBit(5, 1)      // → 7    (101 -> 111)
setBit(5, 2)      // → 5    (101, already set)
```

### clearBit(value: number, n: number): number
Clear a specific bit to 0 at position n. If already clear, returns value unchanged.
```typescript
clearBit(7, 0)    // → 6    (111 -> 110)
clearBit(7, 1)    // → 5    (111 -> 101)
clearBit(5, 0)    // → 5    (101, already clear)
clearBit(8, 3)    // → 0    (1000 -> 0000)
```

### toggleBit(value: number, n: number): number
Toggle a specific bit (flip between 0 and 1) at position n.
```typescript
toggleBit(5, 0)   // → 4    (101 -> 100, clear)
toggleBit(5, 1)   // → 7    (101 -> 111, set)
toggleBit(5, 2)   // → 1    (101 -> 001, clear)
toggleBit(0, 0)   // → 1    (0 -> 1, set)
```

## Bit Analysis

### popcount(value: number): number
Count the number of bits set to 1 (population count, Hamming weight).
```typescript
popcount(0)       // → 0
popcount(1)       // → 1    (0001)
popcount(3)       // → 2    (0011)
popcount(5)       // → 2    (0101)
popcount(7)       // → 3    (0111)
popcount(15)      // → 4    (1111)
```

### lsb(value: number): number
Find the position of the least significant bit (rightmost 1). Returns -1 if value is 0.
```typescript
lsb(0)            // → -1   (no bits set)
lsb(1)            // → 0    (0001, bit 0)
lsb(2)            // → 1    (0010, bit 1)
lsb(4)            // → 2    (0100, bit 2)
lsb(8)            // → 3    (1000, bit 3)
lsb(5)            // → 0    (0101, bit 0)
```

### msb(value: number): number
Find the position of the most significant bit (leftmost 1). Returns -1 if value is 0.
```typescript
msb(0)            // → -1   (no bits set)
msb(1)            // → 0    (0001, bit 0)
msb(2)            // → 1    (0010, bit 1)
msb(4)            // → 2    (0100, bit 2)
msb(8)            // → 3    (1000, bit 3)
msb(5)            // → 2    (0101, bit 2 is highest)
msb(7)            // → 2    (0111, bit 2 is highest)
```

## Bit Manipulation

### reverseBits(value: number): number
Reverse the bits of a value (within 32-bit range). The least significant bit becomes the most significant.
```typescript
reverseBits(0)    // → 0
reverseBits(1)    // → 2147483648   (bit 0 -> bit 31)
```

### extractBits(value: number, start: number, length: number): number
Extract a range of bits from value, starting at position 'start' and spanning 'length' bits. Returns the extracted bits shifted right to start at bit 0.
```typescript
extractBits(15, 0, 2)   // → 3   (1111, bits 0-1 = 11)
extractBits(15, 1, 2)   // → 3   (1111, bits 1-2 = 11)
extractBits(5, 0, 2)    // → 1   (101, bits 0-1 = 01)
extractBits(5, 1, 2)    // → 2   (101, bits 1-2 = 10)
```

### rotateLeft(value: number, n: number): number
Rotate bits left by n positions. Bits that fall off the left wrap around to the right.
```typescript
// Similar to shift left, but with wraparound
```

### rotateRight(value: number, n: number): number
Rotate bits right by n positions. Bits that fall off the right wrap around to the left.
```typescript
// Similar to shift right, but with wraparound
```

## Utility Functions

### mask(n: number): number
Get a bitmask with n bits set to 1 (starting from bit 0).
```typescript
mask(0)   // → 0      (empty)
mask(1)   // → 1      (0001)
mask(2)   // → 3      (0011)
mask(3)   // → 7      (0111)
mask(4)   // → 15     (1111)
```

### isPowerOf2Bits(value: number): number
Check if a number is a power of 2 using bit operations. A power of 2 has only one bit set.
- Returns 1 if value is a power of 2
- Returns 0 otherwise
```typescript
isPowerOf2Bits(0)    // → 0
isPowerOf2Bits(1)    // → 1
isPowerOf2Bits(2)    // → 1
isPowerOf2Bits(3)    // → 0
isPowerOf2Bits(4)    // → 1
isPowerOf2Bits(8)    // → 1
```

### hasMask(value: number, mask: number): number
Check if all bits in mask are set in value.
- Returns 1 if all mask bits are set in value
- Returns 0 otherwise
```typescript
hasMask(7, 3)     // → 1   (111 contains 011)
hasMask(5, 1)     // → 1   (101 contains 001)
hasMask(5, 2)     // → 0   (101 does not contain 010)
hasMask(15, 7)    // → 1   (1111 contains 0111)
hasMask(8, 4)     // → 0   (1000 does not contain 0100)
```

## Common Patterns

### Check Multiple Flags
```typescript
let flags: number = 5;     // 0101
if (hasMask(flags, 1)) {
  // Bit 0 is set
}
if (hasMask(flags, 4)) {
  // Bit 2 is set
}
```

### Combine Flags
```typescript
let options: number = 0;
options = setBit(options, 0);   // Enable option 1
options = setBit(options, 2);   // Enable option 3
options = clearBit(options, 1);  // Disable option 2
```

### Count Set Bits
```typescript
let bits: number = 7;  // 111
let count: number = popcount(bits);  // → 3
```

### Find Highest Bit
```typescript
let value: number = 12;  // 1100
let highest: number = msb(value);  // → 3 (bit 3 is highest)
```

### Extract Range of Bits
```typescript
let value: number = 127;  // 1111111
let middle: number = extractBits(value, 2, 3);  // Extract bits 2-4
```

## Implementation Notes

### Performance Characteristics
- **Fast** (O(1)): isBitSet, setBit, clearBit, toggleBit, mask, hasMask
- **Linear** (O(log n)): shiftLeft, shiftRight (where n is shift amount)
- **Linear** (O(32)): and, or, xor, not, popcount, lsb, msb, reverseBits, extractBits, isPowerOf2Bits

### Bit Width
All operations work within a 32-bit integer space, though stored as IEEE 754 double precision numbers. Higher bits are preserved but not directly used in most operations.

### Negative Numbers
Negative numbers use two's complement representation within the 32-bit range. Most bitwise operations work with the bit representation as-is.

### Common Use Cases
- **Flag combinations**: Use setBit/clearBit/hasMask to manage feature flags
- **Compression**: Use bit shifting to pack multiple small values
- **Permissions**: Use bitwise AND to check if user has required permissions
- **Graphics**: Use bit operations for color channels (RGB)
- **Optimization**: Use power-of-2 checks with isPowerOf2Bits instead of math operations
