// Collections module for ART. Import with: `import { Map, Set, ... } from "art/collections";`
// Provides Map and Set implementations using number-based identifiers.

// Simple key-value pair structure
type KeyValuePair = [key: string, value: number];

// Map-like collection implementation
export type Map = number;

// Set-like collection implementation
export type Set = number;

// Map storage: each Map ID maps to an array of [key, value] pairs
let _mapStorage: KeyValuePair[][] = [];
let _mapCounter: number = 0;

// Set storage: each Set ID maps to an array of values
let _setStorage: number[][] = [];
let _setCounter: number = 0;

// Creates a new Map.
export function createMap(): Map {
  let mapId: Map = _mapCounter;
  _mapCounter = _mapCounter + 1;
  _mapStorage = _mapStorage + [[]];
  return mapId;
}

// Creates a new Set.
export function createSet(): Set {
  let setId: Set = _setCounter;
  _setCounter = _setCounter + 1;
  _setStorage = _setStorage + [[]];
  return setId;
}

// Sets a value in a Map.
export function mapSet(map: Map, key: string, value: number): void {
  if (map < 0 || map >= _mapStorage.length) { return; }

  let entries: KeyValuePair[] = _mapStorage[map];
  let i: number = 0;
  let found: boolean = false;

  while (i < entries.length) {
    if (entries[i][0] == key) {
      entries[i] = [key, value];
      found = true;
    }
    i = i + 1;
  }

  if (!found) {
    entries = entries + [[key, value]];
    _mapStorage[map] = entries;
  }
}

// Gets a value from a Map. Returns 0 if not found.
export function mapGet(map: Map, key: string): number {
  if (map < 0 || map >= _mapStorage.length) { return 0; }

  let entries: KeyValuePair[] = _mapStorage[map];
  let i: number = 0;

  while (i < entries.length) {
    if (entries[i][0] == key) {
      return entries[i][1];
    }
    i = i + 1;
  }

  return 0;
}

// Checks if a Map has a key.
export function mapHas(map: Map, key: string): boolean {
  if (map < 0 || map >= _mapStorage.length) { return false; }

  let entries: KeyValuePair[] = _mapStorage[map];
  let i: number = 0;

  while (i < entries.length) {
    if (entries[i][0] == key) {
      return true;
    }
    i = i + 1;
  }

  return false;
}

// Deletes a key from a Map.
export function mapDelete(map: Map, key: string): boolean {
  if (map < 0 || map >= _mapStorage.length) { return false; }

  let entries: KeyValuePair[] = _mapStorage[map];
  let newEntries: KeyValuePair[] = [];
  let found: boolean = false;
  let i: number = 0;

  while (i < entries.length) {
    if (entries[i][0] != key) {
      newEntries = newEntries + [entries[i]];
    } else {
      found = true;
    }
    i = i + 1;
  }

  _mapStorage[map] = newEntries;
  return found;
}

// Clears all entries from a Map.
export function mapClear(map: Map): void {
  if (map < 0 || map >= _mapStorage.length) { return; }
  _mapStorage[map] = [];
}

// Gets the number of entries in a Map.
export function mapSize(map: Map): number {
  if (map < 0 || map >= _mapStorage.length) { return 0; }
  return _mapStorage[map].length;
}

// Gets all keys from a Map.
export function mapKeys(map: Map): string[] {
  if (map < 0 || map >= _mapStorage.length) { return []; }

  let entries: KeyValuePair[] = _mapStorage[map];
  let keys: string[] = [];
  let i: number = 0;

  while (i < entries.length) {
    keys = keys + [entries[i][0]];
    i = i + 1;
  }

  return keys;
}

// Gets all values from a Map.
export function mapValues(map: Map): number[] {
  if (map < 0 || map >= _mapStorage.length) { return []; }

  let entries: KeyValuePair[] = _mapStorage[map];
  let values: number[] = [];
  let i: number = 0;

  while (i < entries.length) {
    values = values + [entries[i][1]];
    i = i + 1;
  }

  return values;
}

// Gets all entries from a Map as [key, value] pairs.
export function mapEntries(map: Map): KeyValuePair[] {
  if (map < 0 || map >= _mapStorage.length) { return []; }
  return _mapStorage[map];
}

// Iterates over a Map with a callback for each [key, value] pair.
export function mapForEach(map: Map, callback: (key: string, value: number) => void): void {
  if (map < 0 || map >= _mapStorage.length) { return; }

  let entries: KeyValuePair[] = _mapStorage[map];
  let i: number = 0;

  while (i < entries.length) {
    callback(entries[i][0], entries[i][1]);
    i = i + 1;
  }
}

