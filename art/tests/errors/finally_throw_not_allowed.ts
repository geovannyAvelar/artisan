function bad(): number {
  try {
    doSomething();
  } finally {
    throw { message: "not allowed" };
  }
}

function doSomething(): void {}

function main(): number {
  return bad();
}
