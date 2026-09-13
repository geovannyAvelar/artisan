// Iterators module for ART. Import with: `import { Iterator, makeIterator, ... } from "art/iterators";`
// Provides functional iterator protocol with lazy evaluation and chainable operations.

// Iterator state: represents a single iteration step
// Returns [value, hasMore] where hasMore indicates if iteration should continue
type IteratorStep = [value: number, hasMore: boolean];

// Iterator type: a function that returns the next element and whether to continue
export type Iterator = () => IteratorStep;

// Creates an iterator from an array.
export function makeIterator(arr: number[]): Iterator {
  let index: number = 0;

  return function(): IteratorStep {
    if (index < arr.length) {
      let value: number = arr[index];
      index = index + 1;
      return [value, true];
    }
    return [0, false];
  };
}

// Creates an iterator from a range [start, end).
export function rangeIterator(start: number, end: number): Iterator {
  let current: number = start;

  return function(): IteratorStep {
    if (current < end) {
      let value: number = current;
      current = current + 1;
      return [value, true];
    }
    return [0, false];
  };
}

// Creates a filtered iterator.
export function filterIterator(iter: Iterator, predicate: (value: number) => boolean): Iterator {
  let current: IteratorStep = [0, true];
  let done: boolean = false;

  return function(): IteratorStep {
    while (current[1] && !done) {
      current = iter();
      if (current[1]) {
        if (predicate(current[0])) {
          return current;
        }
      } else {
        done = true;
        return [0, false];
      }
    }
    return [0, false];
  };
}

// Creates a mapped iterator.
export function mapIterator(iter: Iterator, fn: (value: number) => number): Iterator {
  return function(): IteratorStep {
    let step: IteratorStep = iter();
    if (step[1]) {
      return [fn(step[0]), true];
    }
    return [0, false];
  };
}

// Creates an iterator that takes only n elements.
export function takeIterator(iter: Iterator, n: number): Iterator {
  let count: number = 0;

  return function(): IteratorStep {
    if (count < n) {
      let step: IteratorStep = iter();
      if (step[1]) {
        count = count + 1;
        return step;
      }
    }
    return [0, false];
  };
}

// Creates an iterator that skips n elements.
export function skipIterator(iter: Iterator, n: number): Iterator {
  let skipped: number = 0;
  let isSkipping: boolean = true;

  return function(): IteratorStep {
    while (isSkipping && skipped < n) {
      let step: IteratorStep = iter();
      if (!step[1]) {
        return [0, false];
      }
      skipped = skipped + 1;
    }
    isSkipping = false;
    return iter();
  };
}

// Creates an iterator that cycles through another iterator multiple times.
export function cycleIterator(iter: Iterator, times: number): Iterator {
  let cycleCount: number = 0;
  let currentIter: Iterator = iter;

  return function(): IteratorStep {
    while (cycleCount < times) {
      let step: IteratorStep = currentIter();
      if (step[1]) {
        return step;
      }
      cycleCount = cycleCount + 1;
      currentIter = iter;
    }
    return [0, false];
  };
}

// Creates an iterator that applies a function as a side effect.
export function tapIterator(iter: Iterator, fn: (value: number) => void): Iterator {
  return function(): IteratorStep {
    let step: IteratorStep = iter();
    if (step[1]) {
      fn(step[0]);
    }
    return step;
  };
}

// Creates an iterator from a generator-like function.
export function generateIterator(seed: number, next: (value: number) => number, maxIterations: number): Iterator {
  let current: number = seed;
  let iterations: number = 0;

  return function(): IteratorStep {
    if (iterations < maxIterations) {
      let value: number = current;
      current = next(current);
      iterations = iterations + 1;
      return [value, true];
    }
    return [0, false];
  };
}

// Creates an iterator that repeats a single value.
export function repeatIterator(value: number, times: number): Iterator {
  let count: number = 0;

  return function(): IteratorStep {
    if (count < times) {
      count = count + 1;
      return [value, true];
    }
    return [0, false];
  };
}

