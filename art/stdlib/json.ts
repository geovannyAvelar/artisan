// JSON module for ART. Import with: `import { stringify, parse } from "art/json";`
// Provides JSON serialization and deserialization.

export function stringify(value: any): string {
  return stringifyValue(value);
}

export function parse(text: string): any {
  let result: [value: any, index: number] = parseValue(text, 0);
  return result[0];
}

function stringifyValue(value: any): string {
  if (value == null) { return "null"; }
  
  let valueType: string = typeof value;
  
  if (valueType == "string") {
    return stringifyString(value as string);
  }
  if (valueType == "number") {
    return value as string;
  }
  if (valueType == "boolean") {
    if (value as boolean) { return "true"; }
    return "false";
  }
  
  if (isArray(value)) {
    return stringifyArray(value as any[]);
  }
  
  return "{}";
}

function stringifyString(str: string): string {
  let result: string = "\"";
  let i: number = 0;
  
  while (i < str.length) {
    let c: string = str.substring(i, i + 1);
    
    if (c == "\"") {
      result = result + "\\\"";
    } else if (c == "\\") {
      result = result + "\\\\";
    } else if (c == "\n") {
      result = result + "\\n";
    } else if (c == "\r") {
      result = result + "\\r";
    } else if (c == "\t") {
      result = result + "\\t";
    } else {
      result = result + c;
    }
    
    i = i + 1;
  }
  
  result = result + "\"";
  return result;
}

function stringifyArray(arr: any[]): string {
  if (arr.length == 0) { return "[]"; }
  
  let result: string = "[";
  let i: number = 0;
  
  while (i < arr.length) {
    if (i > 0) {
      result = result + ",";
    }
    result = result + stringifyValue(arr[i]);
    i = i + 1;
  }
  
  result = result + "]";
  return result;
}

function isArray(value: any): boolean {
  if (value == null) { return false; }
  let valueType: string = typeof value;
  return valueType == "array" || valueType == "number[]" || valueType == "string[]";
}

function parseValue(text: string, index: number): [value: any, index: number] {
  index = skipWhitespace(text, index);
  
  if (index >= text.length) { return [null, index]; }
  
  let c: string = text.substring(index, index + 1);
  
  if (c == "\"") {
    return parseString(text, index);
  }
  if (c == "{") {
    return parseObject(text, index);
  }
  if (c == "[") {
    return parseArray(text, index);
  }
  if (c == "t" || c == "f") {
    return parseBoolean(text, index);
  }
  if (c == "n") {
    return parseNull(text, index);
  }
  if (c == "-" || (c >= "0" && c <= "9")) {
    return parseNumber(text, index);
  }
  
  return [null, index];
}

function parseString(text: string, index: number): [value: string, index: number] {
  index = index + 1;
  let result: string = "";
  
  while (index < text.length) {
    let c: string = text.substring(index, index + 1);
    
    if (c == "\"") {
      return [result, index + 1];
    }
    if (c == "\\") {
      index = index + 1;
      if (index < text.length) {
        let escaped: string = text.substring(index, index + 1);
        if (escaped == "\"") {
          result = result + "\"";
        } else if (escaped == "\\") {
          result = result + "\\";
        } else if (escaped == "n") {
          result = result + "\n";
        } else if (escaped == "r") {
          result = result + "\r";
        } else if (escaped == "t") {
          result = result + "\t";
        } else {
          result = result + escaped;
        }
      }
    } else {
      result = result + c;
    }
    
    index = index + 1;
  }
  
  return [result, index];
}

function parseNumber(text: string, index: number): [value: number, index: number] {
  let isNegative: boolean = false;
  
  if (index < text.length && text.substring(index, index + 1) == "-") {
    isNegative = true;
    index = index + 1;
  }
  
  let num: number = 0;
  while (index < text.length) {
    let c: string = text.substring(index, index + 1);
    if (c >= "0" && c <= "9") {
      num = num * 10 + (c.charCodeAt(0) - "0".charCodeAt(0));
      index = index + 1;
    } else {
      break;
    }
  }
  
  if (isNegative) {
    num = 0 - num;
  }
  
  return [num, index];
}

function parseBoolean(text: string, index: number): [value: boolean, index: number] {
  if (index + 4 <= text.length && text.substring(index, index + 4) == "true") {
    return [true, index + 4];
  }
  if (index + 5 <= text.length && text.substring(index, index + 5) == "false") {
    return [false, index + 5];
  }
  return [false, index];
}

function parseNull(text: string, index: number): [value: any, index: number] {
  if (index + 4 <= text.length && text.substring(index, index + 4) == "null") {
    return [null, index + 4];
  }
  return [null, index];
}

function parseArray(text: string, index: number): [value: any[], index: number] {
  index = index + 1;
  let result: any[] = [];
  
  index = skipWhitespace(text, index);
  if (index < text.length && text.substring(index, index + 1) == "]") {
    return [result, index + 1];
  }
  
  while (index < text.length) {
    let value: [value: any, index: number] = parseValue(text, index);
    result = result + [value[0]];
    index = value[1];
    
    index = skipWhitespace(text, index);
    if (index >= text.length) { break; }
    
    let c: string = text.substring(index, index + 1);
    if (c == "]") {
      return [result, index + 1];
    }
    if (c == ",") {
      index = index + 1;
    } else {
      break;
    }
  }
  
  return [result, index];
}

function parseObject(text: string, index: number): [value: any, index: number] {
  index = index + 1;
  let result: any = {};
  
  index = skipWhitespace(text, index);
  if (index < text.length && text.substring(index, index + 1) == "}") {
    return [result, index + 1];
  }
  
  while (index < text.length) {
    index = skipWhitespace(text, index);
    
    if (index >= text.length || text.substring(index, index + 1) != "\"") { break; }
    
    let keyResult: [value: string, index: number] = parseString(text, index);
    let key: string = keyResult[0];
    index = keyResult[1];
    
    index = skipWhitespace(text, index);
    if (index >= text.length || text.substring(index, index + 1) != ":") { break; }
    index = index + 1;
    
    let valueResult: [value: any, index: number] = parseValue(text, index);
    result[key] = valueResult[0];
    index = valueResult[1];
    
    index = skipWhitespace(text, index);
    if (index >= text.length) { break; }
    
    let c: string = text.substring(index, index + 1);
    if (c == "}") {
      return [result, index + 1];
    }
    if (c == ",") {
      index = index + 1;
    } else {
      break;
    }
  }
  
  return [result, index];
}

function skipWhitespace(text: string, index: number): number {
  while (index < text.length) {
    let c: string = text.substring(index, index + 1);
    if (c != " " && c != "\t" && c != "\n" && c != "\r") {
      break;
    }
    index = index + 1;
  }
  return index;
}
