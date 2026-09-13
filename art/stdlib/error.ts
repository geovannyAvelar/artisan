// Error handling utilities for ART. Import with: `import { Error, throwError, ... } from "art/error";`
// Since ART doesn't have true exceptions, we use error codes and messages.

// Error codes - use these to identify error types
export const ERROR_UNKNOWN: number = 0;
export const ERROR_TYPE: number = 1;
export const ERROR_RANGE: number = 2;
export const ERROR_VALUE: number = 3;
export const ERROR_NULL: number = 4;
export const ERROR_INDEX: number = 5;
export const ERROR_NOT_FOUND: number = 6;
export const ERROR_INVALID: number = 7;
export const ERROR_OVERFLOW: number = 8;
export const ERROR_UNDERFLOW: number = 9;
export const ERROR_PERMISSION: number = 10;

// Global error state
let _lastErrorCode: number = 0;
let _lastErrorMessage: string = "";
let _errorThrown: boolean = false;

// Represents an error object
export function createError(code: number, message: string): number {
  _lastErrorCode = code;
  _lastErrorMessage = message;
  return code;
}

// Throws an error (sets error state). Returns error code.
export function throwError(code: number, message: string): number {
  _lastErrorCode = code;
  _lastErrorMessage = message;
  _errorThrown = true;
  return code;
}

// Returns the last error code.
export function getErrorCode(): number {
  return _lastErrorCode;
}

// Returns the last error message.
export function getErrorMessage(): string {
  return _lastErrorMessage;
}

// Clears the error state.
export function clearError(): void {
  _lastErrorCode = 0;
  _lastErrorMessage = "";
  _errorThrown = false;
}

// Returns true if an error has been thrown.
export function hasError(): boolean {
  return _errorThrown;
}

// Checks if a condition is true, throws error if false.
export function assert(condition: boolean, message: string): void {
  if (!condition) {
    throwError(ERROR_VALUE, message);
  }
}

// Throws a type error.
export function typeError(message: string): number {
  return throwError(ERROR_TYPE, message);
}

// Throws a range error.
export function rangeError(message: string): number {
  return throwError(ERROR_RANGE, message);
}

// Throws a value error.
export function valueError(message: string): number {
  return throwError(ERROR_VALUE, message);
}

// Throws a null/undefined reference error.
export function nullError(message: string): number {
  return throwError(ERROR_NULL, message);
}

// Throws an index error.
export function indexError(message: string): number {
  return throwError(ERROR_INDEX, message);
}

// Throws a not found error.
export function notFoundError(message: string): number {
  return throwError(ERROR_NOT_FOUND, message);
}

// Creates an error message string from code and message.
export function errorToString(code: number, message: string): string {
  let typeStr: string = "";
  if (code == ERROR_TYPE) { typeStr = "TypeError"; }
  else if (code == ERROR_RANGE) { typeStr = "RangeError"; }
  else if (code == ERROR_VALUE) { typeStr = "ValueError"; }
  else if (code == ERROR_NULL) { typeStr = "NullError"; }
  else if (code == ERROR_INDEX) { typeStr = "IndexError"; }
  else if (code == ERROR_NOT_FOUND) { typeStr = "NotFoundError"; }
  else if (code == ERROR_INVALID) { typeStr = "InvalidError"; }
  else if (code == ERROR_OVERFLOW) { typeStr = "OverflowError"; }
  else if (code == ERROR_UNDERFLOW) { typeStr = "UnderflowError"; }
  else if (code == ERROR_PERMISSION) { typeStr = "PermissionError"; }
  else { typeStr = "Error"; }

  return typeStr + ": " + message;
}

// Simulates try-catch by checking error state after operation.
// Usage: operation(); if (hasError()) { handle error }
export function handleError(handler: (code: number, message: string) => void): void {
  if (hasError()) {
    handler(getErrorCode(), getErrorMessage());
    clearError();
  }
}

// Validates value is not null/undefined. Returns value or throws error.
export function requireNonNull(value: number, message: string): number {
  if (value == 0) {
    nullError(message);
  }
  return value;
}

// Validates value is within range. Throws error if not.
export function requireInRange(value: number, min: number, max: number, message: string): number {
  if (value < min || value > max) {
    rangeError(message);
  }
  return value;
}

// Validates array index is valid. Throws error if not.
export function requireValidIndex(index: number, arrayLength: number, message: string): number {
  if (index < 0 || index >= arrayLength) {
    indexError(message);
  }
  return index;
}

// Validates condition. Throws error with formatted message if false.
export function require(condition: boolean, code: number, message: string): void {
  if (!condition) {
    throwError(code, message);
  }
}

// Error recovery - attempts operation, returns default on error.
export function tryOrDefault(value: number, defaultValue: number): number {
  if (hasError()) {
    clearError();
    return defaultValue;
  }
  return value;
}

// Error recovery - attempts operation, returns 0 on error.
export function tryOrZero(value: number): number {
  if (hasError()) {
    clearError();
    return 0;
  }
  return value;
}
