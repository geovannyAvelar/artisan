function bad(): number {
  for (let i: number = 0; i < 3; i = i + 1) {
    try {
      doSomething();
    } finally {
      break; // targets the FOR loop outside the finally - not allowed
    }
  }
  return 0;
}

function doSomething(): void {}

function main(): number {
  return bad();
}