// Creates an iterator that yields while a condition is true.
export function whileIterator(seed: number, condition: (value: number) => boolean, step: (value: number) => number): Iterator {
  let current: number = seed;
  let done: boolean = false;

  return function(): IteratorStep {
    if (!done && condition(current)) {
      let value: number = current;
      current = step(current);
      return [value, true];
    }
    done = true;
    return [0, false];
  };
}

// Creates an iterator that chains multiple iterators.
export function chainIterators(iterators: Iterator[]): Iterator {
  let currentIndex: number = 0;

  return function(): IteratorStep {
    while (currentIndex < iterators.length) {
      let step: IteratorStep = iterators[currentIndex]();
      if (step[1]) {
        return step;
      }
      currentIndex = currentIndex + 1;
    }
    return [0, false];
  };
}

// Collects all values from an iterator into an array.
export function collect(iter: Iterator): number[] {
  let result: number[] = [];
  let step: IteratorStep = iter();

  while (step[1]) {
    result = result + [step[0]];
    step = iter();
  }

  return result;
}

// Counts the number of elements in an iterator.
export function count(iter: Iterator): number {
  let result: number = 0;
  let step: IteratorStep = iter();

  while (step[1]) {
    result = result + 1;
    step = iter();
  }

  return result;
}

// Sums all values in an iterator.
export function sum(iter: Iterator): number {
  let result: number = 0;
  let step: IteratorStep = iter();

  while (step[1]) {
    result = result + step[0];
    step = iter();
  }

  return result;
}

// Finds the minimum value in an iterator.
export function min(iter: Iterator): number {
  let step: IteratorStep = iter();
  if (!step[1]) { return 0; }

  let result: number = step[0];
  step = iter();

  while (step[1]) {
    if (step[0] < result) {
      result = step[0];
    }
    step = iter();
  }

  return result;
}

// Finds the maximum value in an iterator.
export function max(iter: Iterator): number {
  let step: IteratorStep = iter();
  if (!step[1]) { return 0; }

  let result: number = step[0];
  step = iter();

  while (step[1]) {
    if (step[0] > result) {
      result = step[0];
    }
    step = iter();
  }

  return result;
}

// Reduces an iterator to a single value.
export function reduce(iter: Iterator, initial: number, fn: (acc: number, value: number) => number): number {
  let result: number = initial;
  let step: IteratorStep = iter();

  while (step[1]) {
    result = fn(result, step[0]);
    step = iter();
  }

  return result;
}

// Finds the first value matching a predicate.
export function findFirst(iter: Iterator, predicate: (value: number) => boolean): number {
  let step: IteratorStep = iter();

  while (step[1]) {
    if (predicate(step[0])) {
      return step[0];
    }
    step = iter();
  }

  return 0;
}

// Checks if any value matches a predicate.
export function anyMatch(iter: Iterator, predicate: (value: number) => boolean): boolean {
  let step: IteratorStep = iter();

  while (step[1]) {
    if (predicate(step[0])) {
      return true;
    }
    step = iter();
  }

  return false;
}

// Checks if all values match a predicate.
export function allMatch(iter: Iterator, predicate: (value: number) => boolean): boolean {
  let step: IteratorStep = iter();

  while (step[1]) {
    if (!predicate(step[0])) {
      return false;
    }
    step = iter();
  }

  return true;
}

// Counts values matching a predicate.
export function countMatching(iter: Iterator, predicate: (value: number) => boolean): number {
  let result: number = 0;
  let step: IteratorStep = iter();

  while (step[1]) {
    if (predicate(step[0])) {
      result = result + 1;
    }
    step = iter();
  }

  return result;
}

// Applies a function to each element (for side effects).
export function forEach(iter: Iterator, fn: (value: number) => void): void {
  let step: IteratorStep = iter();

  while (step[1]) {
    fn(step[0]);
    step = iter();
  }
}

