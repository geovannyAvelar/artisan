import { throwError, getErrorCode, getErrorMessage, clearError, hasError, assert, ERROR_VALUE, ERROR_TYPE, ERROR_RANGE } from "art/error";

function testThrowError(): number {
  clearError();
  throwError(ERROR_VALUE, "test error");
  if (getErrorCode() != ERROR_VALUE) { return 1; }
  if (getErrorMessage() != "test error") { return 2; }
  if (!hasError()) { return 3; }
  return 0;
}

function testClearError(): number {
  throwError(ERROR_VALUE, "error");
  clearError();
  if (hasError()) { return 1; }
  if (getErrorCode() != 0) { return 2; }
  return 0;
}

function testAssert(): number {
  clearError();
  assert(true, "should pass");
  if (hasError()) { return 1; }

  assert(false, "should fail");
  if (!hasError()) { return 2; }
  if (getErrorCode() != ERROR_VALUE) { return 3; }
  return 0;
}

function testErrorString(): number {
  let code: number = ERROR_TYPE;
  let msg: string = "test";
  if (msg == "") { return 1; }
  return 0;
}

function testMultipleErrors(): number {
  clearError();
  throwError(ERROR_VALUE, "first");
  if (getErrorCode() != ERROR_VALUE) { return 1; }

  throwError(ERROR_TYPE, "second");
  if (getErrorCode() != ERROR_TYPE) { return 2; }
  if (getErrorMessage() != "second") { return 3; }
  return 0;
}
