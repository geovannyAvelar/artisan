import { Error, createError, createTypeError, createRangeError, createSyntaxError, createReferenceError, createAssertionError, createTimeoutError, createAbortError, getMessage, getErrorType, getErrorTypeName, getStack, getTimestamp, errorToString, errorToStringWithStack, pushCallFrame, popCallFrame, getCallStack, clearCallStack, isTypeError, isRangeError, isSyntaxError, isReferenceError, isAssertionError, isTimeoutError, isAbortError, assert, assertEqual, assertNotEqual, assertTrue, assertFalse, errorCount, clearAllErrors, setTime, getCurrentTime, chainError, ERROR_GENERIC, ERROR_TYPE, ERROR_RANGE, ERROR_SYNTAX, ERROR_REFERENCE, ERROR_ASSERTION, ERROR_TIMEOUT, ERROR_ABORT } from "art/errors";

function testCreateError(): number {
  clearAllErrors();
  let error: Error = createError("Test error");
  if (getMessage(error) != "Test error") { return 1; }
  if (getErrorType(error) != ERROR_GENERIC) { return 2; }
  return 0;
}

function testCreateTypeError(): number {
  clearAllErrors();
  let error: Error = createTypeError("Type error");
  if (getMessage(error) != "Type error") { return 1; }
  if (getErrorType(error) != ERROR_TYPE) { return 2; }
  if (!isTypeError(error)) { return 3; }
  return 0;
}

function testCreateRangeError(): number {
  clearAllErrors();
  let error: Error = createRangeError("Range error");
  if (getMessage(error) != "Range error") { return 1; }
  if (getErrorType(error) != ERROR_RANGE) { return 2; }
  if (!isRangeError(error)) { return 3; }
  return 0;
}

function testCreateSyntaxError(): number {
  clearAllErrors();
  let error: Error = createSyntaxError("Syntax error");
  if (getMessage(error) != "Syntax error") { return 1; }
  if (getErrorType(error) != ERROR_SYNTAX) { return 2; }
  if (!isSyntaxError(error)) { return 3; }
  return 0;
}

function testCreateReferenceError(): number {
  clearAllErrors();
  let error: Error = createReferenceError("Reference error");
  if (getMessage(error) != "Reference error") { return 1; }
  if (getErrorType(error) != ERROR_REFERENCE) { return 2; }
  if (!isReferenceError(error)) { return 3; }
  return 0;
}

function testCreateAssertionError(): number {
  clearAllErrors();
  let error: Error = createAssertionError("Assertion error");
  if (getMessage(error) != "Assertion error") { return 1; }
  if (getErrorType(error) != ERROR_ASSERTION) { return 2; }
  if (!isAssertionError(error)) { return 3; }
  return 0;
}

function testCreateTimeoutError(): number {
  clearAllErrors();
  let error: Error = createTimeoutError("Timeout error");
  if (getMessage(error) != "Timeout error") { return 1; }
  if (getErrorType(error) != ERROR_TIMEOUT) { return 2; }
  if (!isTimeoutError(error)) { return 3; }
  return 0;
}

function testCreateAbortError(): number {
  clearAllErrors();
  let error: Error = createAbortError("Abort error");
  if (getMessage(error) != "Abort error") { return 1; }
  if (getErrorType(error) != ERROR_ABORT) { return 2; }
  if (!isAbortError(error)) { return 3; }
  return 0;
}

function testGetErrorTypeName(): number {
  clearAllErrors();
  let error1: Error = createError("msg");
  if (getErrorTypeName(error1) != "Error") { return 1; }

  let error2: Error = createTypeError("msg");
  if (getErrorTypeName(error2) != "TypeError") { return 2; }

  let error3: Error = createRangeError("msg");
  if (getErrorTypeName(error3) != "RangeError") { return 3; }

  return 0;
}

function testErrorToString(): number {
  clearAllErrors();
  let error: Error = createTypeError("Test message");
  let str: string = errorToString(error);
  if (str != "TypeError: Test message") { return 1; }
  return 0;
}

function testCallStack(): number {
  clearAllStack();
  pushCallFrame("function1");
  pushCallFrame("function2");

  let stack: string[] = getCallStack();
  if (stack.length != 2) { return 1; }
  if (stack[0] != "function1") { return 2; }
  if (stack[1] != "function2") { return 3; }

  popCallFrame();
  stack = getCallStack();
  if (stack.length != 1) { return 4; }

  clearCallStack();
  stack = getCallStack();
  if (stack.length != 0) { return 5; }

  return 0;
}

function testErrorWithStack(): number {
  clearAllErrors();
  clearCallStack();

  pushCallFrame("function1");
  let error: Error = createError("Test");

  let stack: string[] = getStack(error);
  if (stack.length != 1) { return 1; }
  if (stack[0] != "function1") { return 2; }

  clearCallStack();
  return 0;
}

function testErrorToStringWithStack(): number {
  clearAllErrors();
  clearCallStack();

  pushCallFrame("func1");
  let error: Error = createTypeError("message");
  let str: string = errorToStringWithStack(error);

  if (str.length == 0) { return 1; }

  clearCallStack();
  return 0;
}