// Creates an iterator that yields pairs of (index, value).
export function enumerate(iter: Iterator): Iterator {
  let index: number = 0;

  return function(): IteratorStep {
    let step: IteratorStep = iter();
    if (step[1]) {
      let value: number = index;
      index = index + 1;
      return [value, true];
    }
    return [0, false];
  };
}

// Creates an iterator that yields only distinct values.
export function distinct(iter: Iterator): Iterator {
  let seen: number[] = [];

  return function(): IteratorStep {
    let step: IteratorStep = iter();

    while (step[1]) {
      let value: number = step[0];
      let found: boolean = false;
      let i: number = 0;

      while (i < seen.length) {
        if (seen[i] == value) {
          found = true;
        }
        i = i + 1;
      }

      if (!found) {
        seen = seen + [value];
        return [value, true];
      }

      step = iter();
    }

    return [0, false];
  };
}

// Creates an iterator that yields (current, next) pairs.
export function pairwise(iter: Iterator): Iterator {
  let prev: IteratorStep = iter();
  let done: boolean = false;

  return function(): IteratorStep {
    if (done || !prev[1]) { return [0, false]; }

    let current: IteratorStep = iter();
    if (!current[1]) {
      done = true;
      return [0, false];
    }

    let result: number = prev[0] + current[0];
    prev = current;
    return [result, true];
  };
}

// Creates a flat iterator from an iterator of arrays.
export function flatMap(iter: Iterator, fn: (value: number) => number[]): Iterator {
  let currentIter: Iterator = makeIterator([]);
  let done: boolean = false;

  return function(): IteratorStep {
    while (true) {
      let step: IteratorStep = currentIter();
      if (step[1]) {
        return step;
      }

      let outerStep: IteratorStep = iter();
      if (!outerStep[1]) {
        done = true;
        return [0, false];
      }

      let mapped: number[] = fn(outerStep[0]);
      currentIter = makeIterator(mapped);
    }
  };
}

// Creates an iterator that batches elements.
export function batch(iter: Iterator, size: number): Iterator {
  let buffer: number[] = [];
  let done: boolean = false;

  return function(): IteratorStep {
    while (buffer.length < size && !done) {
      let step: IteratorStep = iter();
      if (step[1]) {
        buffer = buffer + [step[0]];
      } else {
        done = true;
      }
    }

    if (buffer.length > 0) {
      let value: number = buffer[0];
      buffer = buffer.slice(1, buffer.length);
      return [value, true];
    }

    return [0, false];
  };
}

// Creates an iterator with a window of size n sliding over elements.
export function window(iter: Iterator, size: number): Iterator {
  let buffer: number[] = [];
  let done: boolean = false;

  return function(): IteratorStep {
    while (buffer.length < size && !done) {
      let step: IteratorStep = iter();
      if (step[1]) {
        buffer = buffer + [step[0]];
      } else {
        done = true;
      }
    }

    if (buffer.length >= size) {
      let value: number = buffer[0];
      buffer = buffer.slice(1, buffer.length);
      return [value, true];
    }

    return [0, false];
  };
}

// Creates an iterator that intersperes a value between elements.
export function intersperse(iter: Iterator, separator: number): Iterator {
  let step: IteratorStep = iter();
  let first: boolean = true;
  let needsSeparator: boolean = false;

  return function(): IteratorStep {
    if (needsSeparator) {
      needsSeparator = false;
      return [separator, true];
    }

    if (first) {
      first = false;
      if (step[1]) {
        needsSeparator = true;
        let value: number = step[0];
        step = iter();
        return [value, true];
      }
      return [0, false];
    }

    if (step[1]) {
      needsSeparator = true;
      let value: number = step[0];
      step = iter();
      return [value, true];
    }

    return [0, false];
  };
}
