// Expected error: 'Color' has no member 'Purple'.
enum Color { Red, Green, Blue }

function main(): number {
  let c: Color = Color.Purple;
  return 0;
}
