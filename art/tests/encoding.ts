import { encodeBase64, decodeBase64, encodeURL, decodeURL, encodeHex, decodeHex, isValidBase64, isValidURLEncoded } from "art/encoding";

function testEncodeBase64(): number {
  let encoded: string = encodeBase64("ABC");
  if (encoded.length == 0) { return 1; }
  return 0;
}

function testDecodeBase64(): number {
  let encoded: string = encodeBase64("test");
  let decoded: string = decodeBase64(encoded);
  if (decoded.length == 0) { return 1; }
  return 0;
}

function testEncodeURL(): number {
  let encoded: string = encodeURL("hello world");
  if (encoded.length == 0) { return 1; }
  if (encoded.indexOf("hello") == -1) { return 2; }
  return 0;
}

function testDecodeURL(): number {
  let encoded: string = encodeURL("test value");
  let decoded: string = decodeURL(encoded);
  if (decoded.length == 0) { return 1; }
  return 0;
}

function testEncodeHex(): number {
  let hex: string = encodeHex("A");
  if (hex.length == 0) { return 1; }
  return 0;
}

function testDecodeHex(): number {
  let hex: string = encodeHex("B");
  let decoded: string = decodeHex(hex);
  if (decoded.length == 0) { return 1; }
  return 0;
}

function testIsValidBase64(): number {
  if (!isValidBase64("AAAA")) { return 1; }
  if (isValidBase64("!@#$")) { return 2; }
  return 0;
}

function testIsValidURLEncoded(): number {
  if (!isValidURLEncoded("hello")) { return 1; }
  if (!isValidURLEncoded("hello%20world")) { return 2; }
  return 0;
}
