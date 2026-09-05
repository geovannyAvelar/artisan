# ART

**A**rtisan **R**epresentation for **T**ype**S**cript — a small,
statically typed language, purpose-built as the app-logic layer for the
[artisan](../README.md) native desktop framework.

ART looks like TypeScript with the dynamic parts removed: no `any`, no
prototypes, no dynamic property access. It's compiled ahead of time to
native machine code via LLVM — not interpreted, not JIT'd — and linked
straight into an artisan binary alongside its JS counterpart. If you've
used TypeScript, ART should read as "the parts of TypeScript a native,
ahead-of-time compiler can make good on," nothing more.

```ts
class Counter {
  count: number;

  function increment(): void {
    this.count = this.count + 1;
  }
}

function onButtonClick(event: Event): void {
  let c: Counter = { count: 0 };
  c.increment();
  let label: Node = document.getElementById("count");
  if (!label.isNull()) {
    label.textContent = numberToString(c.count);
  }
}

// No `function setupApp()` wrapper needed - bare top-level statements
// become its body (see "Top-level statements" below). Wrapped in a
// block because it uses `document`, which a bare top-level `let` can't
// (that's always a persistent global instead).
{
  let button: Node = document.getElementById("my-button");
  if (!button.isNull()) {
    button.addEventListener("click", onButtonClick, false);
  }
}
```

## Quick start

The easiest way to use ART is through `artisan-cli`, which never
requires touching this directory directly:

```bash
artisan-cli new my-app
artisan-cli build my-app --run
```

