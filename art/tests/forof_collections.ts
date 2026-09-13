import { createSet, setAdd, setValues, setForEach, createMap, mapSet, mapKeys, mapValues, mapEntries, mapForEach } from "art/collections";

// Test for...of with Set iteration via setValues()
function testForOfSetValues(): number {
  let set: number = createSet();
  setAdd(set, 1);
  setAdd(set, 2);
  setAdd(set, 3);

  let sum: number = 0;
  let values: number[] = setValues(set);
  for (let v of values) {
    sum = sum + v;
  }

  if (sum != 6) { return 1; }
  return 0;
}

// Test for...of with Map values via mapValues()
function testForOfMapValues(): number {
  let map: number = createMap();
  mapSet(map, "a", 10);
  mapSet(map, "b", 20);
  mapSet(map, "c", 30);

  let sum: number = 0;
  let values: number[] = mapValues(map);
  for (let v of values) {
    sum = sum + v;
  }

  if (sum != 60) { return 1; }
  return 0;
}

// Test for...of with Map keys via mapKeys()
function testForOfMapKeys(): number {
  let map: number = createMap();
  mapSet(map, "key1", 1);
  mapSet(map, "key2", 2);

  let count: number = 0;
  let keys: string[] = mapKeys(map);
  for (let k of keys) {
    if (k.length > 0) { count = count + 1; }
  }

  if (count != 2) { return 1; }
  return 0;
}

// Test for...of with break on Set
function testForOfSetWithBreak(): number {
  let set: number = createSet();
  setAdd(set, 10);
  setAdd(set, 20);
  setAdd(set, 30);
  setAdd(set, 40);

  let found: number = 0;
  let values: number[] = setValues(set);
  for (let v of values) {
    if (v == 30) {
      found = v;
      break;
    }
  }

  if (found != 30) { return 1; }
  return 0;
}

// Test for...of with continue on Map
function testForOfMapWithContinue(): number {
  let map: number = createMap();
  mapSet(map, "a", 1);
  mapSet(map, "b", 2);
  mapSet(map, "c", 3);
  mapSet(map, "d", 4);

  let sum: number = 0;
  let values: number[] = mapValues(map);
  for (let v of values) {
    if (v % 2 == 0) { continue; }
    sum = sum + v;
  }

  if (sum != 4) { return 1; }
  return 0;
}

// Test nested for...of loops with collections
function testNestedForOfCollections(): number {
  let map1: number = createMap();
  mapSet(map1, "x", 1);
  mapSet(map1, "y", 2);

  let map2: number = createMap();
  mapSet(map2, "a", 10);
  mapSet(map2, "b", 20);

  let sum: number = 0;
  let values1: number[] = mapValues(map1);
  for (let v1 of values1) {
    let values2: number[] = mapValues(map2);
    for (let v2 of values2) {
      sum = sum + v1 + v2;
    }
  }

  if (sum != 120) { return 1; }
  return 0;
}

// Test for...of with filtered Set
function testForOfFilteredSet(): number {
  let set: number = createSet();
  setAdd(set, 1);
  setAdd(set, 2);
  setAdd(set, 3);
  setAdd(set, 4);
  setAdd(set, 5);

  let evenSum: number = 0;
  let values: number[] = setValues(set);
  for (let v of values) {
    if (v % 2 == 0) {
      evenSum = evenSum + v;
    }
  }

  if (evenSum != 6) { return 1; }
  return 0;
}

// Test for...of with Map entries via mapEntries()
function testForOfMapEntries(): number {
  let map: number = createMap();
  mapSet(map, "a", 1);
  mapSet(map, "b", 2);
  mapSet(map, "c", 3);

  let sum: number = 0;
  let entries: [key: string, value: number][] = mapEntries(map);
  for (let entry of entries) {
    sum = sum + entry[1];
  }

  if (sum != 6) { return 1; }
  return 0;
}

// Test for...of with Set and forEach callback
function testForOfSetIteration(): number {
  let set: number = createSet();
  setAdd(set, 5);
  setAdd(set, 10);
  setAdd(set, 15);

  let count: number = 0;
  let values: number[] = setValues(set);
  for (let v of values) {
    count = count + 1;
  }

  if (count != 3) { return 1; }
  return 0;
}

// Test for...of with Map and multiple operations
function testForOfMapComplex(): number {
  let map: number = createMap();
  let i: number = 0;
  while (i < 5) {
    let key: string = "k" + (i as string);
    mapSet(map, key, i * 10);
    i = i + 1;
  }

  let sum: number = 0;
  let values: number[] = mapValues(map);
  for (let v of values) {
    sum = sum + v;
  }

  if (sum != 100) { return 1; }
  return 0;
}
