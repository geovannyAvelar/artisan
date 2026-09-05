// Bitwise/shift operators (& | ^ ~ << >> >>>) - real JS ToInt32
// semantics: each operand truncates to a 32-bit signed int, the operator
// runs on those bits, and the result widens back to a double.

function assertEq(actual: number, expected: number, failCount: number): number {
  if (actual != expected) { return failCount + 1; }
  return failCount;
}

function main(): number {
  let fails: number = 0;

  fails = assertEq(6 & 3, 2, fails);   // 0110 & 0011 = 0010
  fails = assertEq(6 | 3, 7, fails);   // 0110 | 0011 = 0111
  fails = assertEq(6 ^ 3, 5, fails);   // 0110 ^ 0011 = 0101
  fails = assertEq(~0, -1, fails);     // all bits set -> -1, not 4294967295
  fails = assertEq(~5, -6, fails);

  fails = assertEq(1 << 4, 16, fails);
  fails = assertEq(-8 >> 1, -4, fails);      // arithmetic: sign-preserving
  fails = assertEq(-1 >>> 28, 15, fails);    // unsigned: top 4 bits of all-1s

  // shift amount is masked to 0-31, same as real JS - a shift by 32 is a
  // shift by 0 (32 & 31 == 0), NOT a shift that clears everything.
  fails = assertEq(1 << 32, 1, fails);

  // precedence: '|'/'^'/'&' each bind tighter than '&&' but looser than
  // '==' - `1 | 2 == 3` is `1 | (2 == 3)`... but '==' needs matching
  // types (number vs boolean would be a type error), so this checks
  // precedence a different way instead: '&' binds tighter than '|'/'^',
  // matching real JS (`a | b & c` is `a | (b & c)`, not `(a | b) & c`).
  fails = assertEq(1 | 2 & 3, 3, fails); // 2 & 3 = 2 first, then 1 | 2 = 3
  fails = assertEq(4 ^ 2 & 6, 6, fails); // 2 & 6 = 2 first, then 4 ^ 2 = 6

  // shift binds tighter than relational, looser than additive
  fails = assertEq(1 << 2 + 1, 8, fails);         // 1 << (2+1) = 1 << 3 = 8
  if (!(1 << 2 > 3)) { fails = fails + 1; }        // (1 << 2) > 3 -> 4 > 3 -> true

  // combined with a variable, not just literals
  let mask: number = 255;
  let value: number = 4095;
  fails = assertEq(value & mask, 255, fails);

  return fails;
}
