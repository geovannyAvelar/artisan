// Expected error: 'greet' expects at least 1 argument(s), got 0 - the
// leading, fixed parameter is still required even though the rest
// parameter after it accepts zero or more.
function greet(name: string, ...rest: string[]): string {
  return name;
}

function main(): number {
  greet();
  return 0;
}
