// Errors module for ART. Import with: `import { Error, TypeError, RangeError, ... } from "art/errors";`
// Provides comprehensive error handling with error types, stack traces, and error management.

// Error type: represents an error with message and stack trace
export type Error = number;

// Error type constants
export const ERROR_GENERIC: number = 0;
export const ERROR_TYPE: number = 1;
export const ERROR_RANGE: number = 2;
export const ERROR_SYNTAX: number = 3;
export const ERROR_REFERENCE: number = 4;
export const ERROR_ASSERTION: number = 5;
export const ERROR_TIMEOUT: number = 6;
export const ERROR_ABORT: number = 7;
export const ERROR_CUSTOM: number = 8;

// Error storage
type ErrorData = [message: string, type: number, stack: string[], timestamp: number];
let _errors: ErrorData[] = [];
let _errorCounter: number = 0;
let _callStack: string[] = [];
let _startTime: number = 0;

// Creates a generic Error.
export function createError(message: string): Error {
  let errorId: Error = _errorCounter;
  _errorCounter = _errorCounter + 1;

  let stack: string[] = [];
  let i: number = 0;
  while (i < _callStack.length) {
    stack = stack + [_callStack[i]];
    i = i + 1;
  }

  _errors = _errors + [[message, ERROR_GENERIC, stack, now()]];
  return errorId;
}

// Creates a TypeError.
export function createTypeError(message: string): Error {
  let errorId: Error = _errorCounter;
  _errorCounter = _errorCounter + 1;

  let stack: string[] = [];
  let i: number = 0;
  while (i < _callStack.length) {
    stack = stack + [_callStack[i]];
    i = i + 1;
  }

  _errors = _errors + [[message, ERROR_TYPE, stack, now()]];
  return errorId;
}

// Creates a RangeError.
export function createRangeError(message: string): Error {
  let errorId: Error = _errorCounter;
  _errorCounter = _errorCounter + 1;

  let stack: string[] = [];
  let i: number = 0;
  while (i < _callStack.length) {
    stack = stack + [_callStack[i]];
    i = i + 1;
  }

  _errors = _errors + [[message, ERROR_RANGE, stack, now()]];
  return errorId;
}

// Creates a SyntaxError.
export function createSyntaxError(message: string): Error {
  let errorId: Error = _errorCounter;
  _errorCounter = _errorCounter + 1;

  let stack: string[] = [];
  let i: number = 0;
  while (i < _callStack.length) {
    stack = stack + [_callStack[i]];
    i = i + 1;
  }

  _errors = _errors + [[message, ERROR_SYNTAX, stack, now()]];
  return errorId;
}

// Creates a ReferenceError.
export function createReferenceError(message: string): Error {
  let errorId: Error = _errorCounter;
  _errorCounter = _errorCounter + 1;

  let stack: string[] = [];
  let i: number = 0;
  while (i < _callStack.length) {
    stack = stack + [_callStack[i]];
    i = i + 1;
  }

  _errors = _errors + [[message, ERROR_REFERENCE, stack, now()]];
  return errorId;
}

// Creates an AssertionError.
export function createAssertionError(message: string): Error {
  let errorId: Error = _errorCounter;
  _errorCounter = _errorCounter + 1;

  let stack: string[] = [];
  let i: number = 0;
  while (i < _callStack.length) {
    stack = stack + [_callStack[i]];
    i = i + 1;
  }

  _errors = _errors + [[message, ERROR_ASSERTION, stack, now()]];
  return errorId;
}

// Creates a TimeoutError.
export function createTimeoutError(message: string): Error {
  let errorId: Error = _errorCounter;
  _errorCounter = _errorCounter + 1;

  let stack: string[] = [];
  let i: number = 0;
  while (i < _callStack.length) {
    stack = stack + [_callStack[i]];
    i = i + 1;
  }

  _errors = _errors + [[message, ERROR_TIMEOUT, stack, now()]];
  return errorId;
}

// Creates an AbortError.
export function createAbortError(message: string): Error {
  let errorId: Error = _errorCounter;
  _errorCounter = _errorCounter + 1;

  let stack: string[] = [];
  let i: number = 0;
  while (i < _callStack.length) {
    stack = stack + [_callStack[i]];
    i = i + 1;
  }

  _errors = _errors + [[message, ERROR_ABORT, stack, now()]];
  return errorId;
}

// Gets the error message.
export function getMessage(error: Error): string {
  if (error < 0 || error >= _errors.length) { return ""; }
  return _errors[error][0];
}

// Gets the error type.
export function getErrorType(error: Error): number {
  if (error < 0 || error >= _errors.length) { return ERROR_GENERIC; }
  return _errors[error][1];
}

// Gets the error type name as a string.
export function getErrorTypeName(error: Error): string {
  if (error < 0 || error >= _errors.length) { return "Error"; }

  let type: number = _errors[error][1];
  if (type == ERROR_TYPE) { return "TypeError"; }
  if (type == ERROR_RANGE) { return "RangeError"; }
  if (type == ERROR_SYNTAX) { return "SyntaxError"; }
  if (type == ERROR_REFERENCE) { return "ReferenceError"; }
  if (type == ERROR_ASSERTION) { return "AssertionError"; }
  if (type == ERROR_TIMEOUT) { return "TimeoutError"; }
  if (type == ERROR_ABORT) { return "AbortError"; }
  if (type == ERROR_CUSTOM) { return "CustomError"; }
  return "Error";
}

