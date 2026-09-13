import { keys, values, entries, assign, hasOwnProperty } from "art/object";

function testKeysEmpty(): number {
  let obj: any = {};
  let result: string[] = keys(obj);
  if (result.length != 0) { return 1; }
  return 0;
}

function testKeysNull(): number {
  let result: string[] = keys(null);
  if (result.length != 0) { return 1; }
  return 0;
}

function testValuesEmpty(): number {
  let obj: any = {};
  let result: any[] = values(obj);
  if (result.length != 0) { return 1; }
  return 0;
}

function testValuesNull(): number {
  let result: any[] = values(null);
  if (result.length != 0) { return 1; }
  return 0;
}

function testEntriesEmpty(): number {
  let obj: any = {};
  let result: [string, any][] = entries(obj);
  if (result.length != 0) { return 1; }
  return 0;
}

function testEntriesNull(): number {
  let result: [string, any][] = entries(null);
  if (result.length != 0) { return 1; }
  return 0;
}

function testAssignBasic(): number {
  let target: any = {};
  let source: any = {};
  source["a"] = 1;
  
  let result: any = assign(target, source);
  if (result["a"] != 1) { return 1; }
  return 0;
}

function testAssignNull(): number {
  let target: any = null;
  let source: any = {};
  source["a"] = 1;
  
  let result: any = assign(target, source);
  if (result != null) { return 1; }
  return 0;
}

function testAssignMultiple(): number {
  let target: any = {};
  let source1: any = {};
  let source2: any = {};
  source1["a"] = 1;
  source2["b"] = 2;
  
  let result: any = assign(target, source1, source2);
  if (result["a"] != 1) { return 1; }
  if (result["b"] != 2) { return 2; }
  return 0;
}

function testHasOwnPropertyTrue(): number {
  let obj: any = {};
  obj["key"] = "value";
  
  if (!hasOwnProperty(obj, "key")) { return 1; }
  return 0;
}

function testHasOwnPropertyFalse(): number {
  let obj: any = {};
  if (hasOwnProperty(obj, "missing")) { return 1; }
  return 0;
}

function testHasOwnPropertyNull(): number {
  if (hasOwnProperty(null, "any")) { return 1; }
  return 0;
}
