import { createMap, createSet, mapSet, mapGet, mapHas, mapDelete, mapClear, mapSize, mapKeys, mapValues, mapEntries, mapForEach, setAdd, setHas, setDelete, setClear, setSize, setValues, setForEach, setIntersection, setUnion, setDifference, setEquals, setIsSubset, setIsSuperset, clearAllCollections } from "art/collections";

function testCreateMap(): number {
  let map: number = createMap();
  if (map < 0) { return 1; }
  if (mapSize(map) != 0) { return 2; }
  return 0;
}

function testCreateSet(): number {
  let set: number = createSet();
  if (set < 0) { return 1; }
  if (setSize(set) != 0) { return 2; }
  return 0;
}

function testMapSet(): number {
  let map: number = createMap();
  mapSet(map, "key1", 42);

  if (mapSize(map) != 1) { return 1; }
  return 0;
}

function testMapGet(): number {
  let map: number = createMap();
  mapSet(map, "key1", 42);

  let value: number = mapGet(map, "key1");
  if (value != 42) { return 1; }
  return 0;
}

function testMapGetNotFound(): number {
  let map: number = createMap();
  let value: number = mapGet(map, "nonexistent");
  if (value != 0) { return 1; }
  return 0;
}

function testMapHas(): number {
  let map: number = createMap();
  mapSet(map, "key1", 42);

  if (!mapHas(map, "key1")) { return 1; }
  if (mapHas(map, "key2")) { return 2; }
  return 0;
}

function testMapDelete(): number {
  let map: number = createMap();
  mapSet(map, "key1", 42);

  let deleted: boolean = mapDelete(map, "key1");
  if (!deleted) { return 1; }
  if (mapSize(map) != 0) { return 2; }
  if (mapHas(map, "key1")) { return 3; }
  return 0;
}

function testMapDeleteNotFound(): number {
  let map: number = createMap();
  let deleted: boolean = mapDelete(map, "nonexistent");
  if (deleted) { return 1; }
  return 0;
}

function testMapClear(): number {
  let map: number = createMap();
  mapSet(map, "key1", 42);
  mapSet(map, "key2", 99);

  mapClear(map);
  if (mapSize(map) != 0) { return 1; }
  return 0;
}

function testMapSize(): number {
  let map: number = createMap();
  if (mapSize(map) != 0) { return 1; }

  mapSet(map, "key1", 42);
  if (mapSize(map) != 1) { return 2; }

  mapSet(map, "key2", 99);
  if (mapSize(map) != 2) { return 3; }
  return 0;
}

function testMapKeys(): number {
  let map: number = createMap();
  mapSet(map, "key1", 42);
  mapSet(map, "key2", 99);

  let keys: string[] = mapKeys(map);
  if (keys.length != 2) { return 1; }
  return 0;
}

function testMapValues(): number {
  let map: number = createMap();
  mapSet(map, "key1", 42);
  mapSet(map, "key2", 99);

  let values: number[] = mapValues(map);
  if (values.length != 2) { return 1; }
  return 0;
}

function testMapEntries(): number {
  let map: number = createMap();
  mapSet(map, "key1", 42);
  mapSet(map, "key2", 99);

  let entries: [key: string, value: number][] = mapEntries(map);
  if (entries.length != 2) { return 1; }
  return 0;
}

function testMapForEach(): number {
  let map: number = createMap();
  mapSet(map, "key1", 42);
  mapSet(map, "key2", 99);

  let count: number = 0;
  let sum: number = 0;

  mapForEach(map, function(key: string, value: number): void {
    count = count + 1;
    sum = sum + value;
  });

  if (count != 2) { return 1; }
  if (sum != 141) { return 2; }
  return 0;
}

function testMapUpdate(): number {
  let map: number = createMap();
  mapSet(map, "key1", 42);
  mapSet(map, "key1", 99);

  if (mapSize(map) != 1) { return 1; }
  if (mapGet(map, "key1") != 99) { return 2; }
  return 0;
}

function testSetAdd(): number {
  let set: number = createSet();
  setAdd(set, 42);

  if (setSize(set) != 1) { return 1; }
  return 0;
}

function testSetHas(): number {
  let set: number = createSet();
  setAdd(set, 42);

  if (!setHas(set, 42)) { return 1; }
  if (setHas(set, 99)) { return 2; }
  return 0;
}

function testSetDelete(): number {
  let set: number = createSet();
  setAdd(set, 42);

  let deleted: boolean = setDelete(set, 42);
  if (!deleted) { return 1; }
  if (setSize(set) != 0) { return 2; }
  if (setHas(set, 42)) { return 3; }
  return 0;
}

