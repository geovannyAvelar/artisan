// Regression guard: makeArray<T> is a body-less internal builtin, NOT
// an isExtern declare function, even though both look identical under a
// naive "body == nullptr" check - a Handler-typed `fill` argument here
// must NOT go through the native-bridge {fn,env}-unpacking rule (see
// FunctionDecl::isExtern's own doc comment). art/tests/signals.ts
// already exercises this pattern incidentally; this pins it down
// directly and minimally.

function noop(): void {}

function main(): number {
  let fails: number = 0;

  let arr: () => void[] = makeArray::<() => void>(5, noop);
  if (arr.length != 5) { fails = fails + 1; }

  let i: number = 0;
  while (i < 5) {
    if (arr[i] != noop) { fails = fails + 1; }
    i = i + 1;
  }

  return fails;
}
