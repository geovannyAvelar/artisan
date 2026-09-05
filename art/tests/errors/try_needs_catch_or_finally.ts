function bad(): number {
  try {
    doSomething();
  }
  return 0;
}

function doSomething(): void {}

function main(): number {
  return bad();
}