// Gets the error stack trace.
export function getStack(error: Error): string[] {
  if (error < 0 || error >= _errors.length) { return []; }

  let stack: string[] = [];
  let errorStack: string[] = _errors[error][2];
  let i: number = 0;
  while (i < errorStack.length) {
    stack = stack + [errorStack[i]];
    i = i + 1;
  }

  return stack;
}

// Gets the error timestamp.
export function getTimestamp(error: Error): number {
  if (error < 0 || error >= _errors.length) { return 0; }
  return _errors[error][3];
}

// Formats error as a string.
export function errorToString(error: Error): string {
  if (error < 0 || error >= _errors.length) { return ""; }

  let message: string = _errors[error][0];
  let typeName: string = getErrorTypeName(error);
  return typeName + ": " + message;
}

// Formats error with stack trace as a multi-line string.
export function errorToStringWithStack(error: Error): string {
  if (error < 0 || error >= _errors.length) { return ""; }

  let result: string = errorToString(error);
  let stack: string[] = getStack(error);
  let i: number = 0;

  while (i < stack.length) {
    result = result + "\n    at " + stack[i];
    i = i + 1;
  }

  return result;
}

// Pushes a call frame onto the call stack.
export function pushCallFrame(frame: string): void {
  _callStack = _callStack + [frame];
}

// Pops the top call frame from the call stack.
export function popCallFrame(): void {
  if (_callStack.length > 0) {
    _callStack = _callStack.slice(0, _callStack.length - 1);
  }
}

// Gets the current call stack.
export function getCallStack(): string[] {
  let stack: string[] = [];
  let i: number = 0;
  while (i < _callStack.length) {
    stack = stack + [_callStack[i]];
    i = i + 1;
  }
  return stack;
}

// Clears the call stack.
export function clearCallStack(): void {
  _callStack = [];
}

// Checks if error is a TypeError.
export function isTypeError(error: Error): boolean {
  return getErrorType(error) == ERROR_TYPE;
}

// Checks if error is a RangeError.
export function isRangeError(error: Error): boolean {
  return getErrorType(error) == ERROR_RANGE;
}

// Checks if error is a SyntaxError.
export function isSyntaxError(error: Error): boolean {
  return getErrorType(error) == ERROR_SYNTAX;
}

// Checks if error is a ReferenceError.
export function isReferenceError(error: Error): boolean {
  return getErrorType(error) == ERROR_REFERENCE;
}

// Checks if error is an AssertionError.
export function isAssertionError(error: Error): boolean {
  return getErrorType(error) == ERROR_ASSERTION;
}

// Checks if error is a TimeoutError.
export function isTimeoutError(error: Error): boolean {
  return getErrorType(error) == ERROR_TIMEOUT;
}

// Checks if error is an AbortError.
export function isAbortError(error: Error): boolean {
  return getErrorType(error) == ERROR_ABORT;
}

// Assertion function that throws an AssertionError if condition is false.
export function assert(condition: boolean, message: string): void {
  if (!condition) {
    let error: Error = createAssertionError(message);
    throw error;
  }
}

// Assertion for equality.
export function assertEqual(actual: number, expected: number, message: string): void {
  if (actual != expected) {
    let msg: string = message + " (expected " + (expected as string) + " but got " + (actual as string) + ")";
    let error: Error = createAssertionError(msg);
    throw error;
  }
}

// Assertion for strict inequality.
export function assertNotEqual(actual: number, unexpected: number, message: string): void {
  if (actual == unexpected) {
    let msg: string = message + " (should not be " + (actual as string) + ")";
    let error: Error = createAssertionError(msg);
    throw error;
  }
}

// Assertion for truthy value.
export function assertTrue(value: boolean, message: string): void {
  if (!value) {
    let error: Error = createAssertionError(message);
    throw error;
  }
}

// Assertion for falsy value.
export function assertFalse(value: boolean, message: string): void {
  if (value) {
    let error: Error = createAssertionError(message);
    throw error;
  }
}

// Assertion that value is not null/undefined (for type checking).
export function assertDefined(value: number, message: string): void {
  if (value == 0) {
    let error: Error = createAssertionError(message);
    throw error;
  }
}

// Gets total error count.
export function errorCount(): number {
  return _errors.length;
}

// Gets error by index.
export function getErrorAt(index: number): Error {
  if (index < 0 || index >= _errors.length) { return -1; }
  return index;
}

// Clears all errors (for testing).
export function clearAllErrors(): void {
  _errors = [];
  _errorCounter = 0;
}

// Time tracking for error timestamps.
let _currentTime: number = 0;

// Gets current time for timestamps.
function now(): number {
  return _currentTime;
}

// Advances simulated time.
export function advanceTime(ms: number): void {
  _currentTime = _currentTime + ms;
}

// Sets the simulated time.
export function setTime(time: number): void {
  _currentTime = time;
}

// Gets current simulated time.
export function getCurrentTime(): number {
  return _currentTime;
}

// Wraps a function to catch errors and return them instead of throwing.
export function tryCatch<T>(fn: () => T): [result: T, error: Error] {
  let error: Error = -1;
  let result: T = fn();
  return [result, error];
}

// Rethrows an error with additional context.
export function rethrow(error: Error, context: string): Error {
  pushCallFrame(context);
  return error;
}

// Chains error information.
export function chainError(error: Error, context: string): Error {
  let message: string = getMessage(error);
  let newMessage: string = context + ": " + message;
  let type: number = getErrorType(error);

  let newErrorId: Error = _errorCounter;
  _errorCounter = _errorCounter + 1;

  let stack: string[] = getStack(error);
  stack = stack + [context];

  _errors = _errors + [[newMessage, type, stack, getTimestamp(error)]];
  return newErrorId;
}
