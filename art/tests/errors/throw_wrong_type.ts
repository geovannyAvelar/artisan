// A throw with no enclosing try and no matching 'throws' on the
// enclosing function - rejected (checked exceptions: every function
// that can let an exception escape must declare it).
function main(): void {
  throw "just a string";
}