That scaffolds `app.tsx`: a clean `import { Node, Event } from "art";`
plus a small JSX-driven counter, which is where your own code goes
(`.tsx` rather than plain `.ts` so it can use JSX - see [JSX
(.tsx)](#jsx-tsx) below - and `pages/index.html` is deliberately just a
bare mount point as a result). The DOM bridge itself (`declare`d and
wrapped in ergonomic classes - see [The DOM bridge](#the-dom-bridge)
below) is ART's own standard library ([`stdlib/art.ts`](stdlib/art.ts)),
not something copied into your project.

To build the `art` compiler itself and use it standalone (e.g. to
compile a `.ts` file with no artisan app around it at all):

```bash
cmake -S art -B art/build -GNinja
cmake --build art/build
art/build/art path/to/app.ts -o app          # compiles + links a native binary
art/build/art path/to/app.ts --emit-obj -o app.o  # object file only
art/build/art path/to/app.ts --emit-llvm -o app.ll  # LLVM IR, for inspection
```

Needs LLVM 18 (`llvm-18-dev` or equivalent) to build, and the
[Boehm-Demers-Weiser GC](https://www.hboehm.info/gc/) (`libgc-dev`) to
link a standalone binary (every ART allocation goes through it - see
[Memory management](#memory-management)). Neither is required to build
artisan itself unless an ART app is actually configured
(`ARTISAN_APP_ART_SOURCE`/`app.ts`/`app.tsx`) - a JS-only project never
pulls either in.

## Language guide

### Types

`number` (a double, same range/precision as real TypeScript's), 
`boolean`, `string` (UTF-8 bytes, `+` concatenation, `==`/`!=`,
`.length`, `s[i]` character indexing - immutable, no `s[i] = ...`), and
`T[]` arrays (`.length`, `arr[i]`/`arr[i] = v`, `for...of`), and `any`
(see [Dynamic typing](#dynamic-typing)). No GENERAL union types -
`T | null` is the one exception (see [Nullable types](#nullable-types))
- see [What's not in ART](#whats-not-in-art) for how code that would
reach for the rest gets by without them.

```ts
let n: number = 42;
let ok: boolean = true;
let s: string = "hello, " + "world";
let xs: number[] = [1, 2, 3];

for (let i: number = 0; i < xs.length; i++) {
  xs[i] = xs[i] * 2;
}
for (let x of xs) {
  // ...
}
```

`numberToString(n: number): string`/`stringToNumber(s: string): number`
are real builtins, not `declare function`s - available in any ART
program, DOM or not. `stringToNumber` parses a leading numeric prefix,
parseFloat-style (`stringToNumber("42px")` -> `42`); a string with no
numeric prefix at all returns `0`, not an error - see
[the main README](../README.md#using-art) for the full contract of
each, including libc's `%.15g`/`strtod` edge cases.

Template literals (`` `text ${expr}` ``) are sugar for exactly that
`+`/`numberToString` concatenation, not a distinct runtime feature - a
real expression, resolving to `string`:

```ts
let name: string = "world";
let greeting: string = `hello, ${name}!`; // "hello, world!"
let n: number = 5;
let msg: string = `n is ${n}`; // "n is 5" - numberToString, same as +
```

`${...}` accepts `string` or `number` only - the same "no implicit
stringification beyond `number`" rule JSX's own `{ expr }` children
already have (write your own conversion first for anything else, e.g.
a `boolean` via `cond ? "true" : "false"`). An interpolation can be an
arbitrarily complex expression, nested template literals and a
brace-using one (an object literal, if the surrounding call site gives
it a target type - e.g. a generic call's own argument) included: the
tokenizer tracks brace nesting per open interpolation to find *its own*
closing `}`, so an unrelated `{`/`}` pair inside one is never mistaken
for it. The literal itself can span multiple lines, and `` \` ``/`` \$
`` escape a literal backtick/`$` (so `` \${ `` doesn't start an
interpolation), alongside the same `\n`/`\t`/`\r`/`\\` escapes a plain
`"..."` string already has.

An array *literal* always spells out every element at compile time -
`makeArray<T>(size: number, fill: T): T[]`, a builtin (like
`numberToString`), is the way to allocate one whose length is only
known at runtime, filling every slot with `fill`:

```ts
let zeros: number[] = makeArray::<number>(10, 0);
```

`fill` is a single value stored into every slot - for a reference type
(a struct, another array, a `Handler`), every slot ends up pointing at
the *same* object, the same well-known gotcha a real `Array(n).fill(obj)`
has in JS.

`cond ? then : else` - the conditional (ternary) operator - is a real
expression, not a two-armed `if`/`else` in disguise: real branching plus
an LLVM `phi` node selecting the result, so only the taken branch's side
effects run (not an eagerly-evaluated `select`). Same precedence real
TS/JS gives it: lower than `||`, higher than `=` (`a || b ? c : d` is
`(a || b) ? c : d`; a bare ternary can't itself be an assignment
target). Right-associative, so it chains without parens: `a ? b : c ?
d : e` is `a ? b : (c ? d : e)`. No union types, so both branches must
resolve to the exact same type (not just something wide enough to cover
either, the way real TS/JS would widen to e.g. `number | string`) -
checked against an expected type when one's available (a `let`'s own
declared type, a function's return type, ...), otherwise inferred from
the `then` branch and required of `else` too.

```ts
let max: number = a > b ? a : b;
item.setStyle("color", wasSelected ? "" : "blue");
```

### Functions

```ts
function add(a: number, b: number): number {
  return a + b;
}
```

A bare function name (not a call) is a Handler value - `(params...) =>
void` - a plain, capture-free code address:

```ts
function onClick(): void { /* ... */ }
let handler: () => void = onClick;
```

A Handler value doesn't have to be a bare name, either - a variable or
array element holding one is callable too (`handler()`,
`handlers[i]()`), compiled as a real indirect call through the computed
function pointer. This is what makes something like a dynamic list of
event subscribers possible at all - see [Signals](#signals). A Handler
value can also be a real closure - see [Closures](#closures) below.

### Closures

`function(params): void { ... }` - the same syntax as a top-level
function declaration, minus the name - is a closure LITERAL, usable
anywhere a Handler value is expected: a `let` initializer, a call
argument, a class field, an array element, a return value. Unlike a
bare function name, it can reference ("capture") a local from
whichever function/closure it's written inside:

```ts
function makeCounter(label: Node): () => void {
  let count: number = 0;
  return function(): void {
    count = count + 1;
    label.textContent = numberToString(count);
  };
}
```

Capture is **by reference**, not a snapshot: two closures capturing the
same variable share one live binding, and each sees the other's writes.
```ts
let count: number = 0;
let increment: () => void = function(): void { count = count + 1; };
let read: () => void = function(): void {
  label.textContent = numberToString(count); // sees increment()'s writes
};
increment();
read(); // label now reads "1"
```
This is what makes per-item event handlers in a rendered list
straightforward (see [List rendering](#list-rendering) below) - each
item's closure captures its own index/id, no `data-*` attribute or
event-delegation trick needed.

A closure created inside a loop's **body** (a `let` there, or a
`for...of` loop variable) gets its own fresh capture every iteration,
matching real JS `let` semantics:

```ts
let handlers: () => void[] = makeArray::<() => void>(3, noopHandler);
let i: number = 0;
while (i < 3) {
  let idx: number = i; // fresh binding each iteration
  handlers[idx] = function(): void { labels[idx].textContent = numberToString(idx); };
  i = i + 1;
}
// handlers[0]() writes "0" into labels[0], handlers[1]() writes "1" into
// labels[1], and so on - each captured its OWN idx, not whichever value
// `i` ended up at.
```

**One accepted gap from real JS**: a classic `for (let k = 0; k < n;
k++)` loop-**header** counter does NOT get a fresh capture per
iteration - every closure created in the loop shares that one cell, so
by the time any of them run, `k` already holds its final value. Use a
`let` in the loop body instead (as above) - or `for...of`, which does
get correct per-iteration captures - whenever each iteration's closure
needs to see its own value.

Only a void-returning closure is legal (same restriction a bare
function reference already has). There's no explicit capture list -
every outer local/parameter the closure's body references is captured
automatically - and no support (yet) for a *named* nested function
declaration; only anonymous closure literals.

### Interfaces

A structural, heap-allocated struct - an object literal must match one
exactly (no excess or missing fields):

```ts
interface Point {
  x: number;
  y: number;
}

let p: Point = { x: 1, y: 2 };
```

### Classes

`class`/`declare class` add methods and `get`/`set` accessor properties
on top of what an interface already gives. A method call on a class
that stands alone (no `extends` anywhere in sight - the overwhelming
majority of classes) is pure call-site sugar, resolved statically
against the receiver's own declared type, compiling to a plain call
with the receiver spliced in as the real first argument - same as
always. A class actually touched by `extends` is different: see
[Inheritance](#inheritance) below for real virtual dispatch. `this` is
implicit, never written in a method's own parameter list; there's no
`new` - a class instance is built the exact same way an interface's
already is.

```ts
class Signal<T> {
  raw: T;

  get value(): T { return this.raw; }
  set value(v: T) { this.raw = v; }

  function reset(initial: T): void { this.raw = initial; }
}
```

`get`/`set` are contextual, not reserved words - recognized only in
class-body position by lookahead (`get`/`set`, another identifier,
`(`), so they stay ordinary, usable identifiers everywhere else. A
class's fields, plain methods, and accessor properties all share one
namespace: a getter and setter may share a name (forming one read/write
property); anything else colliding with an already-used name is a
compile error. A getter with no setter is read-only; a setter with no
getter is write-only. `obj.prop++`/`--` isn't supported on an accessor
property (only a plain field/element/variable) - reading through the
getter, adding one, and writing back through the setter would need a
distinct codegen path this doesn't build, so it's a clear compile error
instead of a silent miscompile.

`declare class` is the opaque counterpart (no accessible fields, same as
`declare type`) - for wrapping a `declare function`-based FFI surface
into ergonomic calls instead of raw `ArtDoSomething(handle, ...)`
C-style ones. This is exactly how the DOM bridge is exposed - see
[The DOM bridge](#the-dom-bridge).

### Inheritance

`class Dog extends Animal { ... }` - single inheritance, real virtual
dispatch: a method called through a base-typed reference reaches
whichever override the *actual* runtime instance has, not just whatever
was statically visible at the call site.

```ts
class Animal {
  name: string;
  function speak(): string { return this.name + " makes a sound"; }
}

class Dog extends Animal {
  function speak(): string { return this.name + " barks"; }
}

function announce(a: Animal): string {
  return a.speak(); // a Dog passed in here still barks, not "makes a sound"
}
```

A derived class's own fields are its base's own (recursively, for a
multi-level chain) followed by its own new ones, in that order - what
makes upcasting a `Dog` to an `Animal` a pure, free pointer
reinterpretation (the base's fields sit at the exact same offsets in
both layouts) rather than needing any real conversion, and also why a
derived class can't redeclare a name from anywhere in its base chain
(no field shadowing). A derived instance is assignable anywhere its
base is expected - a variable, a parameter, a return value, an array/
struct element - the one new kind of assignability inheritance adds (a
plain interface/class type otherwise still needs an *exact* match, no
implicit widening beyond this).

Only a class actually touched by `extends` - has a base, or is one for
something else - pays for any of this: it alone gets a vtable pointer
prepended to its own compiled layout, and only ITS methods compile to
an indirect call through that pointer instead of a direct one. A class
that stands alone is completely unaffected - same struct layout, same
direct-call codegen as before this feature existed at all, which is
also why virtual dispatch could be added without touching how a single
existing (non-inheriting) class already worked.

An override must match its ancestor's own signature exactly (same
parameter types, same return type) - ART has no covariance/
contravariance rules to make good on a mismatched one. `get`/`set`
accessors and static members inherit (a derived class can read/write an
inherited property, or call `Base.staticMethod()` by name) but can't be
overridden yet - only a plain instance method participates in virtual
dispatch in this first pass.

**Real limits, not oversights**: no `implements`, no multiple
inheritance, and - the big one - no downcasting or `instanceof`. Real
`instanceof`/an unchecked cast both need runtime type identification (a
type tag actually carried on the object, checked at the cast site) -
ART has none yet, the same reason a `catch` clause can only catch one
fixed type (see [Exceptions](#exceptions) above). Upcasting alone (the
safe direction - a Dog is unconditionally usable as the Animal it
already is) needs no such thing, which is exactly why it's the one
piece implemented so far.

### Generics

Functions, interfaces, and classes can all take type parameters -
monomorphized (like C++ templates, not type erasure), explicit
instantiation only, one real, independently-checked, separately-compiled
copy generated per distinct type actually used:

```ts
function identity<T>(x: T): T { return x; }
let n: number = identity::<number>(5); // '::<T>' - a *call* site

interface Box<T> { value: T; }
let b: Box<number> = { value: 5 }; // plain '<T>' - a *type reference*

class Wrapper<T> {
  raw: T;
  get value(): T { return this.raw; }
}
let w: Wrapper<string> = { raw: "hi" };
```

`::<T>` at a call site vs. plain `<T>` at a type reference isn't
arbitrary: the parser runs before type checking and can't yet know
whether a name is generic, so a bare `<` after a call target would be a
genuine parse-time ambiguity with the `<`/`>` comparison operators. A
type reference can never start where a comparison expression could, so
no such ambiguity exists there. No type inference anywhere - the
argument list (or type reference) is always explicit. A method/accessor
can't be individually generic yet - only the class itself can.

### Modules

`export` in front of a function/interface/class/`declare` counterpart/
top-level `let`/`const`; `import { a, b } from "path";` at the top of a
file (`.ts` implied). `path` is either relative (`./`/`../`) - resolved
against the importing file's own directory - or bare - resolved against
ART's standard library instead (see [The DOM bridge](#the-dom-bridge)'s
`import { Node, Event } from "art";`). Access control only, not real
per-file namespacing - every top-level name across a whole project must
still be globally unique.

```ts
// math.ts
export function add(a: number, b: number): number { return a + b; }

// app.ts
import { add } from "./math";
function main(): number { return add(1, 2); }
```

The whole file graph reachable from the entry point is resolved and
merged into one program before type checking - missing files, missing/
private exports, and circular imports are all caught up front, each
reported with the real file and line involved. A file with no `import`s
at all is completely unaffected by any of this - single-file programs
compile exactly as they always have.

### Top-level state

A `let`/`const` at the top level can hold any type with any initializer
- a call, an object/array literal, another (earlier) global, a bare
function reference. A bare number/boolean/string literal is a real
compile-time constant; anything else is computed once, in declaration
order, by a small generated function that runs before `main`/`setupApp`
(an LLVM module constructor, the same mechanism the garbage collector's
own initialization uses). A later global can reference an earlier one
but not the reverse - ordinary "declare before use" semantics.

```ts
interface Config { retries: number; }
let config: Config = { retries: 3 };  // an object literal - real code, run once
let doubled: number = config.retries * 2;  // references an earlier global
```

### Top-level statements

A top-level *statement* (an `if`, `while`, bare call, or block - not a
declaration or a `let`/`const`) doesn't declare anything, so there's
nothing to persist: it's collected, across every file in the merged
program (dependency-first order, same as globals), into the body of a
generated `setupApp` - the procedural alternative to writing `function
setupApp(): void { ... }` explicitly. Both produce the exact same
`setupApp` C symbol main.cpp's trampoline already calls once per page
load; a project just can't have both (a clear compile error, not a
silent pick-one).

```ts
addToTotal(10);
if (total > 5) { addToTotal(20); }
```

The two lists (persistent globals vs. per-call top-level statements)
exist because they're genuinely different lifetimes, not two ways to
write the same thing - and writing a bare top-level `let` where you
meant a per-run local fails loudly: a global that directly calls the
ambient `document` (or anything backed by it) is a compile error, since
a global initializes once, before any page has ever loaded - not the
narrow "no page loaded yet" window `.isNull()` normally covers
elsewhere, but the permanent state at that point. Wrap it in a block to
get a real local instead, re-run fresh every `setupApp` call:

```ts
{
  let button: Node = document.getElementById("my-button");
  if (!button.isNull()) { button.addEventListener("click", onButtonClick, false); }
}
```

(This check only catches a *direct* call in a global's own initializer,
not one reached indirectly through another function it calls into - a
narrower guarantee than "provably safe", but enough to catch the
obvious mistake.)

## The DOM bridge

`Node` and `Event` are opaque foreign handles (`declare class`) wrapping
a curated subset of artisan's own C++ DOM API
([`include/node_c_api.h`](../include/node_c_api.h), re-exposed with
ART-ABI-compatible signatures in
[`include/art_bridge.h`](../include/art_bridge.h)), named and shaped to
match a real browser's own DOM API as closely as possible: `get`/`set`
accessors wherever a browser would use a plain property
(`node.textContent`, `event.key`), ordinary methods everywhere a
browser has an actual method (`getElementById`, `setAttribute`,
`addEventListener`, `createElement`, `appendChild`, ...). All of this -
the raw `declare function`s and both classes - lives in ART's own
standard library ([`stdlib/art.ts`](stdlib/art.ts)), not any one
project: `import { Node, Event } from "art";` pulls it in from anywhere
(a bare, non-relative import path - see [Modules](#modules) - resolves
against this directory, baked into the `art` binary at its own build
time). `export`ed from there: `Node`/`Event`, and the two generic
functions below for a project that uses custom events.

```ts
function setupApp(): void {
  let button: Node = document.getElementById("my-button");
  if (!button.isNull()) {
    button.addEventListener("click", onButtonClick, false);
  }
}
```

`document` is ambient - always just there, the same way a real
browser's own global `document` is. It isn't a real variable or a
declared function; it's pure call-site sugar for `ArtDocument()`,
rewritten in place at compile time, and a project that declares its own
`document` shadows the sugar entirely, the same precedence a real local
always has over anything ambient. It exists specifically so a handler -
which gets no `Node` parameter of its own, unlike `setupApp` - has a
path to the DOM at all. Unlike `Node`/`Event`, `ArtDocument` needs no
`export`/`import` to keep working across files - the sugar's rewritten
call skips Sema's usual per-file visibility check entirely, since an
ambient identifier gated behind an import wouldn't really be ambient.

`.isNull()` is the only way to test a lookup for "no match" - ART has no
null literal of its own to compare against. The DOM bridge itself never
throws - `ArtIsNull`/`.isNull()` is the whole story for a failed lookup
here - but ART does now have real exceptions elsewhere (see
[Exceptions](#exceptions) below); `.isNull()` predates them and was
never redone in terms of them, since a bridge-lookup failure isn't
exceptional the way an actually-thrown error is.

`classListAdd`/`classListRemove`/`classListContains`/`classListToggle`
manage the `"class"` attribute's space-separated tokens - flattened
directly onto `Node` (`item.classListAdd("selected")`), not a nested
`classList` object, since there's no separate handle for it at the C API
level either. `classListToggle(name, hasForce, force)` covers real
`classList.toggle(name, force)`'s two shapes with ART's always-required
parameters: `classListToggle(name, false, false)` is a plain toggle;
`classListToggle(name, true, force)` pins membership to `force`.
`getStyle`/`setStyle` read/write one inline `style="..."` property at a
time (`color`, `backgroundColor`, `fontWeight`, `borderColor`,
`borderWidth` - the same five a `<style>` block supports); `""` means
unset, and setting `""` removes the property.

```ts
function onItemClick(event: Event): void {
  let item: Node = event.target;
  let wasSelected: boolean = item.classListContains("selected");
  item.classListToggle("selected", false, false);
  if (wasSelected) { item.setStyle("color", ""); } else { item.setStyle("color", "blue"); }
}
```

`ArtDispatchEvent<T>`/`ArtEventDetail<T>` are the one part of the bridge
still called as free functions, not methods - a method/accessor can't be
generic yet (see [Generics](#generics)). `T` can be `number`, `boolean`,
or `string`; the bridge provides a real, separately compiled pair for
each.

## Timers

`setTimeout`/`setInterval`/`clearTimeout`/`clearInterval`/
`requestAnimationFrame`/`cancelAnimationFrame` - plain global functions
(`import { setTimeout, ... } from "art";`), not `Node` methods, matching
real JS/a browser: there's no natural receiver for a timer the way there
is for DOM mutation.

```ts
import { Node, setTimeout, clearInterval, setInterval } from "art";

let seconds: number = 0;
let intervalId: number = 0;

function onTick(): void {
  seconds = seconds + 1;
  let label: Node = document.getElementById("timer");
  if (!label.isNull()) { label.textContent = numberToString(seconds); }
}

function stopAfter10s(): void {
  clearInterval(intervalId);
}

{
  intervalId = setInterval(onTick, 1000);
  setTimeout(stopAfter10s, 10000);
}
```

`setTimeout(callback: () => void, delayMs: number): number` runs
`callback` once, after at least `delayMs`; `setInterval` runs it
repeatedly, rescheduling from the moment it actually fired (not the
moment it was originally due, so a late-running frame can't trigger a
catch-up burst). Both return an id `clearTimeout`/`clearInterval` can
cancel with - either cancels either kind, the same single entry point
real `clearTimeout`/`clearInterval` each are underneath.
`requestAnimationFrame(callback: (timestamp: number) => void): number`
runs `callback` once with the current timestamp just before the next
repaint (not a fixed delay); calling it again from inside `callback` -
the normal way to drive a continuous animation - schedules for the
*next* repaint, never re-runs within the current one.
`cancelAnimationFrame` cancels a still-pending id.

Every id here is a real `double` on the C++ side of the bridge
(`art_bridge.h`), not the `int` `TimerQueue`/`AnimationFrameQueue` use
internally - ART's `number` is always a C ABI `double` (see
`art/codegen.cpp`'s `MapType`), so a `declare function` returning or
taking `number` needs an actual `double` at the boundary, or the
mismatched calling convention (an integer register vs. an XMM one)
hands back or reads garbage. This is exactly the bug a first pass at
this feature shipped with: `setTimeout`/`setInterval` appeared to work
in casual testing (firing doesn't depend on the id being read back
correctly), but `clearTimeout`/`clearInterval` silently canceled
nothing, since the id they received was garbage - only caught by
scheduling a timer, clearing it immediately, and then actually
advancing the clock far enough to prove it never fired.

## JSX (.tsx)

A `.tsx` file (only - see below) can build a `Node` with an element
literal instead of `document.createElement`/`.setAttribute`/
`.appendChild` calls:

```tsx
function badge(text: string): Node {
  return <span class="badge">{text}</span>;
}
```

is exactly

```ts
function badge(text: string): Node {
  let n: Node = document.createElement("span");
  n.setAttribute("class", "badge");
  n.appendChild(document.createTextNode(text));
  return n;
}
```

- **A real expression**, not restricted to statement position - usable
  anywhere a `Node` is expected (a `let` initializer, a function's
  `return`, another JSX element's own `{ ... }` child slot, ...), the
  same way an object/array literal already builds a value through a
  sequence of instructions before yielding it (see
  `Codegen::GenExpr`'s own `JsxElement` case).
- **Attributes** (`name="literal"` or `name={expr}`) are plain HTML
  attribute names, not React's `className`/camelCase - `class`, not
  `className`; `data-index`, not `dataIndex`. Every value must resolve
  to `string` - no implicit stringification of a `number` (write
  `numberToString(x)` yourself, same rule as everywhere else in ART).
  `on<type>` (`onclick`, `onkeydown`, ...) is the one special case:
  its value must be a `(event: Event) => void` handler instead, and it
  desugars to `.addEventListener(type, handler, false)` - always
  non-capturing, the same simplicity real JSX/React's `onClick={...}`
  has (call `.addEventListener` yourself afterward for `capture: true`).
- **Children** are nested JSX elements, or a `{ expr }` interpolation:
  a `Node`-valued one is appended as-is, a `string`/`number` one becomes
  a text node (`numberToString` first, for a number), and a `Node[]`
  one is *spread* - each element appended in its own right, via a real
  runtime loop (the array's length is only known at runtime, same shape
  a `for...of` loop's own codegen already has) - the way a dynamically-
  sized list of children gets into JSX at all, since a fixed `{a}{b}`
  list can only ever hold however many children the source literally
  spells out. Anything else (a `boolean`, or a `T[]` of some other T)
  is a compile error, not a silent stringify. There's deliberately no
  bare/unquoted text between tags (`<div>Hello</div>` doesn't parse,
  `<div>{"Hello"}</div>` does): ART's tokenizer lexes a whole file into
  one flat token stream up front with no "raw text" mode to switch into
  mid-parse, so text content has to arrive as an ordinary string
  expression instead.

  ```tsx
  function renderList(labels: string[]): Node {
    let items: Node[] = makeArray::<Node>(labels.length, <li></li>);
    let i: number = 0;
    while (i < labels.length) {
      items[i] = <li data-index={numberToString(i)}>{labels[i]}</li>;
      i = i + 1;
    }
    return <ul>{items}</ul>; // spread - however many labels there are
  }
  ```
- **Attribute/tag names can be hyphenated** (`data-index`, `aria-label`,
  even a hyphenated custom-element tag) and can collide with an ART
  keyword (`class`, `for`, `type`, ...) - the parser reassembles
  `identifier ('-' identifier)*` from the separate tokens the tokenizer
  actually produces (there's no hyphenated-identifier token), and
  accepts a keyword's own raw spelling anywhere a JSX name is expected.
- **Self-closing** (`<br />`) and a required, exactly-matching closing
  tag (`<div>...</div>`, never left implicit) are both supported; a
  mismatched or missing closing tag is a compile error naming both tags.
- **Fragments** (`<>...</>`) have no tag name, no attributes, and no
  wrapping element - just its children, resolving to `Node[]` instead
  of `Node`, so a helper can return several sibling nodes without an
  unwanted wrapper tag. Nesting one inside another element's own
  children spreads it there for free, the same as any other `Node[]`
  child (see above) - a fragment is just a `Node[]`-producing
  expression, nothing more special than that.

  ```tsx
  function headerAndFooter(): Node[] {
    return <>
      <li class="header">{"header"}</li>
      <li class="footer">{"footer"}</li>
    </>;
  }

  function renderList(labels: string[]): Node {
    let items: Node[] = makeArray::<Node>(labels.length, <li></li>);
    let i: number = 0;
    while (i < labels.length) {
      items[i] = <li>{labels[i]}</li>;
      i = i + 1;
    }
    return <ul>{headerAndFooter()}{items}</ul>; // both spread, in order
  }
  ```

Why `.tsx` only, not JSX inline in any `.ts` file: a bare `<` is
otherwise always the start of a `<`/`<=` comparison (there's no other
legal expression it could begin), so enabling JSX parsing is genuinely
unambiguous everywhere - but it's still gated on the file extension
(`Parser::jsxEnabled`, set from the file's own path in main.cpp/
module_resolver.cpp) rather than always on, so an ordinary `.ts` file's
stray `<` keeps getting the exact same "expected an expression" error
it always has, never a confusing JSX-flavored one. A relative `import`
with no explicit extension tries `.ts` first, falling back to `.tsx`,
so a `.tsx` component file can still be imported without spelling out
which it is.

## Signals

A `Signal<T>` is a reactive value: reading `.value` inside an `effect()`
automatically subscribes that effect to it, and writing `.value`
automatically re-runs every effect that ever read it - `.unsubscribe(fn)`
removes one later, a safe no-op if `fn` was never subscribed (or already
removed). This isn't a compiler feature - it's an ordinary generic
class, built entirely from generics, `get`/`set` accessors, non-literal
globals, indirect calls, and runtime-sized arrays (a signal's own
subscriber list genuinely grows, via `makeArray<T>` - see
[Types](#types) above).

```ts
let clickCount: Signal<number> = makeSignal::<number>(0);

function updateLabel(): void {
  let label: Node = document.getElementById("count");
  if (!label.isNull()) { label.textContent = numberToString(clickCount.value); }
}

function onButtonClick(event: Event): void {
  clickCount.value = clickCount.value + 1; // updateLabel re-runs on its own
}

function setupApp(): void {
  effect(updateLabel); // binds the label to clickCount, once, for good
  let button: Node = document.getElementById("increment-button");
  if (!button.isNull()) { button.addEventListener("click", onButtonClick, false); }
}
```

`onButtonClick` never touches the DOM. See
[the main README's "Signals" section](../README.md#signals) for the
full `Signal`/`makeSignal`/`effect` source, ready to copy into a
project as its own module.

### List rendering

A `Signal<T[]>` plus a "clear and rebuild" effect keeps a DOM list in
sync with data with no diffing needed. Each rendered item can now carry
its own bound handler - a closure capturing that item's own index -
instead of needing event delegation:

```ts
function renderList(): void {
  let container: Node = document.getElementById("item-list");
  if (container.isNull()) { return; }
  while (container.childCount() > 0) { container.childAt(0).remove(); } // clear
  let xs: number[] = items.value; // subscribes renderList to items
  let i: number = 0;
  while (i < xs.length) { // rebuild
    let idx: number = i; // captured below - fresh binding each iteration
    let li: Node = document.createElement("li");
    li.textContent = numberToString(xs[idx]);
    li.addEventListener("click", function(event: Event): void {
      items.value = removeAt(items.value, idx); // re-runs renderList on its own
    }, false);
    container.appendChild(li);
    i = i + 1;
  }
}
```

Event delegation (one listener on the *container*, resolving
`event.target` back to the clicked item via a `data-index` attribute) is
still a valid, sometimes simpler choice - especially if a list is large
enough that registering one listener per item matters, since delegation
needs exactly one registration no matter how many items there are:

```ts
function onItemClick(event: Event): void {
  let target: Node = event.target;
  let xs: number[] = items.value;
  let i: number = 0;
  while (i < xs.length) {
    if (target.getAttribute("data-index") == numberToString(i)) {
      items.value = removeAt(xs, i);
      return;
    }
    i = i + 1;
  }
}
```

Removing the very node a click is bubbling through (the clear step tears
down the whole subtree, including whichever `<li>` was just clicked) is
safe either way: `.remove()` only detaches a node from its parent, it
never frees it mid-dispatch, so the ongoing bubbling walk keeps working
off pointers that are still valid, just no longer attached to anything.
See [the main README's "List rendering" section](../README.md#list-rendering)
for the full worked example, including `appendNumber`/`removeAt` and the
wiring to an "add" button.

## Memory management

Every heap allocation - arrays, strings, interfaces, classes - is
garbage-collected by the Boehm-Demers-Weiser collector. ART code never
explicitly frees anything (there's no `delete`/`free` in the language),
but unreachable allocations do get reclaimed automatically. The
collector is conservative: it scans the native stack/registers/globals
for anything that looks like a pointer into its own heap rather than
tracking types precisely, so it can - rarely - retain a little garbage a
precise collector wouldn't, but it never frees something still
reachable.

A heap-allocated ART value handed across the FFI boundary (e.g. an
`ArtString*` passed into a `declare function`) is safe without any
extra bookkeeping: the bridge always copies it into artisan's own
(unrelated) memory before doing anything that could outlive the call.

## Exceptions

`try { ... } catch (name: Error) { ... }` and `throw expr;` are real,
non-local control flow - a `throw` deep inside a call stack jumps
directly to the nearest enclosing `catch`, skipping every ordinary
`return` in between, exactly like real TS/JS.

```ts
function mightFail(x: number): number {
  if (x < 0) { throw { message: "x must be non-negative" }; }
  return x * 2;
}

function main(): number {
  try {
    return mightFail(-1);
  } catch (e: Error) {
    let label: Node = document.getElementById("error");
    if (!label.isNull()) { label.textContent = e.message; }
    return 0;
  }
}
```

Two real, deliberate limits on this first version, both because
generalizing them needs infrastructure ART doesn't have yet:

- **`Error` (`{ message: string }`) is the only throwable/catchable
  type.** `throw` always needs one; `catch (name: Type)` must always
  write `Error` as `Type`. Real, unrestricted polymorphic catching
  (`catch` selecting among several different thrown types by their
  actual runtime type) needs runtime type identification - ART has none
  yet (no `any`, no dynamic dispatch - see
  [What's not in ART](#whats-not-in-art)) - so with exactly one
  throwable type in flight, every active handler always matches
  whatever's thrown, and there's no type-tag check to get wrong. `Error`
  is a real, ordinary struct otherwise (built the normal way -
  `{ message: "..." }`), not compiler magic beyond being the one thing
  `throw`/`catch` recognize.
- **No `finally` yet.** Its own codegen has to guarantee running on
  every exit path out of a `try` - normal completion, an exception this
  same `try` catches, one that propagates past it uncaught, *and* an
  early `return`/`break`/`continue` fired from inside the `try` - a
  distinct, substantial piece of work on top of the mechanism below.

Mechanism: `setjmp`/`longjmp`, called directly (never through a wrapper
function - `setjmp` only captures a resumable state for the function
that calls it directly), not LLVM's own Itanium-ABI `invoke`/
`landingpad` machinery real C++ exceptions use. This is the right
choice specifically *for ART*, not a shortcut: the usual reason that
machinery is so complex is running destructors correctly for every
stack frame being unwound past, and ART has no destructors at all -
everything is GC'd (see [Memory management](#memory-management)) - so
none of that complexity buys anything here. A global stack of
handler-frame structs (one `jmp_buf` plus a link to the next-outer
active one, stack-allocated per dynamically-active `try` - correct even
under recursion, since `alloca` allocates fresh space on every real call
into the function that owns it) rooted at one module-global pointer is
the entire runtime; `throw` walks it, `try` pushes/pops it, all as
hand-generated LLVM IR (see `Codegen::GenStmt`'s own `Try`/`Throw`
cases), the same "no separate C++ runtime file, the standalone `art`
compiler stays self-contained" approach `makeArray<T>` already uses.

An uncaught `throw` - no active handler anywhere - calls `abort()`,
matching real JS's "an uncaught exception terminates the program" (no
message is printed yet).

A `return`/`break`/`continue` that exits a `try` body early has to
correctly restore the handler stack on its way out, or a *later*,
completely unrelated `throw` could `longjmp` into a stack frame that's
already been reused elsewhere - real memory corruption, not a cosmetic
bug. This is handled uniformly (see `Codegen::ScopeExit`/
`GenExceptionHandlerCleanup`): the same stack that already tracks
`break`/`continue` targets for loops/`switch` also tracks every
currently-active `try` frame, in real nesting order, so escaping past
any number of nested ones - crossing loop, `switch`, and `try`
boundaries all at once - still costs exactly one restore.

## Nullable types

`T | null` is ART's one and only union shape - not general `T1 | T2`
unions (see [What's not in ART](#whats-not-in-art)), just this one
specific pattern, matching the single most common use of union types in
real TS code.

```ts
function findUser(id: number): User | null {
  if (id == 0) { return null; }
  return notNull::<User>({ id: id, name: "..." });
}

function greet(id: number): string {
  let user: User | null = findUser(id);
  if (user != null) {
    return "hello, " + user.name; // `user` is a plain `User` here
  }
  return "no such user";
}
```

`null` is a real literal, but only where an expected `T | null` type is
already known (a `let`'s own declared type, a parameter, a return type)
- like an empty array literal, there's nothing to infer a type *from* in
`null` on its own. `notNull<T>(v: T): T | null` is the one and only way
to produce a *present* `T | null` value from a plain `T` - there's no
implicit widening anywhere (no assigning a plain `T` where `T | null` is
expected without going through it), trading a bit of ceremony at
call sites for never having to insert an implicit boxing coercion at
every assignment/parameter/return site in Codegen.

**Narrowing** - using a `T | null` value as a plain `T` - is recognized
in exactly two shapes, both requiring the checked value to be a bare
local variable (not `obj.field`, not a function call):

- `if (x != null) { /* x is T here */ }` - narrowed for the `then`
  branch only.
- `if (x == null) { <this branch always returns/throws/breaks/
  continues> }` - with no `else` - narrows `x` to `T` for the rest of
  the enclosing block, since the only way past the `if` is through that
  narrowing already having been proven true.

Reassigning `x` anywhere inside its narrowed region - even back to a
provably non-null value - invalidates the narrowing for the rest of
that region: the compiler doesn't re-verify the new value, so it
conservatively goes back to treating `x` as `T | null` and requires
narrowing it again before further plain-`T` use.

Representation: every `T | null`, regardless of `T`'s own natural shape
(a `double`, an `i1`, a pointer), is a small boxed heap cell holding
exactly one `T` - `null` is a real null pointer to that box, "present"
is a non-null pointer to a box holding the value. This uniform
representation is what makes narrowing's own codegen a single, small
piece of logic (one extra load, in exactly one place -
`Codegen::GenExpr`'s `Identifier` case) rather than needing a distinct
strategy per underlying type.

Two real, deliberate limits, same "ceremony over generality" trade the
rest of this section makes:

- **No arrays of nullable element type.** `(T | null)[]` needs
  parenthesized types in element position, and ART's type grammar
  doesn't have those - `T | null` only appears as a whole declared
  type (a variable, parameter, or return type), never nested inside
  another type. A function returning `T | null` per lookup, called
  once per element, gets the same result without needing the array
  itself to hold nullable slots.
- **No narrowing through fields or function calls.** `if (obj.field !=
  null)` or `if (getValue() != null)` don't narrow anything - only a
  bare local variable's own identifier is recognized, so `obj.field`
  must be assigned to a local first if it needs narrowing.

## Dynamic typing

`any` is real, TS-style dynamic typing - a value that carries its own
runtime type tag and accepts anything, checked (never silently trusted)
via `typeof`:

```ts
function describe(x: any): string {
  if (typeof x == "number") { return "a number: " + numberToString(x); }
  if (typeof x == "string") { return "a string: " + x; }
  if (typeof x == "boolean") { return x ? "true" : "false"; }
  return "something else: " + typeof x;
}

function main(): number {
  let n: any = 42;
  let s: any = "hello";
  if (describe(n) != "a number: 42") { return 1; }
  if (describe(s) != "a string: hello") { return 1; }
  return 0;
}
```

Assigning a plain, concrete value where `any` is expected (a `let`'s own
declared type, a parameter, a return type, an array/struct element)
widens IMPLICITLY - no ceremony, unlike `T | null`'s own `notNull<T>`
requirement (see [Nullable types](#nullable-types)): that ceremony-free
widening is `any`'s entire reason to exist, matching real TS exactly.
`null` widens into `any` too, same as real TS/JS.

Getting a concrete value back OUT needs `typeof x === "..."` narrowing
first - there's no implicit narrowing the other direction, same "prove
it first" discipline `T | null` already has. Narrowing recognizes
exactly the same two condition shapes `T | null` does (see its own
section above for the full "one bare local variable, checked one way,
not compound conditions" scope), just testing `typeof x` against a
string literal instead of comparing `x` to `null`:

- `if (typeof x === "...") { /* x is that concrete type here */ }` -
  narrowed for the `then` body only.
- `if (typeof x !== "...") { <always exits> }` - with no `else` -
  narrows `x` for the rest of the enclosing block.

(ART's `==`/`!=` already mean strict, no-coercion equality - see
[Types](#types) - so there's no separate `===`/`!==` to write; plain
`==`/`!=` are what real TS's `===`/`!==` would be here.) Reassigning `x`
anywhere inside its narrowed region invalidates the narrowing for the
rest of that region, exactly the same real hazard (and the same fix)
`T | null`'s own narrowing has - the compiler doesn't re-verify a new
value, so it conservatively requires re-narrowing before further
concrete-typed use.

Representation: every `any` is a boxed `ptr` - a fresh GC cell `{ tag,
payload }`, `tag` saying what's actually in it right now. A
number/boolean gets its own tiny heap cell (neither has a spare
pointer-shaped bit pattern to reuse); a string/struct/array/function
reuses its own already-`ptr`-shaped value directly as `payload`, no
extra allocation.

Two real, deliberate limits, both because generalizing them needs
runtime type identification ART doesn't have (see "What's not in ART"'s
own "no downcasting or instanceof" bullet) - `any` can always hold
either of these, `typeof` always correctly reports them, but neither
narrows back out to anything usable:

- **`typeof x === "object"` never narrows.** Both a struct instance and
  an array report `"object"` (indistinguishable via `typeof` alone,
  exactly like real JS's own `typeof [1, 2, 3] === "object"`), and even
  restricted to just structs, `any` alone can't say WHICH struct type
  was boxed - there could be any number of candidates.
- **`typeof x === "function"` never narrows either.** `any` erases
  exactly which handler signature (`(x: number) => void` vs `() =>
  void`, say) was boxed - narrowing to some signature-less "it's a
  function" wouldn't be safely callable, and ART has no real `Function`
  type to narrow to in the first place.
- **A `T | null` value can't be assigned into `any` directly.** `any`
  and `T | null` are two separate, non-interacting escape hatches in
  this version - narrow the `T | null` down to plain `T` first (`if (x
  != null) { let a: any = x; ... }`), then it boxes the same as any
  other concrete value.

## What's not in ART

Deliberate omissions, not oversights - each traded for something else
(usually: simple enough to actually finish, or a native ahead-of-time
compiler with no runtime being able to make good on it at all):

- **No downcasting or `instanceof`.** Real inheritance and virtual
  dispatch do exist now (see [Inheritance](#inheritance)) - upcasting a
  derived instance to a base type is safe and needs no runtime type
  info at all, but the reverse direction (or checking an object's actual
  type at runtime) does, and ART has none yet.
- **No type inference for generics.** Every instantiation is explicit
  (`::<T>` or `<T>`).
- **No general union types.** `any` (see [Dynamic typing](#dynamic-typing))
  and `T | null` (see [Nullable types](#nullable-types)) are the two
  exceptions - a real `number | string`-style union with more than one
  named alternative isn't supported. A lookup that can fail and isn't
  worth a `T | null` return still returns something `.isNull()`-checkable
  instead.
- **No manual memory management.** Everything is GC'd (see
  [Memory management](#memory-management)) - there's no `free`/`delete`
  to omit accidentally, but also none available if you wanted it.

## Architecture

A conventional single-pass-per-phase pipeline, each phase in its own
file pair:

| Phase | Files | Does |
|---|---|---|
| Tokenizer | [`tokenizer.h/.cpp`](tokenizer.h) | Source text → tokens |
| Parser | [`parser.h/.cpp`](parser.h) | Tokens → AST ([`ast.h/.cpp`](ast.h)), recursive descent |
| Module resolution | [`module_resolver.h/.cpp`](module_resolver.h) | Multi-file `import`/`export` graphs → one merged `Program` |
| Sema | [`sema.h/.cpp`](sema.h) | Type checking, name resolution, generic instantiation |
| Codegen | [`codegen.h/.cpp`](codegen.h) | Checked AST → LLVM IR, via the LLVM C++ API directly |
| Driver | [`main.cpp`](main.cpp) | CLI, ties the above together, emits IR/object/linked binary |

Generics (functions, interfaces, classes) are monomorphized entirely in
Sema: each concrete instantiation is a deep-cloned, independently
type-checked copy of the template, registered under a mangled name
(`Box$number`, `Signal$number$get$value`, ...) - Codegen never needs to
know a function/method came from a generic template at all, it just
sees a flat list of fully-concrete `FunctionDecl`s.

The doc comments throughout `sema.h`/`sema.cpp`/`codegen.h`/
`codegen.cpp` are the actual source of truth for exact type-checking
and code-generation rules - this file stays at the "how do I use it"
level.

## Building and testing standalone

```bash
cmake -S art -B art/build -GNinja
cmake --build art/build
art/build/art some_file.ts -o out && ./out
```

`art/tests/run.sh` is the regression suite: every `art/tests/*.ts`
program (and each `art/tests/<name>/app.ts` multi-file one) is
compiled, linked into a real binary, and *run* - its own `main():
number` returns a failure count, so a nonzero exit always names a real
regression, not just a compile error. These need nothing beyond `art`
itself (i.e. LLVM) - none of them touch the DOM/timer bridge. Anything
that does (`art/tests/*.tsx`) is compile-only (`--emit-obj`): actually
linking and running one needs the real bridge object files plus libgc/
SDL2, out of scope for this script, so these only prove the compiler
itself accepts the program, not that it behaves correctly at runtime -
DOM/event behavior is instead verified the way it always has been in
this project, a standalone C++ harness linking the compiled object file
directly against `dom_node.cpp`/`art_bridge.cpp` and firing real
`Node::Click()`/`DispatchEvent()` calls, just not checked into the repo
as a persistent, re-runnable suite the way `art/tests/` is.
`art/tests/errors/*` are the opposite of both: expected to *fail* to
compile, a real language mistake caught, not a regression.

```bash
art/tests/run.sh                 # uses art/build/art by default
art/tests/run.sh path/to/art     # or point it at a different binary
```
