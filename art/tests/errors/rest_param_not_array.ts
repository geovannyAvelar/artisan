// Expected error: a rest parameter's own declared type must be an array.
function bad(...x: number): number {
  return x;
}

function main(): number {
  return 0;
}
