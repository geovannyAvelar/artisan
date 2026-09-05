// Expected error: 'throw' requires an Error value - a bare string (or
// any other type) isn't throwable yet, only the one builtin Error type.
function main(): number {
  throw "just a string";
}
