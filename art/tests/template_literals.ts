// Template literals: interpolation, escapes, multi-line, nesting, and
// brace-depth tracking around a nested interpolation.

function identity<T>(v: T): T { return v; }

interface Point { x: number; y: number; }

function main(): number {
  let fails: number = 0;

  let name: string = "world";
  let greeting: string = `hello, ${name}!`;
  if (greeting != "hello, world!") { fails = fails + 1; }

  let a: number = 2;
  let bNum: number = 3;
  let sum: string = `${a} + ${bNum} = ${a + bNum}`;
  if (sum != "2 + 3 = 5") { fails = fails + 1; }

  let plain: string = `just text`;
  if (plain != "just text") { fails = fails + 1; }

  let empty: string = ``;
  if (empty != "") { fails = fails + 1; }

  let escaped: string = `back\`tick and \${not interpolated}`;
  if (escaped != "back`tick and ${not interpolated}") { fails = fails + 1; }

  let multi: string = `line one
line two`;
  if (multi != "line one\nline two") { fails = fails + 1; }

  let inner: string = `inner`;
  let outer: string = `outer: ${`nested ${inner}`}`;
  if (outer != "outer: nested inner") { fails = fails + 1; }

  // A real '{'/'}' pair (an object literal, needing a generic call for
  // its target type) nested inside an interpolation - proves the
  // tokenizer's brace-depth tracking doesn't mistake it for the
  // interpolation's own closing brace.
  let braced: string = `x is ${numberToString(identity::<Point>({ x: 7, y: 8 }).x)}`;
  if (braced != "x is 7") { fails = fails + 1; }

  return fails;
}
