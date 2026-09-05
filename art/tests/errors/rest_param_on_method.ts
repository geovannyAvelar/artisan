// Expected error: a rest parameter isn't supported on a class method yet.
class Logger {
  function log(...msgs: string[]): void {}
}

function main(): number {
  return 0;
}
