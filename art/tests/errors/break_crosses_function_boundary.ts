// Expected error: `break` can't jump out of a nested function even
// though it's lexically inside a loop textually - same as real JS.
function main(): number {
  let fn: () => void = function(): void {
    break;
  };
  for (let i: number = 0; i < 3; i = i + 1) {
    fn();
  }
  return 0;
}
