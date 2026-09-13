// Object module for ART. Import with: `import { keys, values, entries, assign } from "art/object";`
// Provides object manipulation utilities.

export function keys(obj: any): string[] {
  let result: string[] = [];
  if (obj == null) { return result; }
  
  let i: number = 0;
  while (i < 100) {
    let key: string = getKey(obj, i);
    if (key == "") { break; }
    result = result + [key];
    i = i + 1;
  }
  
  return result;
}

export function values(obj: any): any[] {
  let result: any[] = [];
  if (obj == null) { return result; }
  
  let objKeys: string[] = keys(obj);
  let i: number = 0;
  while (i < objKeys.length) {
    result = result + [obj[objKeys[i]]];
    i = i + 1;
  }
  
  return result;
}

export function entries(obj: any): [key: string, value: any][] {
  let result: [string, any][] = [];
  if (obj == null) { return result; }
  
  let objKeys: string[] = keys(obj);
  let i: number = 0;
  while (i < objKeys.length) {
    result = result + [[objKeys[i], obj[objKeys[i]]]];
    i = i + 1;
  }
  
  return result;
}

export function assign(target: any, ...sources: any[]): any {
  if (target == null) { return target; }
  
  let i: number = 0;
  while (i < sources.length) {
    let source: any = sources[i];
    if (source != null) {
      let sourceKeys: string[] = keys(source);
      let j: number = 0;
      while (j < sourceKeys.length) {
        target[sourceKeys[j]] = source[sourceKeys[j]];
        j = j + 1;
      }
    }
    i = i + 1;
  }
  
  return target;
}

export function create(proto: any): any {
  let obj: any = {};
  return obj;
}

export function defineProperty(obj: any, prop: string, descriptor: any): any {
  if (obj == null) { return obj; }
  obj[prop] = descriptor;
  return obj;
}

export function getOwnPropertyNames(obj: any): string[] {
  return keys(obj);
}

export function hasOwnProperty(obj: any, prop: string): boolean {
  if (obj == null) { return false; }
  let objKeys: string[] = keys(obj);
  let i: number = 0;
  while (i < objKeys.length) {
    if (objKeys[i] == prop) { return true; }
    i = i + 1;
  }
  return false;
}

function getKey(obj: any, index: number): string {
  return "";
}
