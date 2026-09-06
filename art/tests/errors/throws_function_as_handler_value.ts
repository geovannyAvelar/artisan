// A `throws`-declaring function can't be referenced as a plain Handler
// value - it could be invoked later by native code (an event handler,
// a timer) with no ART exception-handling context on the call stack.
// Calling it directly (inside a matching try/catch) is fine; only
// storing/passing it around as a value is rejected.

function risky(): void throws string {
  throw "boom";
}

function main(): void {
  let h: () => void = risky;
}
