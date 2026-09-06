// A catch whose declared type doesn't match what's actually thrown,
// with no other enclosing try/throws to catch it either - rejected
// (any type CAN be caught now - checked exceptions verify the actual
// type at each throw/catch site instead of restricting to one
// universal throwable type).
interface NotAnError {
  code: number;
}

function main(): void {
  try {
    throw "a string, not a NotAnError";
  } catch (e: NotAnError) {
    // wrong type - doesn't catch a thrown string
  }
}
