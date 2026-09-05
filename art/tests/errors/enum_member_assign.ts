// Expected error: an enum member is always a real constant - it can't
// be reassigned, same as any other 'const'.
enum Color { Red, Green, Blue }

function main(): number {
  Color.Red = Color.Blue;
  return 0;
}
