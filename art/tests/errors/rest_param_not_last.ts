// Expected error: a rest parameter must be the last one.
function bad(...xs: number[], y: number): number {
  return y;
}

function main(): number {
  return 0;
}
