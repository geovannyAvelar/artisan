// Expected error: only an interface/class instance can be destructured -
// not a plain number.
function main(): number {
  let { x } = 5;
  return 0;
}
