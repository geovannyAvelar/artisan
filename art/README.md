# ART

**A**rtisan **R**epresentation for **T**ype**S**cript — a small,
statically typed language, purpose-built as the app-logic layer for the
[artisan](../README.md) native desktop framework.

ART looks like TypeScript with the dynamic parts removed: no `any`, no
prototypes, no dynamic property access, no closures. It's compiled ahead
of time to native machine code via LLVM — not interpreted, not JIT'd —
and linked straight into an artisan binary alongside its C++/Go/JS
counterparts. If you've used TypeScript, ART should read as "the parts
of TypeScript a native, ahead-of-time compiler can make good on,"
nothing more.

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
artisan-cli new my-app  # --lang art is the default
artisan-cli build my-app --run
```

That scaffolds `app.ts` with the DOM bridge already `declare`d and
wrapped in ergonomic classes (see [The DOM bridge](#the-dom-bridge)
below), plus an empty `setupApp`.

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
(`ARTISAN_APP_ART_SOURCE`/`app.ts`) - a C++/Go/JS-only project never
pulls either in.

## Language guide

### Types

`number` (a double, same range/precision as real TypeScript's), 
`boolean`, `string` (UTF-8 bytes, `+` concatenation, `==`/`!=`,
`.length`, `s[i]` character indexing - immutable, no `s[i] = ...`), and
`T[]` arrays (`.length`, `arr[i]`/`arr[i] = v`, `for...of`). No `any`,
no union types, no `null`/`undefined` - see
[What's not in ART](#whats-not-in-art) for how code that would reach for
these gets by without them.

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

### Functions

```ts
function add(a: number, b: number): number {
  return a + b;
}
```

A bare function name (not a call) is ART's only function-pointer-shaped
value - `(params...) => void` - always a plain, capture-free code
address, since ART has no closures:

```ts
function onClick(): void { /* ... */ }
let handler: () => void = onClick;
```

A Handler value doesn't have to be a bare name, either - a variable or
array element holding one is callable too (`handler()`,
`handlers[i]()`), compiled as a real indirect call through the computed
function pointer. This is what makes something like a dynamic list of
event subscribers possible at all - see [Signals](#signals).

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
on top of what an interface already gives - deliberately *not* real
OOP: no inheritance, no virtual/dynamic dispatch. A method call is pure
call-site sugar, always resolved statically against the receiver's own
declared type, compiling to a plain call with the receiver spliced in as
the real first argument. `this` is implicit, never written in a
method's own parameter list; there's no `new` - a class instance is
built the exact same way an interface's already is.

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
top-level `let`/`const`; `import { a, b } from "./path";` at the top of
a file (relative, `.ts` implied). Access control only, not real
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
`addEventListener`, `createElement`, `appendChild`, ...).

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
path to the DOM at all.

`.isNull()` is the only way to test a lookup for "no match" - ART has no
null literal of its own to compare against. `ArtIsNull`/`.isNull()`
being `true` is otherwise the whole story: no exceptions, no thrown
errors anywhere in ART.

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
sync with data with no diffing needed - and since there are no closures,
each rendered item can't carry its own bound handler, so one listener on
the *container* uses event delegation instead, resolving `event.target`
back to the item that was clicked via a `data-index` attribute:

```ts
function renderList(): void {
  let container: Node = document.getElementById("item-list");
  if (container.isNull()) { return; }
  while (container.childCount() > 0) { container.childAt(0).remove(); } // clear
  let xs: number[] = items.value; // subscribes renderList to items
  let i: number = 0;
  while (i < xs.length) { // rebuild
    let li: Node = document.createElement("li");
    li.textContent = numberToString(xs[i]);
    li.setAttribute("data-index", numberToString(i));
    container.appendChild(li);
    i = i + 1;
  }
}

function onItemClick(event: Event): void {
  let target: Node = event.target;
  let xs: number[] = items.value;
  let i: number = 0;
  while (i < xs.length) {
    if (target.getAttribute("data-index") == numberToString(i)) {
      items.value = removeAt(xs, i); // re-runs renderList on its own
      return;
    }
    i = i + 1;
  }
}
```

Removing the very node a click is bubbling through (the clear step tears
down the whole subtree, including whichever `<li>` was just clicked) is
safe: `.remove()` only detaches a node from its parent, it never frees
it mid-dispatch, so the ongoing bubbling walk keeps working off pointers
that are still valid, just no longer attached to anything. See
[the main README's "List rendering" section](../README.md#list-rendering)
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

## What's not in ART

Deliberate omissions, not oversights - each traded for something else
(usually: simple enough to actually finish, or a native ahead-of-time
compiler with no runtime being able to make good on it at all):

- **No closures.** A bare function name is just its own compiled code
  address - there's no captured environment to box, which is also why
  passing a handler around never needs a registry/handle scheme the way
  Go's `cgo.Handle` does.
- **No inheritance or dynamic dispatch.** A method call is always
  resolved statically against the receiver's declared type.
- **No type inference for generics.** Every instantiation is explicit
  (`::<T>` or `<T>`).
- **No `any`, no union types, no `null`/`undefined`.** A lookup that can
  fail returns something `.isNull()`-checkable instead.
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

There's no dedicated test suite yet - correctness is verified by
compiling and running real `.ts` programs (and, for anything touching
the DOM bridge, a small standalone C++ harness linking the compiled
object file directly against `dom_node.cpp`/`art_bridge.cpp` and firing
real `Node::Click()`/`DispatchEvent()` calls) rather than unit-testing
the compiler's internals in isolation.