function testSetDeleteNotFound(): number {
  let set: number = createSet();
  let deleted: boolean = setDelete(set, 42);
  if (deleted) { return 1; }
  return 0;
}

function testSetClear(): number {
  let set: number = createSet();
  setAdd(set, 42);
  setAdd(set, 99);

  setClear(set);
  if (setSize(set) != 0) { return 1; }
  return 0;
}

function testSetSize(): number {
  let set: number = createSet();
  if (setSize(set) != 0) { return 1; }

  setAdd(set, 42);
  if (setSize(set) != 1) { return 2; }

  setAdd(set, 99);
  if (setSize(set) != 2) { return 3; }
  return 0;
}

function testSetValues(): number {
  let set: number = createSet();
  setAdd(set, 42);
  setAdd(set, 99);

  let values: number[] = setValues(set);
  if (values.length != 2) { return 1; }
  return 0;
}

function testSetForEach(): number {
  let set: number = createSet();
  setAdd(set, 42);
  setAdd(set, 99);

  let count: number = 0;
  let sum: number = 0;

  setForEach(set, function(value: number): void {
    count = count + 1;
    sum = sum + value;
  });

  if (count != 2) { return 1; }
  if (sum != 141) { return 2; }
  return 0;
}

function testSetAddDuplicate(): number {
  let set: number = createSet();
  setAdd(set, 42);
  setAdd(set, 42);

  if (setSize(set) != 1) { return 1; }
  return 0;
}

function testSetUnion(): number {
  let set1: number = createSet();
  setAdd(set1, 1);
  setAdd(set1, 2);

  let set2: number = createSet();
  setAdd(set2, 2);
  setAdd(set2, 3);

  let union: number = setUnion(set1, set2);
  if (setSize(union) != 3) { return 1; }
  if (!setHas(union, 1)) { return 2; }
  if (!setHas(union, 2)) { return 3; }
  if (!setHas(union, 3)) { return 4; }
  return 0;
}

function testSetIntersection(): number {
  let set1: number = createSet();
  setAdd(set1, 1);
  setAdd(set1, 2);
  setAdd(set1, 3);

  let set2: number = createSet();
  setAdd(set2, 2);
  setAdd(set2, 3);
  setAdd(set2, 4);

  let intersection: number = setIntersection(set1, set2);
  if (setSize(intersection) != 2) { return 1; }
  if (!setHas(intersection, 2)) { return 2; }
  if (!setHas(intersection, 3)) { return 3; }
  if (setHas(intersection, 1)) { return 4; }
  return 0;
}

function testSetDifference(): number {
  let set1: number = createSet();
  setAdd(set1, 1);
  setAdd(set1, 2);
  setAdd(set1, 3);

  let set2: number = createSet();
  setAdd(set2, 2);
  setAdd(set2, 3);
  setAdd(set2, 4);

  let difference: number = setDifference(set1, set2);
  if (setSize(difference) != 1) { return 1; }
  if (!setHas(difference, 1)) { return 2; }
  if (setHas(difference, 2)) { return 3; }
  return 0;
}

function testSetEquals(): number {
  let set1: number = createSet();
  setAdd(set1, 1);
  setAdd(set1, 2);
  setAdd(set1, 3);

  let set2: number = createSet();
  setAdd(set2, 1);
  setAdd(set2, 2);
  setAdd(set2, 3);

  if (!setEquals(set1, set2)) { return 1; }

  let set3: number = createSet();
  setAdd(set3, 1);
  setAdd(set3, 2);

  if (setEquals(set1, set3)) { return 2; }
  return 0;
}

function testSetIsSubset(): number {
  let set1: number = createSet();
  setAdd(set1, 1);
  setAdd(set1, 2);

  let set2: number = createSet();
  setAdd(set2, 1);
  setAdd(set2, 2);
  setAdd(set2, 3);

  if (!setIsSubset(set1, set2)) { return 1; }
  if (setIsSubset(set2, set1)) { return 2; }
  return 0;
}

function testSetIsSuperset(): number {
  let set1: number = createSet();
  setAdd(set1, 1);
  setAdd(set1, 2);
  setAdd(set1, 3);

  let set2: number = createSet();
  setAdd(set2, 1);
  setAdd(set2, 2);

  if (!setIsSuperset(set1, set2)) { return 1; }
  if (setIsSuperset(set2, set1)) { return 2; }
  return 0;
}

function testClearAllCollections(): number {
  let map: number = createMap();
  let set: number = createSet();

  mapSet(map, "key", 42);
  setAdd(set, 42);

  clearAllCollections();

  let newMap: number = createMap();
  if (newMap != 0) { return 1; }
  return 0;
}
