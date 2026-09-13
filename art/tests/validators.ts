import { isNumber, isPositiveNumber, isNegativeNumber, isInteger, isInRange, isNotEmpty, isEmpty, isLengthInRange, isEmail, isURL, isAlphanumeric, isAlpha, isNumeric, startsWithUpperCase, startsWithLowerCase, hasUpperCase, hasLowerCase, hasDigit, hasSpecialChar, isEmptyArray, arrayContains, hasDuplicates, arrayEquals } from "art/validators";

function testIsNumber(): number {
  if (!isNumber(5)) { return 1; }
  if (!isNumber(0)) { return 2; }
  if (!isNumber(-3)) { return 3; }
  return 0;
}

function testIsPositiveNumber(): number {
  if (!isPositiveNumber(5)) { return 1; }
  if (isPositiveNumber(0)) { return 2; }
  if (isPositiveNumber(-1)) { return 3; }
  return 0;
}

function testIsNegativeNumber(): number {
  if (!isNegativeNumber(-5)) { return 1; }
  if (isNegativeNumber(0)) { return 2; }
  if (isNegativeNumber(1)) { return 3; }
  return 0;
}

function testIsInteger(): number {
  if (!isInteger(5)) { return 1; }
  if (isInteger(3.14)) { return 2; }
  return 0;
}

function testIsInRange(): number {
  if (!isInRange(5, 0, 10)) { return 1; }
  if (!isInRange(0, 0, 10)) { return 2; }
  if (isInRange(15, 0, 10)) { return 3; }
  return 0;
}

function testIsNotEmpty(): number {
  if (!isNotEmpty("hello")) { return 1; }
  if (isNotEmpty("")) { return 2; }
  if (isNotEmpty("   ")) { return 3; }
  return 0;
}

function testIsEmail(): number {
  if (!isEmail("test@example.com")) { return 1; }
  if (isEmail("invalid")) { return 2; }
  if (isEmail("@")) { return 3; }
  return 0;
}

function testIsURL(): number {
  if (!isURL("https://example.com")) { return 1; }
  if (!isURL("http://test.com")) { return 2; }
  if (isURL("not a url")) { return 3; }
  return 0;
}

function testIsAlphanumeric(): number {
  if (!isAlphanumeric("abc123")) { return 1; }
  if (isAlphanumeric("abc-123")) { return 2; }
  return 0;
}

function testIsAlpha(): number {
  if (!isAlpha("abcXYZ")) { return 1; }
  if (isAlpha("abc123")) { return 2; }
  return 0;
}

function testIsNumeric(): number {
  if (!isNumeric("123")) { return 1; }
  if (!isNumeric("3.14")) { return 2; }
  if (isNumeric("abc")) { return 3; }
  return 0;
}

function testStartsWithUpperCase(): number {
  if (!startsWithUpperCase("Hello")) { return 1; }
  if (startsWithUpperCase("hello")) { return 2; }
  return 0;
}

function testStartsWithLowerCase(): number {
  if (!startsWithLowerCase("hello")) { return 1; }
  if (startsWithLowerCase("Hello")) { return 2; }
  return 0;
}

function testHasUpperCase(): number {
  if (!hasUpperCase("Hello")) { return 1; }
  if (hasUpperCase("hello")) { return 2; }
  return 0;
}

function testHasLowerCase(): number {
  if (!hasLowerCase("Hello")) { return 1; }
  if (hasLowerCase("HELLO")) { return 2; }
  return 0;
}

function testHasDigit(): number {
  if (!hasDigit("abc123")) { return 1; }
  if (hasDigit("abc")) { return 2; }
  return 0;
}

function testIsEmptyArray(): number {
  if (!isEmptyArray([])) { return 1; }
  if (isEmptyArray([1])) { return 2; }
  return 0;
}

function testArrayContains(): number {
  let arr: number[] = [1, 2, 3];
  if (!arrayContains(arr, 2)) { return 1; }
  if (arrayContains(arr, 5)) { return 2; }
  return 0;
}

function testHasDuplicates(): number {
  if (!hasDuplicates([1, 2, 2, 3])) { return 1; }
  if (hasDuplicates([1, 2, 3])) { return 2; }
  return 0;
}

function testArrayEquals(): number {
  let arr1: number[] = [1, 2, 3];
  let arr2: number[] = [1, 2, 3];
  let arr3: number[] = [1, 2, 4];
  if (!arrayEquals(arr1, arr2)) { return 1; }
  if (arrayEquals(arr1, arr3)) { return 2; }
  return 0;
}
