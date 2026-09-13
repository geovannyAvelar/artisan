import { keys, values, entries, assign, copy, merge, hasProperty, getProperty, setProperty, deleteProperty, length, isEmpty, forEachProperty, mapProperties } from "art/object";

function testKeys(): number {
  let obj: number[] = [1, 2, 3];
  let k: number[] = keys(obj);
  if (k.length != 3) { return 1; }
  if (k[0] != 0 || k[1] != 1 || k[2] != 2) { return 2; }
  return 0;
}

function testValues(): number {
  let obj: number[] = [10, 20, 30];
  let v: number[] = values(obj);
  if (v.length != 3) { return 1; }
  if (v[0] != 10 || v[2] != 30) { return 2; }
  return 0;
}

function testEntries(): number {
  let obj: number[] = [5, 6];
  let e: number[][] = entries(obj);
  if (e.length != 2) { return 1; }
  if (e[0][0] != 0 || e[0][1] != 5) { return 2; }
  return 0;
}

function testAssign(): number {
  let target: number[] = [1, 2];
  let source: number[] = [10, 20];
  assign(target, source);
  if (target[0] != 10 || target[1] != 20) { return 1; }
  return 0;
}

function testCopy(): number {
  let obj: number[] = [1, 2, 3];
  let c: number[] = copy(obj);
  if (c.length != obj.length) { return 1; }
  if (c[0] != 1 || c[2] != 3) { return 2; }
  return 0;
}

function testMerge(): number {
  let obj1: number[] = [1, 2];
  let obj2: number[] = [10, 20];
  let m: number[] = merge(obj1, obj2);
  if (m.length != 2) { return 1; }
  if (m[0] != 10 || m[1] != 20) { return 2; }
  return 0;
}

function testHasProperty(): number {
  let obj: number[] = [1, 2, 3];
  if (!hasProperty(obj, 0)) { return 1; }
  if (!hasProperty(obj, 2)) { return 2; }
  if (hasProperty(obj, 5)) { return 3; }
  return 0;
}

function testGetProperty(): number {
  let obj: number[] = [10, 20, 30];
  if (getProperty(obj, 1) != 20) { return 1; }
  if (getProperty(obj, 5) != 0) { return 2; }
  return 0;
}

function testLength(): number {
  let obj: number[] = [1, 2, 3];
  if (length(obj) != 3) { return 1; }

  let empty: number[] = [];
  if (length(empty) != 0) { return 2; }
  return 0;
}

function testIsEmpty(): number {
  let obj: number[] = [1];
  if (isEmpty(obj)) { return 1; }

  let empty: number[] = [];
  if (!isEmpty(empty)) { return 2; }
  return 0;
}

function testMapProperties(): number {
  let obj: number[] = [1, 2, 3];
  let m: number[] = mapProperties(obj, function(key: number, value: number): number { return value * 2; });
  if (m[0] != 2 || m[1] != 4 || m[2] != 6) { return 1; }
  return 0;
}
