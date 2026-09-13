import { Iterator, makeIterator, rangeIterator, filterIterator, mapIterator, takeIterator, skipIterator, cycleIterator, tapIterator, generateIterator, repeatIterator, whileIterator, chainIterators, collect, count, sum, min, max, reduce, findFirst, anyMatch, allMatch, countMatching, forEach, enumerate, distinct, pairwise, flatMap, batch, window, intersperse } from "art/iterators";

function testMakeIterator(): number {
  let arr: number[] = [1, 2, 3];
  let iter: Iterator = makeIterator(arr);

  let step1: [number, boolean] = iter();
  if (step1[0] != 1 || !step1[1]) { return 1; }

  let step2: [number, boolean] = iter();
  if (step2[0] != 2 || !step2[1]) { return 2; }

  let step3: [number, boolean] = iter();
  if (step3[0] != 3 || !step3[1]) { return 3; }

  let step4: [number, boolean] = iter();
  if (step4[1]) { return 4; }

  return 0;
}

function testRangeIterator(): number {
  let iter: Iterator = rangeIterator(0, 3);

  let step1: [number, boolean] = iter();
  if (step1[0] != 0 || !step1[1]) { return 1; }

  let step2: [number, boolean] = iter();
  if (step2[0] != 1 || !step2[1]) { return 2; }

  let step3: [number, boolean] = iter();
  if (step3[0] != 2 || !step3[1]) { return 3; }

  let step4: [number, boolean] = iter();
  if (step4[1]) { return 4; }

  return 0;
}

function testFilterIterator(): number {
  let arr: number[] = [1, 2, 3, 4, 5];
  let iter: Iterator = makeIterator(arr);
  let filtered: Iterator = filterIterator(iter, function(x: number): boolean {
    return x % 2 == 0;
  });

  let step1: [number, boolean] = filtered();
  if (step1[0] != 2 || !step1[1]) { return 1; }

  let step2: [number, boolean] = filtered();
  if (step2[0] != 4 || !step2[1]) { return 2; }

  let step3: [number, boolean] = filtered();
  if (step3[1]) { return 3; }

  return 0;
}

function testMapIterator(): number {
  let arr: number[] = [1, 2, 3];
  let iter: Iterator = makeIterator(arr);
  let mapped: Iterator = mapIterator(iter, function(x: number): number {
    return x * 2;
  });

  let step1: [number, boolean] = mapped();
  if (step1[0] != 2 || !step1[1]) { return 1; }

  let step2: [number, boolean] = mapped();
  if (step2[0] != 4 || !step2[1]) { return 2; }

  let step3: [number, boolean] = mapped();
  if (step3[0] != 6 || !step3[1]) { return 3; }

  return 0;
}

function testTakeIterator(): number {
  let arr: number[] = [1, 2, 3, 4, 5];
  let iter: Iterator = makeIterator(arr);
  let taken: Iterator = takeIterator(iter, 2);

  let step1: [number, boolean] = taken();
  if (step1[0] != 1 || !step1[1]) { return 1; }

  let step2: [number, boolean] = taken();
  if (step2[0] != 2 || !step2[1]) { return 2; }

  let step3: [number, boolean] = taken();
  if (step3[1]) { return 3; }

  return 0;
}

function testSkipIterator(): number {
  let arr: number[] = [1, 2, 3, 4, 5];
  let iter: Iterator = makeIterator(arr);
  let skipped: Iterator = skipIterator(iter, 2);

  let step1: [number, boolean] = skipped();
  if (step1[0] != 3 || !step1[1]) { return 1; }

  let step2: [number, boolean] = skipped();
  if (step2[0] != 4 || !step2[1]) { return 2; }

  return 0;
}

function testCollect(): number {
  let arr: number[] = [1, 2, 3];
  let iter: Iterator = makeIterator(arr);
  let result: number[] = collect(iter);

  if (result.length != 3) { return 1; }
  if (result[0] != 1) { return 2; }
  if (result[2] != 3) { return 3; }
  return 0;
}

function testCount(): number {
  let arr: number[] = [1, 2, 3, 4, 5];
  let iter: Iterator = makeIterator(arr);
  let result: number = count(iter);

  if (result != 5) { return 1; }
  return 0;
}

function testSum(): number {
  let arr: number[] = [1, 2, 3, 4, 5];
  let iter: Iterator = makeIterator(arr);
  let result: number = sum(iter);

  if (result != 15) { return 1; }
  return 0;
}

function testMin(): number {
  let arr: number[] = [5, 2, 8, 1, 9];
  let iter: Iterator = makeIterator(arr);
  let result: number = min(iter);

  if (result != 1) { return 1; }
  return 0;
}

function testMax(): number {
  let arr: number[] = [5, 2, 8, 1, 9];
  let iter: Iterator = makeIterator(arr);
  let result: number = max(iter);

  if (result != 9) { return 1; }
  return 0;
}

