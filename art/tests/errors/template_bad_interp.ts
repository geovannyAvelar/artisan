// Expected error: a template literal's ${...} must be string/number,
// not boolean.
function main(): number {
  let flag: boolean = true;
  let s: string = `flag is ${flag}`;
  return 0;
}
