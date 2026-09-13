// Functional programming utilities for ART. Import with: `import { compose, pipe, curry, ... } from "art/functional";`
// Provides utilities for functional programming patterns.

// Composes functions: compose(f, g)(x) = f(g(x))
export function compose<T, U, V>(f: (x: U) => V, g: (x: T) => U): (x: T) => V {
  return function(x: T): V {
    let intermediate: U = g(x);
    return f(intermediate);
  };
}

// Pipes value through functions left-to-right: pipe(x, f, g) = g(f(x))
export function pipe<T>(value: T, f1: (x: T) => T, f2: (x: T) => T): T {
  let step1: T = f1(value);
  let step2: T = f2(step1);
  return step2;
}

// Identity function: returns its input unchanged.
export function identity<T>(x: T): T {
  return x;
}

// Constant function: always returns the same value.
export function constant<T>(value: T): (x: T) => T {
  return function(x: T): T {
    return value;
  };
}

// Negates a boolean predicate.
export function negate(predicate: (x: number) => boolean): (x: number) => boolean {
  return function(x: number): boolean {
    return !predicate(x);
  };
}

// Combines two predicates with AND logic.
export function and(p1: (x: number) => boolean, p2: (x: number) => boolean): (x: number) => boolean {
  return function(x: number): boolean {
    return p1(x) && p2(x);
  };
}

// Combines two predicates with OR logic.
export function or(p1: (x: number) => boolean, p2: (x: number) => boolean): (x: number) => boolean {
  return function(x: number): boolean {
    return p1(x) || p2(x);
  };
}

// Applies a function multiple times.
export function times<T>(fn: (x: T) => T, n: number, value: T): T {
  let result: T = value;
  let i: number = 0;
  while (i < n) {
    result = fn(result);
    i = i + 1;
  }
  return result;
}

// Applies a function to itself multiple times (self-application).
export function repeatedly<T>(fn: (x: T) => T, n: number, initial: T): T[] {
  let result: T[] = [];
  let current: T = initial;
  let i: number = 0;

  while (i < n) {
    result = result + [current];
    current = fn(current);
    i = i + 1;
  }

  return result;
}

// Creates a function that calls fn at most once.
export function once<T>(fn: (x: T) => T): (x: T) => T {
  let called: boolean = false;
  let result: T = 0;

  return function(x: T): T {
    if (!called) {
      called = true;
      result = fn(x);
    }
    return result;
  };
}

// Partial application: fixes first argument of a function.
export function partial<T, U>(fn: (a: T, b: U) => T, arg1: T): (b: U) => T {
  return function(arg2: U): T {
    return fn(arg1, arg2);
  };
}

// Reverses argument order in a two-argument function (flip).
export function flip<T, U, V>(fn: (a: T, b: U) => V): (b: U, a: T) => V {
  return function(arg2: U, arg1: T): V {
    return fn(arg1, arg2);
  };
}

// Logical NOT for numbers (0=false, non-zero=true).
export function not(x: number): number {
  return x == 0 ? 1 : 0;
}

// Logical AND for numbers.
export function logicalAnd(a: number, b: number): number {
  return (a != 0 && b != 0) ? 1 : 0;
}

// Logical OR for numbers.
export function logicalOr(a: number, b: number): number {
  return (a != 0 || b != 0) ? 1 : 0;
}

// Logical XOR for numbers.
export function logicalXor(a: number, b: number): number {
  return ((a != 0) && (b == 0)) || ((a == 0) && (b != 0)) ? 1 : 0;
}

// Returns the result of applying fn to value, wrapped in another function for chaining.
export function chain<T>(value: T, fn: (x: T) => T): (f: (x: T) => T) => T {
  let result: T = fn(value);
  return function(nextFn: (x: T) => T): T {
    return nextFn(result);
  };
}

// Memoizes a function (caches result for a single input).
export function memoize(fn: (x: number) => number): (x: number) => number {
  let cached: boolean = false;
  let cachedArg: number = 0;
  let cachedResult: number = 0;

  return function(x: number): number {
    if (cached && cachedArg == x) {
      return cachedResult;
    }
    let result: number = fn(x);
    cachedArg = x;
    cachedResult = result;
    cached = true;
    return result;
  };
}

// Applies function to both elements and returns result.
export function both<T>(fn: (x: T) => T, a: T, b: T): T {
  let result1: T = fn(a);
  let result2: T = fn(b);
  return result2;  // Returns second result
}

// Returns function that always returns the given value.
export function always<T>(value: T): (x: T) => T {
  return function(x: T): T {
    return value;
  };
}

// Curries a two-argument function.
export function curried(f: (a: number, b: number) => number): (a: number) => (b: number) => number {
  return function(a: number): (b: number) => number {
    return function(b: number): number {
      return f(a, b);
    };
  };
}
