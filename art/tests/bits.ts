import { and, or, xor, not, shiftLeft, shiftRight, isBitSet, setBit, clearBit, toggleBit, popcount, lsb, msb, reverseBits, extractBits, rotateLeft, rotateRight, mask, isPowerOf2Bits, hasMask } from "art/bits";

function testAnd(): number {
  if (and(5, 3) != 1) { return 1; }  // 101 & 011 = 001
  if (and(15, 7) != 7) { return 2; }  // 1111 & 0111 = 0111
  if (and(8, 4) != 0) { return 3; }  // 1000 & 0100 = 0000
  if (and(255, 255) != 255) { return 4; }
  return 0;
}

function testOr(): number {
  if (or(5, 3) != 7) { return 1; }  // 101 | 011 = 111
  if (or(8, 4) != 12) { return 2; }  // 1000 | 0100 = 1100
  if (or(0, 0) != 0) { return 3; }
  if (or(255, 0) != 255) { return 4; }
  return 0;
}

function testXor(): number {
  if (xor(5, 3) != 6) { return 1; }  // 101 ^ 011 = 110
  if (xor(15, 15) != 0) { return 2; }  // 1111 ^ 1111 = 0000
  if (xor(8, 4) != 12) { return 3; }  // 1000 ^ 0100 = 1100
  if (xor(255, 0) != 255) { return 4; }
  return 0;
}

function testNot(): number {
  let result: number = not(0);
  if (result != 4294967295) { return 1; }  // All 32 bits set
  result = not(1);
  if (result != 4294967294) { return 2; }  // All bits except bit 0
  return 0;
}

function testShiftLeft(): number {
  if (shiftLeft(1, 0) != 1) { return 1; }
  if (shiftLeft(1, 1) != 2) { return 2; }
  if (shiftLeft(1, 3) != 8) { return 3; }
  if (shiftLeft(5, 2) != 20) { return 4; }  // 101 << 2 = 10100 (5 * 4)
  return 0;
}

function testShiftRight(): number {
  if (shiftRight(1, 0) != 1) { return 1; }
  if (shiftRight(2, 1) != 1) { return 2; }
  if (shiftRight(8, 3) != 1) { return 3; }
  if (shiftRight(20, 2) != 5) { return 4; }  // 10100 >> 2 = 101 (20 / 4)
  return 0;
}

function testIsBitSet(): number {
  if (isBitSet(5, 0) != 1) { return 1; }  // 101, bit 0 is set
  if (isBitSet(5, 1) != 0) { return 2; }  // 101, bit 1 is not set
  if (isBitSet(5, 2) != 1) { return 3; }  // 101, bit 2 is set
  if (isBitSet(8, 3) != 1) { return 4; }  // 1000, bit 3 is set
  if (isBitSet(8, 0) != 0) { return 5; }  // 1000, bit 0 is not set
  return 0;
}

function testSetBit(): number {
  if (setBit(0, 0) != 1) { return 1; }  // 0000 -> 0001
  if (setBit(0, 2) != 4) { return 2; }  // 0000 -> 0100
  if (setBit(5, 1) != 7) { return 3; }  // 101 -> 111
  if (setBit(5, 2) != 5) { return 4; }  // 101 (already set)
  return 0;
}

function testClearBit(): number {
  if (clearBit(7, 0) != 6) { return 1; }  // 111 -> 110
  if (clearBit(7, 1) != 5) { return 2; }  // 111 -> 101
  if (clearBit(5, 0) != 5) { return 3; }  // 101 (already clear)
  if (clearBit(8, 3) != 0) { return 4; }  // 1000 -> 0000
  return 0;
}

function testToggleBit(): number {
  if (toggleBit(5, 0) != 4) { return 1; }  // 101 -> 100 (clear)
  if (toggleBit(5, 1) != 7) { return 2; }  // 101 -> 111 (set)
  if (toggleBit(5, 2) != 1) { return 3; }  // 101 -> 001 (clear)
  if (toggleBit(0, 0) != 1) { return 4; }  // 0 -> 1 (set)
  return 0;
}

