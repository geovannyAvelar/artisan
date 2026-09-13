import { compose, identity, negate, and, or, times, once, flip, logicalAnd, logicalOr, memoize } from "art/functional";

function testIdentity(): number {
  if (identity(5) != 5) { return 1; }
  if (identity(-10) != -10) { return 2; }
  return 0;
}

function testNegate(): number {
  let isEven: (x: number) => boolean = function(x: number): boolean {
    return x - ((x / 2) * 2) == 0;
  };
  let isOdd: (x: number) => boolean = negate::<number>(isEven);
  if (!isOdd(3)) { return 1; }
  if (isOdd(2)) { return 2; }
  return 0;
}

function testAnd(): number {
  let isPositive: (x: number) => boolean = function(x: number): boolean { return x > 0; };
  let isEven: (x: number) => boolean = function(x: number): boolean { return x - ((x / 2) * 2) == 0; };
  let isPositiveEven: (x: number) => boolean = and::<number>(isPositive, isEven);
  if (!isPositiveEven(4)) { return 1; }
  if (isPositiveEven(-4)) { return 2; }
  if (isPositiveEven(3)) { return 3; }
  return 0;
}

function testOr(): number {
  let isZero: (x: number) => boolean = function(x: number): boolean { return x == 0; };
  let isOne: (x: number) => boolean = function(x: number): boolean { return x == 1; };
  let isZeroOrOne: (x: number) => boolean = or::<number>(isZero, isOne);
  if (!isZeroOrOne(0)) { return 1; }
  if (!isZeroOrOne(1)) { return 2; }
  if (isZeroOrOne(2)) { return 3; }
  return 0;
}

function testTimes(): number {
  let double: (x: number) => number = function(x: number): number { return x * 2; };
  let result: number = times::<number>(double, 3, 1);
  if (result != 8) { return 1; }
  return 0;
}

function testOnce(): number {
  let counter: number = 0;
  let increment: (x: number) => number = function(x: number): number { return x + 1; };
  let onceIncrement: (x: number) => number = once::<number>(increment);

  let r1: number = onceIncrement(5);
  let r2: number = onceIncrement(10);
  if (r1 != r2) { return 1; }
  return 0;
}

function testFlip(): number {
  let subtract: (a: number, b: number) => number = function(a: number, b: number): number { return a - b; };
  let flipped: (b: number, a: number) => number = flip::<number, number, number>(subtract);

  let r1: number = subtract(10, 3);
  let r2: number = flipped(3, 10);
  if (r1 != r2) { return 1; }
  return 0;
}

function testLogicalAnd(): number {
  if (logicalAnd(1, 1) != 1) { return 1; }
  if (logicalAnd(1, 0) != 0) { return 2; }
  if (logicalAnd(0, 0) != 0) { return 3; }
  return 0;
}

function testLogicalOr(): number {
  if (logicalOr(1, 1) != 1) { return 1; }
  if (logicalOr(1, 0) != 1) { return 2; }
  if (logicalOr(0, 0) != 0) { return 3; }
  return 0;
}

function testMemoize(): number {
  let callCount: number = 0;
  let fn: (x: number) => number = function(x: number): number {
    return x * 2;
  };
  let memoized: (x: number) => number = memoize(fn);

  let r1: number = memoized(5);
  let r2: number = memoized(5);
  if (r1 != r2) { return 1; }
  if (r1 != 10) { return 2; }
  return 0;
}
