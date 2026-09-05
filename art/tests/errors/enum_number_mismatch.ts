// Expected error: an enum is a real, distinct type from `number` - a
// bare number literal can't be assigned where an enum is expected.
enum Color { Red, Green, Blue }

function main(): number {
  let c: Color = 1;
  return 0;
}