function testPopcount(): number {
  if (popcount(0) != 0) { return 1; }
  if (popcount(1) != 1) { return 2; }
  if (popcount(3) != 2) { return 3; }  // 11
  if (popcount(5) != 2) { return 4; }  // 101
  if (popcount(7) != 3) { return 5; }  // 111
  if (popcount(15) != 4) { return 6; }  // 1111
  return 0;
}

function testLsb(): number {
  if (lsb(0) != -1) { return 1; }
  if (lsb(1) != 0) { return 2; }  // 0001, bit 0
  if (lsb(2) != 1) { return 3; }  // 0010, bit 1
  if (lsb(4) != 2) { return 4; }  // 0100, bit 2
  if (lsb(8) != 3) { return 5; }  // 1000, bit 3
  if (lsb(5) != 0) { return 6; }  // 0101, bit 0
  return 0;
}

function testMsb(): number {
  if (msb(0) != -1) { return 1; }
  if (msb(1) != 0) { return 2; }  // 0001, bit 0
  if (msb(2) != 1) { return 3; }  // 0010, bit 1
  if (msb(4) != 2) { return 4; }  // 0100, bit 2
  if (msb(8) != 3) { return 5; }  // 1000, bit 3
  if (msb(5) != 2) { return 6; }  // 0101, bit 2
  if (msb(7) != 2) { return 7; }  // 0111, bit 2
  return 0;
}

function testReverseBits(): number {
  if (reverseBits(0) != 0) { return 1; }
  if (reverseBits(1) != 2147483648) { return 2; }  // Single bit flipped to MSB
  let reversed: number = reverseBits(5);
  if (reversed < 1073741820 || reversed > 1073741824) { return 3; }  // Approximate check
  return 0;
}

function testExtractBits(): number {
  if (extractBits(15, 0, 2) != 3) { return 1; }  // 1111, bits 0-1 = 11
  if (extractBits(15, 1, 2) != 3) { return 2; }  // 1111, bits 1-2 = 11
  if (extractBits(5, 0, 2) != 1) { return 3; }  // 101, bits 0-1 = 01
  if (extractBits(5, 1, 2) != 2) { return 4; }  // 101, bits 1-2 = 10
  return 0;
}

function testMask(): number {
  if (mask(0) != 0) { return 1; }
  if (mask(1) != 1) { return 2; }  // 1
  if (mask(2) != 3) { return 3; }  // 11
  if (mask(3) != 7) { return 4; }  // 111
  if (mask(4) != 15) { return 5; }  // 1111
  return 0;
}

function testIsPowerOf2Bits(): number {
  if (isPowerOf2Bits(0) != 0) { return 1; }
  if (isPowerOf2Bits(1) != 1) { return 2; }
  if (isPowerOf2Bits(2) != 1) { return 3; }
  if (isPowerOf2Bits(3) != 0) { return 4; }
  if (isPowerOf2Bits(4) != 1) { return 5; }
  if (isPowerOf2Bits(5) != 0) { return 6; }
  if (isPowerOf2Bits(8) != 1) { return 7; }
  if (isPowerOf2Bits(16) != 1) { return 8; }
  return 0;
}

function testHasMask(): number {
  if (hasMask(7, 3) != 1) { return 1; }  // 111 contains 011
  if (hasMask(5, 1) != 1) { return 2; }  // 101 contains 001
  if (hasMask(5, 2) != 0) { return 3; }  // 101 does not contain 010
  if (hasMask(15, 7) != 1) { return 4; }  // 1111 contains 0111
  if (hasMask(15, 8) != 1) { return 5; }  // 1111 contains 1000
  if (hasMask(8, 4) != 0) { return 6; }  // 1000 does not contain 0100
  return 0;
}

function testBitOperationChaining(): number {
  let val: number = 0;
  val = setBit(val, 0);  // 0001
  if (val != 1) { return 1; }
  val = setBit(val, 2);  // 0101
  if (val != 5) { return 2; }
  val = clearBit(val, 0);  // 0100
  if (val != 4) { return 3; }
  val = toggleBit(val, 3);  // 1100
  if (val != 12) { return 4; }
  return 0;
}