function testReduce(): number {
  let arr: number[] = [1, 2, 3, 4];
  let iter: Iterator = makeIterator(arr);
  let result: number = reduce(iter, 0, function(acc: number, val: number): number {
    return acc + val;
  });

  if (result != 10) { return 1; }
  return 0;
}

function testFindFirst(): number {
  let arr: number[] = [1, 2, 3, 4, 5];
  let iter: Iterator = makeIterator(arr);
  let result: number = findFirst(iter, function(x: number): boolean {
    return x > 3;
  });

  if (result != 4) { return 1; }
  return 0;
}

function testAnyMatch(): number {
  let arr: number[] = [1, 2, 3, 4, 5];
  let iter: Iterator = makeIterator(arr);
  let result: boolean = anyMatch(iter, function(x: number): boolean {
    return x == 3;
  });

  if (!result) { return 1; }
  return 0;
}

function testAllMatch(): number {
  let arr: number[] = [2, 4, 6, 8];
  let iter: Iterator = makeIterator(arr);
  let result: boolean = allMatch(iter, function(x: number): boolean {
    return x % 2 == 0;
  });

  if (!result) { return 1; }
  return 0;
}

function testCountMatching(): number {
  let arr: number[] = [1, 2, 3, 4, 5, 6];
  let iter: Iterator = makeIterator(arr);
  let result: number = countMatching(iter, function(x: number): boolean {
    return x % 2 == 0;
  });

  if (result != 3) { return 1; }
  return 0;
}

function testForEach(): number {
  let arr: number[] = [1, 2, 3];
  let iter: Iterator = makeIterator(arr);
  let sum: number = 0;

  forEach(iter, function(x: number): void {
    sum = sum + x;
  });

  if (sum != 6) { return 1; }
  return 0;
}

function testEnumerate(): number {
  let arr: number[] = [10, 20, 30];
  let iter: Iterator = makeIterator(arr);
  let enumIter: Iterator = enumerate(iter);

  let step1: [number, boolean] = enumIter();
  if (step1[0] != 0 || !step1[1]) { return 1; }

  let step2: [number, boolean] = enumIter();
  if (step2[0] != 1 || !step2[1]) { return 2; }

  return 0;
}

function testDistinct(): number {
  let arr: number[] = [1, 2, 2, 3, 3, 3, 4];
  let iter: Iterator = makeIterator(arr);
  let distinctIter: Iterator = distinct(iter);
  let result: number[] = collect(distinctIter);

  if (result.length != 4) { return 1; }
  return 0;
}

function testChainIterators(): number {
  let arr1: number[] = [1, 2];
  let arr2: number[] = [3, 4];
  let iter1: Iterator = makeIterator(arr1);
  let iter2: Iterator = makeIterator(arr2);

  let chained: Iterator = chainIterators([iter1, iter2]);
  let result: number[] = collect(chained);

  if (result.length != 4) { return 1; }
  if (result[2] != 3) { return 2; }
  return 0;
}

function testRepeatIterator(): number {
  let iter: Iterator = repeatIterator(42, 3);
  let result: number[] = collect(iter);

  if (result.length != 3) { return 1; }
  if (result[0] != 42) { return 2; }
  return 0;
}

function testGenerateIterator(): number {
  let iter: Iterator = generateIterator(1, function(x: number): number {
    return x * 2;
  }, 4);
  let result: number[] = collect(iter);

  if (result.length != 4) { return 1; }
  if (result[0] != 1) { return 2; }
  if (result[3] != 8) { return 3; }
  return 0;
}

function testChained(): number {
  let arr: number[] = [1, 2, 3, 4, 5];
  let iter: Iterator = makeIterator(arr);
  let filtered: Iterator = filterIterator(iter, function(x: number): boolean {
    return x % 2 == 0;
  });
  let mapped: Iterator = mapIterator(filtered, function(x: number): number {
    return x * 10;
  });

  let result: number[] = collect(mapped);
  if (result.length != 2) { return 1; }
  if (result[0] != 20) { return 2; }
  if (result[1] != 40) { return 3; }
  return 0;
}

function testTapIterator(): number {
  let arr: number[] = [1, 2, 3];
  let iter: Iterator = makeIterator(arr);
  let sum: number = 0;

  let tapped: Iterator = tapIterator(iter, function(x: number): void {
    sum = sum + x;
  });

  collect(tapped);
  if (sum != 6) { return 1; }
  return 0;
}

function testWhileIterator(): number {
  let iter: Iterator = whileIterator(1, function(x: number): boolean {
    return x <= 5;
  }, function(x: number): number {
    return x + 1;
  });

  let result: number[] = collect(iter);
  if (result.length != 5) { return 1; }
  return 0;
}
