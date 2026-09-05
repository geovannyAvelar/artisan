function bad(): number {
  try {
    doSomething();
  } finally {
    return 1; // not allowed - see art/README.md
  }
}

function doSomething(): void {}

function main(): number {
  return bad();
}
