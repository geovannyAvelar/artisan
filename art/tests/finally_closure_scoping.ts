function main(): number {
  let result: number = 0;
  try {
    doSomething();
  } finally {
    let compute: () => void = function (): void {
      result = 99; // a return here would be fine too - it's this closure's own body, not the enclosing finally's
      return;
    };
    compute();
  }
  if (result != 99) { return 1; }
  return 0;
}

function doSomething(): void {}
