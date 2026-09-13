import { sin, cos, tan, asin, acos, atan, log, log10, log2, exp, pow, sinh, cosh, tanh } from "art/math";

function testSin(): number {
  let s: number = sin(0);
  if (s < -0.1 || s > 0.1) { return 1; }

  let spi2: number = sin(1.5708);  // ~π/2
  if (spi2 < 0.9 || spi2 > 1.1) { return 2; }
  return 0;
}

function testCos(): number {
  let c: number = cos(0);
  if (c < 0.9 || c > 1.1) { return 1; }

  let cpi: number = cos(3.14159);  // ~π
  if (cpi > -0.9 || cpi < -1.1) { return 2; }
  return 0;
}

function testTan(): number {
  let t: number = tan(0);
  if (t < -0.1 || t > 0.1) { return 1; }
  return 0;
}

function testAsin(): number {
  let a: number = asin(0);
  if (a < -0.1 || a > 0.1) { return 1; }

  let a1: number = asin(1);
  if (a1 < 1.5 || a1 > 1.6) { return 2; }
  return 0;
}

function testAcos(): number {
  let a: number = acos(1);
  if (a < -0.1 || a > 0.1) { return 1; }
  return 0;
}

function testAtan(): number {
  let a: number = atan(0);
  if (a < -0.1 || a > 0.1) { return 1; }
  return 0;
}

function testLog(): number {
  let l: number = log(1);
  if (l < -0.1 || l > 0.1) { return 1; }

  let le: number = log(2.71828);  // ~e
  if (le < 0.9 || le > 1.1) { return 2; }
  return 0;
}

function testLog10(): number {
  let l: number = log10(1);
  if (l < -0.1 || l > 0.1) { return 1; }

  let l10: number = log10(10);
  if (l10 < 0.9 || l10 > 1.1) { return 2; }
  return 0;
}

function testLog2(): number {
  let l: number = log2(1);
  if (l < -0.1 || l > 0.1) { return 1; }

  let l2: number = log2(2);
  if (l2 < 0.9 || l2 > 1.1) { return 2; }
  return 0;
}

function testExp(): number {
  let e0: number = exp(0);
  if (e0 < 0.9 || e0 > 1.1) { return 1; }

  let e1: number = exp(1);
  if (e1 < 2.7 || e1 > 2.8) { return 2; }
  return 0;
}

function testPow(): number {
  if (pow(2, 3) != 8) { return 1; }
  if (pow(5, 0) != 1) { return 2; }
  if (pow(2, -1) < 0.4 || pow(2, -1) > 0.6) { return 3; }
  return 0;
}

function testSinh(): number {
  let s: number = sinh(0);
  if (s < -0.1 || s > 0.1) { return 1; }
  return 0;
}

function testCosh(): number {
  let c: number = cosh(0);
  if (c < 0.9 || c > 1.1) { return 1; }
  return 0;
}

function testTanh(): number {
  let t: number = tanh(0);
  if (t < -0.1 || t > 0.1) { return 1; }
  return 0;
}