// Adds a value to a Set.
export function setAdd(set: Set, value: number): void {
  if (set < 0 || set >= _setStorage.length) { return; }

  let values: number[] = _setStorage[set];
  let i: number = 0;
  let found: boolean = false;

  while (i < values.length) {
    if (values[i] == value) {
      found = true;
    }
    i = i + 1;
  }

  if (!found) {
    values = values + [value];
    _setStorage[set] = values;
  }
}

// Checks if a Set has a value.
export function setHas(set: Set, value: number): boolean {
  if (set < 0 || set >= _setStorage.length) { return false; }

  let values: number[] = _setStorage[set];
  let i: number = 0;

  while (i < values.length) {
    if (values[i] == value) {
      return true;
    }
    i = i + 1;
  }

  return false;
}

// Deletes a value from a Set.
export function setDelete(set: Set, value: number): boolean {
  if (set < 0 || set >= _setStorage.length) { return false; }

  let values: number[] = _setStorage[set];
  let newValues: number[] = [];
  let found: boolean = false;
  let i: number = 0;

  while (i < values.length) {
    if (values[i] != value) {
      newValues = newValues + [values[i]];
    } else {
      found = true;
    }
    i = i + 1;
  }

  _setStorage[set] = newValues;
  return found;
}

// Clears all values from a Set.
export function setClear(set: Set): void {
  if (set < 0 || set >= _setStorage.length) { return; }
  _setStorage[set] = [];
}

// Gets the number of values in a Set.
export function setSize(set: Set): number {
  if (set < 0 || set >= _setStorage.length) { return 0; }
  return _setStorage[set].length;
}

// Gets all values from a Set as an array.
export function setValues(set: Set): number[] {
  if (set < 0 || set >= _setStorage.length) { return []; }
  return _setStorage[set];
}

// Iterates over a Set with a callback for each value.
export function setForEach(set: Set, callback: (value: number) => void): void {
  if (set < 0 || set >= _setStorage.length) { return; }

  let values: number[] = _setStorage[set];
  let i: number = 0;

  while (i < values.length) {
    callback(values[i]);
    i = i + 1;
  }
}

// Creates the intersection of two Sets.
export function setIntersection(set1: Set, set2: Set): Set {
  let result: Set = createSet();
  if (set1 < 0 || set1 >= _setStorage.length) { return result; }
  if (set2 < 0 || set2 >= _setStorage.length) { return result; }

  let values1: number[] = _setStorage[set1];
  let i: number = 0;

  while (i < values1.length) {
    if (setHas(set2, values1[i])) {
      setAdd(result, values1[i]);
    }
    i = i + 1;
  }

  return result;
}

// Creates the union of two Sets.
export function setUnion(set1: Set, set2: Set): Set {
  let result: Set = createSet();
  if (set1 < 0 || set1 >= _setStorage.length) { return result; }
  if (set2 < 0 || set2 >= _setStorage.length) { return result; }

  let values1: number[] = _setStorage[set1];
  let values2: number[] = _setStorage[set2];
  let i: number = 0;

  while (i < values1.length) {
    setAdd(result, values1[i]);
    i = i + 1;
  }

  i = 0;
  while (i < values2.length) {
    setAdd(result, values2[i]);
    i = i + 1;
  }

  return result;
}

// Creates the difference of two Sets (set1 - set2).
export function setDifference(set1: Set, set2: Set): Set {
  let result: Set = createSet();
  if (set1 < 0 || set1 >= _setStorage.length) { return result; }
  if (set2 < 0 || set2 >= _setStorage.length) { return result; }

  let values1: number[] = _setStorage[set1];
  let i: number = 0;

  while (i < values1.length) {
    if (!setHas(set2, values1[i])) {
      setAdd(result, values1[i]);
    }
    i = i + 1;
  }

  return result;
}

// Checks if two Sets are equal.
export function setEquals(set1: Set, set2: Set): boolean {
  if (setSize(set1) != setSize(set2)) { return false; }

  let values1: number[] = setValues(set1);
  let i: number = 0;

  while (i < values1.length) {
    if (!setHas(set2, values1[i])) {
      return false;
    }
    i = i + 1;
  }

  return true;
}

// Checks if set1 is a subset of set2.
export function setIsSubset(set1: Set, set2: Set): boolean {
  let values1: number[] = setValues(set1);
  let i: number = 0;

  while (i < values1.length) {
    if (!setHas(set2, values1[i])) {
      return false;
    }
    i = i + 1;
  }

  return true;
}

// Checks if set1 is a superset of set2.
export function setIsSuperset(set1: Set, set2: Set): boolean {
  return setIsSubset(set2, set1);
}

// Clears all Maps and Sets (for testing).
export function clearAllCollections(): void {
  _mapStorage = [];
  _setStorage = [];
  _mapCounter = 0;
  _setCounter = 0;
}
