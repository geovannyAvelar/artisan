// Console module for ART. Import with: `import { log, error, warn, info } from "art/console";`
// Provides console output utilities for debugging and logging.

type LogEntry = [level: string, message: string, timestamp: number];
let _logs: LogEntry[] = [];
let _currentTime: number = 0;

export function log(...args: any[]): void {
  let message: string = formatArgs(args);
  _logs = _logs + [["log", message, _currentTime]];
}

export function error(...args: any[]): void {
  let message: string = formatArgs(args);
  _logs = _logs + [["error", message, _currentTime]];
}

export function warn(...args: any[]): void {
  let message: string = formatArgs(args);
  _logs = _logs + [["warn", message, _currentTime]];
}

export function info(...args: any[]): void {
  let message: string = formatArgs(args);
  _logs = _logs + [["info", message, _currentTime]];
}

export function debug(...args: any[]): void {
  let message: string = formatArgs(args);
  _logs = _logs + [["debug", message, _currentTime]];
}

export function trace(): void {
  _logs = _logs + [["trace", "stack trace", _currentTime]];
}

export function assert(condition: boolean, message: string): void {
  if (!condition) {
    _logs = _logs + [["assert", message, _currentTime]];
  }
}

export function clear(): void {
  _logs = [];
}

export function count(label: string): void {
  let message: string = label + ": 1";
  _logs = _logs + [["count", message, _currentTime]];
}

export function time(label: string): void {
  _logs = _logs + [["time", label, _currentTime]];
}

export function timeEnd(label: string): void {
  _logs = _logs + [["timeEnd", label, _currentTime]];
}

export function table(data: any[]): void {
  let message: string = "table";
  _logs = _logs + [["table", message, _currentTime]];
}

export function group(label: string): void {
  _logs = _logs + [["group", label, _currentTime]];
}

export function groupEnd(): void {
  _logs = _logs + [["groupEnd", "", _currentTime]];
}

export function getLogs(): LogEntry[] {
  let result: LogEntry[] = [];
  let i: number = 0;
  while (i < _logs.length) {
    result = result + [_logs[i]];
    i = i + 1;
  }
  return result;
}

export function getLogCount(): number {
  return _logs.length;
}

export function getLastLog(): LogEntry {
  if (_logs.length == 0) { return ["", "", 0]; }
  return _logs[_logs.length - 1];
}

export function setTime(time: number): void {
  _currentTime = time;
}

export function getTime(): number {
  return _currentTime;
}

function formatArgs(args: any[]): string {
  let result: string = "";
  let i: number = 0;
  
  while (i < args.length) {
    if (i > 0) {
      result = result + " ";
    }
    result = result + formatValue(args[i]);
    i = i + 1;
  }
  
  return result;
}

function formatValue(value: any): string {
  if (value == null) { return "null"; }
  
  let valueType: string = typeof value;
  
  if (valueType == "string") {
    return value as string;
  }
  if (valueType == "number") {
    return value as string;
  }
  if (valueType == "boolean") {
    if (value as boolean) { return "true"; }
    return "false";
  }
  
  return "[object Object]";
}