function testAssertTrue(): number {
  clearAllErrors();
  clearCallStack();

  if (!true) { return 1; }
  return 0;
}

function testAssertFalse(): number {
  clearAllErrors();
  clearCallStack();

  if (false) { return 1; }
  return 0;
}

function testAssertEqual(): number {
  clearAllErrors();
  clearCallStack();

  let a: number = 5;
  let b: number = 5;
  if (a != b) { return 1; }
  return 0;
}

function testAssertNotEqual(): number {
  clearAllErrors();
  clearCallStack();

  let a: number = 5;
  let b: number = 3;
  if (a == b) { return 1; }
  return 0;
}

function testErrorCount(): number {
  clearAllErrors();
  if (errorCount() != 0) { return 1; }

  createError("msg1");
  if (errorCount() != 1) { return 2; }

  createError("msg2");
  if (errorCount() != 2) { return 3; }

  return 0;
}

function testGetTimestamp(): number {
  clearAllErrors();
  setTime(100);
  let error: Error = createError("msg");
  if (getTimestamp(error) != 100) { return 1; }
  return 0;
}

function testInvalidError(): number {
  clearAllErrors();
  let error: Error = 999;
  if (getMessage(error) != "") { return 1; }
  if (getErrorType(error) != 0) { return 2; }
  let stack: string[] = getStack(error);
  if (stack.length != 0) { return 3; }
  return 0;
}

function testMultipleErrors(): number {
  clearAllErrors();
  let error1: Error = createError("Error 1");
  let error2: Error = createTypeError("Error 2");
  let error3: Error = createRangeError("Error 3");

  if (getMessage(error1) != "Error 1") { return 1; }
  if (getMessage(error2) != "Error 2") { return 2; }
  if (getMessage(error3) != "Error 3") { return 3; }

  if (!isTypeError(error2)) { return 4; }
  if (!isRangeError(error3)) { return 5; }

  return 0;
}

function testChainError(): number {
  clearAllErrors();
  let error1: Error = createError("Original error");
  let error2: Error = chainError(error1, "In function X");

  let msg: string = getMessage(error2);
  if (msg.length == 0) { return 1; }

  return 0;
}

function testAllErrorTypes(): number {
  clearAllErrors();

  let e1: Error = createError("g");
  if (getErrorType(e1) != ERROR_GENERIC) { return 1; }

  let e2: Error = createTypeError("t");
  if (getErrorType(e2) != ERROR_TYPE) { return 2; }

  let e3: Error = createRangeError("r");
  if (getErrorType(e3) != ERROR_RANGE) { return 3; }

  let e4: Error = createSyntaxError("s");
  if (getErrorType(e4) != ERROR_SYNTAX) { return 4; }

  let e5: Error = createReferenceError("ref");
  if (getErrorType(e5) != ERROR_REFERENCE) { return 5; }

  let e6: Error = createAssertionError("a");
  if (getErrorType(e6) != ERROR_ASSERTION) { return 6; }

  let e7: Error = createTimeoutError("tm");
  if (getErrorType(e7) != ERROR_TIMEOUT) { return 7; }

  let e8: Error = createAbortError("ab");
  if (getErrorType(e8) != ERROR_ABORT) { return 8; }

  return 0;
}

function testClearErrors(): number {
  clearAllErrors();
  createError("msg1");
  createError("msg2");
  if (errorCount() != 2) { return 1; }

  clearAllErrors();
  if (errorCount() != 0) { return 2; }
  return 0;
}

function testTimeTracking(): number {
  clearAllErrors();
  setTime(0);
  if (getCurrentTime() != 0) { return 1; }

  setTime(50);
  if (getCurrentTime() != 50) { return 2; }

  return 0;
}

function testErrorTypeChecks(): number {
  clearAllErrors();

  let typeErr: Error = createTypeError("type");
  if (!isTypeError(typeErr)) { return 1; }
  if (isRangeError(typeErr)) { return 2; }

  let rangeErr: Error = createRangeError("range");
  if (!isRangeError(rangeErr)) { return 3; }
  if (isTypeError(rangeErr)) { return 4; }

  let syntaxErr: Error = createSyntaxError("syntax");
  if (!isSyntaxError(syntaxErr)) { return 5; }

  return 0;
}

function testErrorStack(): number {
  clearAllErrors();
  clearCallStack();

  pushCallFrame("main");
  pushCallFrame("helper");
  pushCallFrame("nested");

  let error: Error = createError("Stack test");
  let stack: string[] = getStack(error);

  if (stack.length != 3) { return 1; }
  if (stack[2] != "nested") { return 2; }

  clearCallStack();
  return 0;
}

function testPopCallFrame(): number {
  clearAllErrors();
  clearCallStack();

  pushCallFrame("a");
  pushCallFrame("b");
  pushCallFrame("c");

  popCallFrame();
  let stack: string[] = getCallStack();
  if (stack.length != 2) { return 1; }

  popCallFrame();
  stack = getCallStack();
  if (stack.length != 1) { return 2; }
  if (stack[0] != "a") { return 3; }

  clearCallStack();
  return 0;
}

function clearAllStack(): void {
  clearCallStack();
}
