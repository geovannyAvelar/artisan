// Expected error: arithmetic isn't allowed directly on an enum-typed
// value (only '=='/'!=' are) - deliberately not TS's "an enum is really
// just a number" interop, matching ART's "no implicit anything"
// philosophy instead. See EnumDecl's own doc comment.
enum Color { Red, Green, Blue }

function main(): number {
  let c: Color = Color.Red;
  return c + 1;
}
