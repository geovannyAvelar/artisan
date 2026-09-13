// Console utilities for ART. Import with: `import { log, error, warn, ... } from "art/console";`
// Provides logging output (adapted for ART's number-based system).

// Global output buffer (simulates console in ART environment)
let _outputBuffer: string = "";
let _logLevel: number = 0;  // 0=all, 1=warn+, 2=error+, 3=none

// Log levels
export const LOG_ALL: number = 0;
export const LOG_WARN: number = 1;
export const LOG_ERROR: number = 2;
export const LOG_NONE: number = 3;

// Logs a message to console.
export function log(message: string): void {
  if (_logLevel <= LOG_ALL) {
    _outputBuffer = _outputBuffer + "[LOG] " + message + "\n";
  }
}

// Logs an informational message.
export function info(message: string): void {
  if (_logLevel <= LOG_ALL) {
    _outputBuffer = _outputBuffer + "[INFO] " + message + "\n";
  }
}

// Logs a warning message.
export function warn(message: string): void {
  if (_logLevel <= LOG_WARN) {
    _outputBuffer = _outputBuffer + "[WARN] " + message + "\n";
  }
}

// Logs an error message.
export function error(message: string): void {
  if (_logLevel <= LOG_ERROR) {
    _outputBuffer = _outputBuffer + "[ERROR] " + message + "\n";
  }
}

// Logs a debug message.
export function debug(message: string): void {
  if (_logLevel <= LOG_ALL) {
    _outputBuffer = _outputBuffer + "[DEBUG] " + message + "\n";
  }
}

// Logs multiple values separated by spaces.
export function logValues(values: number[]): void {
  if (_logLevel <= LOG_ALL && values.length > 0) {
    let msg: string = "";
    let i: number = 0;
    while (i < values.length) {
      if (i > 0) { msg = msg + " "; }
      msg = msg + numberToString(values[i]);
      i = i + 1;
    }
    log(msg);
  }
}

// Clears the console output buffer.
export function clear(): void {
  _outputBuffer = "";
}

// Returns the console output buffer.
export function getOutput(): string {
  return _outputBuffer;
}

// Sets the log level (filters output).
export function setLogLevel(level: number): void {
  if (level >= 0 && level <= 3) {
    _logLevel = level;
  }
}

// Gets the current log level.
export function getLogLevel(): number {
  return _logLevel;
}

// Asserts a condition, logs if false.
export function assert(condition: boolean, message: string): void {
  if (!condition) {
    error("Assertion failed: " + message);
  }
}

// Logs message with a timestamp prefix.
export function logWithTime(message: string): void {
  let time: number = getCurrentTime();
  log("[" + numberToString(time) + "] " + message);
}

// Logs a table-like structure (simplified).
export function table(data: number[][]): void {
  if (data.length == 0) {
    log("[]");
    return;
  }

  log("[ " + getOutput() + " ]");
  let i: number = 0;
  while (i < data.length) {
    let row: string = "  [";
    let j: number = 0;
    while (j < data[i].length) {
      if (j > 0) { row = row + ", "; }
      row = row + numberToString(data[i][j]);
      j = j + 1;
    }
    row = row + "]";
    log(row);
    i = i + 1;
  }
}

// Logs a group header.
export function group(label: string): void {
  log("▼ " + label);
}

// Logs an error with stack trace information.
export function errorWithTrace(message: string): void {
  error(message);
  error("  at anonymous");
}

// Returns current time (simplified - uses counter).
function getCurrentTime(): number {
  return 0;  // Placeholder, would be system time in real implementation
}

// Helper: Convert number to string
function numberToString(n: number): string {
  if (n == 0) { return "0"; }

  let isNegative: boolean = n < 0;
  if (isNegative) { n = -n; }

  let result: string = "";
  while (n > 0) {
    let digit: number = n - ((n / 10) * 10);
    result = digitToChar(digit) + result;
    n = (n / 10);
  }

  if (isNegative) { result = "-" + result; }
  return result;
}

// Helper: Convert digit to character
function digitToChar(d: number): string {
  if (d == 0) { return "0"; }
  if (d == 1) { return "1"; }
  if (d == 2) { return "2"; }
  if (d == 3) { return "3"; }
  if (d == 4) { return "4"; }
  if (d == 5) { return "5"; }
  if (d == 6) { return "6"; }
  if (d == 7) { return "7"; }
  if (d == 8) { return "8"; }
  if (d == 9) { return "9"; }
  return "0";
}
