# ARTISAN

A framework for building native desktop apps from HTML-like markup. Markup
is compiled ahead-of-time (by `artisanc`) into a widget tree baked straight
into the binary, and rendered with Skia - no browser, no webview. App
behavior can be plain C++ (`SetupApp(Node&)`), a compiled Go app, or an
embedded JavaScript file - all three drive the same mutable DOM (`Node`)
at runtime, and none of them know the others exist.

`artisan-cli` is the single command that ties the workflow together:
scaffold a project, add components, and build it into one native binary.

## Prerequisites

- CMake 3.20+, a C++20 compiler, Python 3, Ninja
- `pkg-config` packages: `freetype2`, `fontconfig`, `sdl2`
- Git submodules: Skia, lexbor, QuickJS
- Go 1.21+ - only needed if a project uses a Go app (see [Using Go](#using-go))

```bash
git submodule update --init --recursive
./build_skia.sh
```

`build_skia.sh` only needs to run once (or after updating the Skia
submodule) - it builds Skia's static libraries into
`third_party/skia/out/Debug`, which the main CMake build then links against.

## Building artisan-cli

```bash
cmake -S . -B build
cmake --build build --target artisan_cli
```

This produces `build/artisan-cli`. Put it on your `PATH`, or invoke it by
path, for everything below.

## Creating a project

```bash
artisan-cli new my-app
```

Scaffolds:

```
my-app/
  pages/index.html   # starter markup - the page the app opens on
  src/main.cpp        # SetupApp(Node&) - native startup code
  CMakeLists.txt      # builds this project directly, no artisan-cli needed
```

A C++ project (not `--lang go`) also gets its own `CMakeLists.txt`, so
`cmake -S my-app -B my-app/build && cmake --build my-app/build --target
artisan` works on its own - no `artisan-cli` involved at build time. It
discovers `pages/**/*.html`/`src/**/*.cpp`/`app.js` itself (same
conventions as `artisan-cli build`, just re-implemented in CMake) and
`add_subdirectory()`s this checkout to pull in the real build - so Skia
still needs to be built there first (`./build_skia.sh`), same
prerequisite either way. If this checkout moves, update the
`ARTISAN_CHECKOUT_DIR` cache variable at the top of the generated file
(same constraint `goapp/go.mod`'s `replace` line has for a Go project).

Add more pages under `pages/` - nested folders become nested routes,
Next.js-style (`pages/settings/profile.html` becomes the route
`settings/profile`; a folder's own `index.html` is that folder's route).
Link between pages from markup with `<a href="...">`. Add more `.cpp`
files anywhere under `src/` and they're compiled in automatically - no
need to list them anywhere.

Pass `--lang go` to scaffold a Go project instead (see [Using
Go](#using-go)):

```bash
artisan-cli new my-app --lang go
```

```
my-app/
  pages/index.html
  goapp/
    go.mod   # replace artisango => <this checkout>/go/artisango
    main.go  # SetupApp(artisango.Node) - native startup code
```

A project uses one native language, never both - `new` always scaffolds
one or the other, and `artisan-cli build` refuses to build a project that
somehow ends up with both `src/**/*.cpp` and `goapp/`.

## Building a project

```bash
artisan-cli build my-app --run
```

There's no mode for naming individual files by hand - it's always a project
directory, and its files are discovered automatically: every
`pages/**/*.html`, either every `src/**/*.cpp` *or* a Go app under `goapp/`
(never both - see [Using Go](#using-go) below), and an optional `app.js` at
the project root (embedded and run once at startup, alongside whichever
native language the project uses). Flags:

- `--build-dir <dir>` - where to configure/build (default `./build`)
- `-o, --output <path>` - copy the built binary here
- `--run` - run the binary after a successful build

## Adding a component

```bash
artisan-cli component my-app my-widget
```

Scaffolds a reusable pairing within an existing project:

- `components/my-widget.html` - a markup fragment, meant to be pasted into
  any page under `pages/` wherever you want it to appear (it isn't itself a
  routable page)
- `src/components/my-widget.cpp` - `Setup_my_widget(Node&)`, which the
  page's own `SetupApp` calls explicitly with the `Node` the fragment
  landed in

The generated `Setup_my_widget` includes an example of using `UseState`
(`include/hooks.h`) - a small `useState`-style helper for giving a
component's event handlers shared state (e.g. a counter) without a class
or a hand-rolled `shared_ptr`:

```cpp
auto [count, setCount] = UseState(0);
button->SetOnClick([=]() mutable {
  setCount(count() + 1);
  label->SetTextContent(std::to_string(count()));
});
```

Note there's no re-render loop here: `Setup_<name>` runs once at startup,
so `setCount` just stores the new value - updating the DOM in response
(`SetTextContent`, `SetAttribute`, ...) is still up to the handler, same as
any other mutation via the `Node` API (see `include/dom_node.h`).

## Using Go

A project can drive its DOM from a native Go app instead of C++, compiled
ahead-of-time into a static archive (`go build -buildmode=c-archive`) and
linked straight into the binary - it never runs interpreted, and requires a
Go toolchain only for projects that use it.

```bash
artisan-cli new my-app --lang go
```

scaffolds `goapp/go.mod` (with a `replace artisango => ...` already
pointing at this checkout) and `goapp/main.go`:

```go
package main

import "C"
import (
	"unsafe"

	"artisango"
)

func SetupApp(doc artisango.Node) {
	// Your native startup code goes here, e.g.:
	//
	//   button := doc.FindById("my-button")
	//   if button != nil {
	//     button.SetOnClick(func() {
	//       // ...
	//     })
	//   }
}

//export ArtisanSetupApp
func ArtisanSetupApp(doc unsafe.Pointer) {
	SetupApp(artisango.WrapNode(doc))
}

func main() {}
```

`artisan-cli build` auto-discovers `goapp/` the same way it discovers
`app.js` - no flag needed. `artisango.Node` mirrors the same `Node` API
C++/JS get: `FindById`, `NodeType()` (`artisango.ElementNode`/
`TextNode`), `TagName`, `TextContent`/`SetTextContent`,
`GetAttribute`/`SetAttribute`/`HasAttribute`/`RemoveAttribute`,
`ParentNode`/`NextSibling`/`PreviousSibling`/`Children`,
`Matches(selector)`/`Closest(selector)`,
`QuerySelector`/`QuerySelectorAll` (same bounded selector grammar as
[Using CSS](#using-css) below), `RemoveChild`/`Remove()` (the removed
node stays alive and re-appendable), `CloneNode(deep)`, `ClassList()`
(`Add`/`Remove`/`Contains`/`Toggle`/`ToggleForce`), `Style()`
(`Get`/`Set`, the same five properties [Using CSS](#using-css) supports),
`GetData`/`SetData` (`fooBar` <-> `data-foo-bar`), `SetOnClick`,
`AddEventListener(type, fn, capture)` (any type string works; `"click"`,
`"change"` (checkbox/radio), and `"input"` (text fields) fire on their
own, and `DispatchEvent` below fires any other type) plus
`RemoveEventListener(type, handle, capture)` - `AddEventListener` returns
a `ListenerHandle` token for this (Go func values aren't comparable the
way JS passes the same function reference to both calls, so removal
keys off this instead), and `DispatchEvent(type, bubbles, cancelable)`
fires the same capturing/target/bubbling walk internally-fired events
use, returning `false` if the event was cancelable and some listener
called `preventDefault()` (JS-side only - a Go listener's `fn` is
zero-arg, with no way to call it itself) - see `go/artisango/node.go`
and the C ABI it wraps, `include/node_c_api.h`.

Package-level (not node-scoped, like `CreateElement`):
`artisango.SetTimeout(fn, delayMs)`/`SetInterval(fn, delayMs)`/
`ClearTimer(handle)`, and `RequestAnimationFrame(fn)`/
`CancelAnimationFrame(handle)` (`fn` gets a timestamp; the normal way to
animate is for `fn` to call `RequestAnimationFrame` again itself, which
schedules for the *next* repaint, never the current one) - the same
`TimerQueue`/`AnimationFrameQueue` JS's `setTimeout`/
`requestAnimationFrame` schedule into. `SetTimeout`/`SetInterval`/
`RequestAnimationFrame` each return a handle for
`ClearTimer`/`CancelAnimationFrame` - Go func values aren't comparable,
so there's no "pass the same `fn` back" the way JS's `clearTimeout`
conceptually could.

`artisango.CreateElement(tag)`/`CreateTextNode(text)` create a detached
node; `parent.AppendChild(child)`/`parent.InsertBefore(child, before)`
attach it. A created (or removed, via `RemoveChild`/`Remove()`) node
that's never (re-)appended anywhere leaks until process exit - there's
no garbage collector on the Go side to catch an abandoned one the way
JS's engine does, so always append (or just don't create) rather than
let one go unused.

If moving this checkout later breaks the build, update the `replace` line
in `goapp/go.mod` to point at the new location (same underlying constraint
`ARTISAN_PROJECT_SOURCE_DIR` already has for the C++ path).

## Using ART

ART is a small, statically typed language of this repo's own design -
TypeScript-like syntax with the dynamic parts removed (no `any`, no
prototypes, no dynamic property access), compiled ahead of time to native
machine code via LLVM (see `art/`) rather than interpreted. A project's
`app.art` - one file at the project root by default, like `app.js`, but
optionally the entry point into a real multi-file project via
`import`/`export` (see "Modules" below) - is compiled by the standalone
`art` compiler and linked straight into the binary, the same "runs
compiled, not interpreted" deal `ARTISAN_APP_CPP_SOURCES`/
`ARTISAN_APP_GO_SOURCE` get.

```bash
artisan-cli new my-app --lang art
```

scaffolds `app.art` with the DOM bridge below already `declare`d and
wrapped in `declare class Node`/`declare class Event` (see "Classes"
below), plus an empty setup section. An ART app needs a `setupApp` -
either an explicit function, or (the default the scaffold uses) just
bare top-level statements, which the compiler collects into one for you:

```ts
function onButtonClick(event: Event): void {
  // ...
}

{
  let button: Node = document.getElementById("my-button");
  if (!button.isNull()) {
    button.addEventListener("click", onButtonClick, false);
    button.textContent = "Ready";
  }
}
```

or, written the explicit-function way instead (the two are
interchangeable, but a project can't mix them):

```ts
function setupApp(): void {
  let button: Node = document.getElementById("my-button");
  if (!button.isNull()) {
    button.addEventListener("click", onButtonClick, false);
    button.textContent = "Ready";
  }
}
```

Either way, `setupApp` runs once whenever a page loads, same as
`SetupApp(Node&)`/Go's `ArtisanSetupApp`/a script's top-level code all
are - main.cpp's C++ trampoline calls the exact same `setupApp` symbol
regardless of which form produced it. The block around the top-level
version above isn't decorative: a *bare* top-level `let` (unwrapped) is
always a persistent global (see "Top-level state" below), initialized
once at process start, before any page has ever loaded - so `document`
is always unusable directly in one (a compile error, not a runtime
surprise). A block's own `let`s are ordinary locals, re-run fresh every
time `setupApp` runs, exactly like `{ }` already means everywhere else
in ART - wrap any procedural setup code that touches `document` in one.

`document` is ambient - always just there, the same way a real
browser's own global `document` is, not something `setupApp` (or
anything else) has to be handed or look up itself (see its own doc
comment further down for exactly how). `Node` is an opaque foreign type
(`declare class Node;`
underneath - see "Classes" below) - a plain handle ART code passes around
but never constructs or looks inside; the DOM API itself is a curated
subset of `include/node_c_api.h`, exposed as ART-callable `declare
function`s (`include/art_bridge.h` has the exact signatures) wrapped into
`Node`/`Event` methods and properties (see "Getters and setters" below)
named/shaped to match a real browser's own DOM API as closely as
possible - all copied into every scaffolded `app.art`:
`document.getElementById(id)`, `.querySelector(selector)`, `.isNull()`
(the only way to test a lookup for "no match" - ART has no null literal
of its own to compare against), `.textContent` (get/set), `.getAttribute
(name)`/`.hasAttribute(name)`/`.setAttribute(name, value)`,
`.childCount()`/`.childAt(index)`, and a full set of tree-mutation
methods for building UI at runtime rather than only reading/editing what
`pages/*.html` already compiled in: `document.createElement(tag)`/
`.createTextNode(text)` (a detached node - leaks if never appended,
same accepted tradeoff Go/JS already have, see `include/node_c_api.h`),
`node.appendChild(child)`/`.insertBefore(child, before)` (`before` must
be an existing child, never null - ART has no null literal to pass, so
`appendChild` covers "insert at the end" instead), `.removeChild(child)`/
`.remove()` (detaches, doesn't free - same leaks-if-never-reattached
deal a freshly created node has), and `.cloneNode(deep)`.

`document` (see `kAmbientGlobals` in `art/sema.cpp`) isn't a real
variable or a declared function of its own - it's pure sugar, rewritten
at compile time into a call to the `declare function ArtDocument():
Node;` every scaffolded `app.art` still declares (needed as the sugar's
actual target, even though nothing calls it directly by name anymore).
`ArtDocument()`/`document` returns the current page's root `Node`,
whatever it is at the moment of the call - it exists specifically for a
handler: unlike `setupApp`, a handler takes no `Node` parameter of its
own, so without some way to ask for the document directly it would have
no path to the DOM at all except whatever a top-level `let`/`const`
could carry forward, and a `Node` can't be one of those (see below).
`include/art_bridge_context.h` tracks which `Node` it currently points
to, updated by `main.cpp` once per page load/navigation (the same place
`node_c_api_bridge.h`'s `SetGoTimerContext` already is, and before
`setupApp`/any handler can possibly run) - `.isNull()` on it is `true`
only in the narrow window between one page's tree being torn down and
the next one finishing its build, which a handler invoked from a live
page will never actually observe. A project that declares its own
`document` (a variable, parameter, or function) shadows the sugar
entirely, the same precedence a real local always has over anything
ambient - and nothing else is ambient yet: `window` isn't, since there's
no window-level bridge function (timers, `alert`, `location`, ...) to
desugar it to.

`node.addEventListener(eventType, handler, capture)` registers an event
listener - any type (`"click"`, `"keydown"`, `"change"`, `"input"`, ...),
and it *stacks*: registering a second listener for the same node/type
doesn't replace the first, both run (`Node::AddEventListener`, same as
real `addEventListener` - there's no separate single-handler "onclick"-
style method, matching how a real browser's own `addEventListener` is
the modern, preferred way to do this over the legacy `el.onclick = fn`
property). `handler` isn't a call, just a bare reference to a top-level
`function ...(event: Event): void` (`void` return - the only
function-pointer-shaped value ART has, written as a type like `(event:
Event) => void`), checked structurally like any other type (a mismatched
parameter list is a compile error, not a silent bug). Passing a function
by name this way needs no closures or heap boxing the way a real
callback usually would: ART functions can't capture outer variables in
the first place, so the reference is just that function's own compiled
address, handed straight to `Node::AddEventListener` as a plain C
function pointer - contrast Go's `ArtisanNodeAddEventListener`, which has
to carry a `uintptr_t` handle through a Go-side registry (`cgo.Handle`)
instead, since a Go closure can't produce a raw callable address like
this. `Event` is another opaque foreign type, read through its own
properties: `.eventType` (named that, not `.type` - `type` is a
reserved word, used by `declare type`), `.target` (a `Node`),
`ArtEventDetail<T>` (a free function, not a property - see
`ArtDispatchEvent<T>` below for why), `.bubbles`/`.cancelable`, and the
MouseEvent/KeyboardEvent data (`.clientX`/`.clientY`,
`.ctrlKey`/`.shiftKey`/`.altKey`/`.metaKey`, `.key`/`.code`) - all
read-only (get-only) properties, same as a real browser's own Event.
`.preventDefault()`/`.defaultPrevented` (the latter *is* a get-only
property - it's data, not an action), and
`.stopPropagation()`/`.stopImmediatePropagation()` stay ordinary methods
(the latter two also imply the former - stopping "immediately" means
neither any remaining listener at the current node nor any further
ancestor gets a turn, so e.g. a second `addEventListener` on the *same*
node/type never runs, not just an ancestor's - same as real DOM). Real
DOM has no corresponding getter for either stop method (no
`.propagationStopped`/`.immediatePropagationStopped`) - ART's doesn't
either, for the same reason: script was never meant to be able to ask
"was propagation already stopped", only to cause it.

`node.removeEventListener(eventType, handler, capture)` removes every
listener matching all three exactly - a mismatched call (wrong handler,
wrong `capture`, or one never added at all) is a safe no-op, same as real
`removeEventListener`. `Node::RemoveEventListener` has no built-in notion
of "the same handler" (`EventHandler` is a type-erased `std::function`,
not comparable on its own) - it takes a predicate instead, and
`art_bridge.cpp` supplies one via `std::function::target<T>()`, the exact
mechanism `node_c_api.cpp`'s Go path (`GoCallback`) and `js_engine.cpp`'s
JS path (`JsCallback`) already use for the same reason, just recovering a
named wrapper class around ART's raw function pointer instead of a Go
handle or a JS closure - `addEventListener` above stores one of these
(not a bare lambda, which has no nameable type to recover later)
specifically so removal can find it again.

`ArtDispatchEvent<T>(node, eventType, bubbles, cancelable, detail)` is
the other side of general events - unlike everything above (which all
react to events something else fires), this actually fires one, any type
at all, not just the built-in click/change/input this Node model already
knows to fire (`Node::DispatchEvent`, same signature/semantics: the usual
capturing/target/bubbling walk, returning `false` if the event was
cancelable and some listener called `event.preventDefault()`). `T` is
where a real `CustomEvent`'s `detail` payload's type comes in - see
"Generic functions" below for the language feature itself; the short
version is `ArtDispatchEvent::<number>(document, "scored", true, true, 10)`
and, in a listener on an event ART itself dispatched, `let n: number =
ArtEventDetail::<number>(event);` reads it back, both real, separately
compiled functions (`art_bridge.h`'s `ArtDispatchEvent$number`/
`ArtEventDetail$number`) - `T` can be `number`, `boolean`, or `string`,
each with its own pair the bridge actually provides; nothing else (a call
with a `T` the bridge doesn't have a pair for, e.g. your own interface)
is a link error, not something ART's own type checker catches. Even with
`T` known, `Event::detail` is still a bare `const void*` underneath (see
`Event::detail`'s own note - "carried through the dispatch walk
unexamined" no matter which binding constructs it, JS/Go/ART alike), so
`ArtEventDetail<T>` is only safe to call in a listener on an event *ART
itself* dispatched, with the *same* `T` the dispatch used - nothing tags
the value with which `T` it actually holds, so a mismatched `T` (or an
event some other binding dispatched, whose own `detail` is typically a
boxed script value) reads back nonsense rather than failing loudly - the
same "each binding (now, each `T`) owns its own interpretation" contract
`detail` always had. `ArtEventDetail<T>` on an event with no detail at
all (a plain internally-fired click/change/input) reads back as `""`/
`false`/`0` depending on `T`, the same "can't represent null" convention
`.getAttribute()` already uses for an unset attribute.

### Generic functions

`function`/`declare function` can take type parameters:

```ts
function identity<T>(x: T): T {
  return x;
}
```

and a call site instantiates them explicitly, turbofish-style -
`identity::<number>(5)`, `identity::<Point>(p)`. There's no inference
(`identity(5)` alone is a compile error asking for the `::<...>`) - a
deliberately bounded first pass, not the full breadth a real
TypeScript's generics have. Plain `<T>` at a call site (rather than
`::<T>`) would be genuinely ambiguous with
the `<`/`>` comparison operators - `foo<bar>(baz)` could just as well
parse as `(foo < bar) > (baz)` - and unlike TypeScript's own parser,
which resolves this by already knowing every name's declared kind by the
time it parses an expression, ART's parser runs as a single pass *before*
Sema, so it can't yet know whether `foo` even names a generic function.
`::<...>` sidesteps the ambiguity outright, the same reason Rust's own
turbofish exists.

Generic functions are monomorphized, not type-erased - matching
real-world ahead-of-time-compiled languages with no runtime type
information (C++ templates, Rust generics) rather than Java's or C#'s
generics, which lean on a runtime to erase/reify as needed. Each distinct
`name::<ConcreteTypes...>` combination actually called becomes its own
ordinary, fully concrete function once compiled - `identity::<number>`
and `identity::<string>` are two unrelated LLVM functions with mangled
names (`identity$number`, `identity$string` - `$`, a real if
nonstandard identifier character both GCC and Clang accept, chosen so a
mangled name stays a single valid symbol without escaping). A generic
function's body is never type-checked in its own unsubstituted "template"
form - only each instantiation actually used, individually, with its own
type parameters already replaced by the concrete types that call site
gave (a lazy, on-first-use scheme, the same "never fully checked until
instantiated" property C++ templates have - a generic function nobody
ever calls can have i.e. a body that would fail to type-check for *every*
concrete `T`, and ART will never notice). This also means a generic
`declare function` needs a separately provided, separately named extern
symbol per distinct instantiation actually used - see `ArtDispatchEvent`/
`ArtEventDetail` above for exactly this in practice: the bridge doesn't
(and structurally can't) provide one for every type a project might ever
write, only `number`/`boolean`/`string`.

### Generic interfaces

`interface`/`declare type` can take type parameters too:

```ts
interface Box<T> {
  value: T;
}
```

Unlike a generic function call, a generic *type* reference - `Box<number>`
as a variable's declared type, a parameter/return type, another
interface's own field type, anywhere a type is written - uses plain
`<...>`, no turbofish: a type position can never also parse as a `<`/`>`
comparison the way an expression can, so there's no ambiguity to route
around in the first place (see "Generic functions" above for why the
call-site form needs one). `Box<number>` monomorphizes exactly like a
generic function call does, reusing the very same machinery: the first
time a given `name<ConcreteTypes...>` combination is actually used, its
fields get cloned and resolved (with type parameters substituted for the
concrete types this reference gave) into an ordinary, fully concrete
interface registered under a mangled name (`Box$number`) - indistinguishable
from a plain, hand-written `interface Box$number { value: number; }` to
everything downstream (object literal checking, field access, Codegen's
own struct-layout code), which is why none of those needed any changes
at all to support this. A self-referential generic interface (`interface
Node<T> { value: T; next: Node<T>; }`) resolves correctly, the same
"register the instantiation before resolving its own fields" trick a
self-recursive generic *function* needs - but building an actual
value at that type still isn't practical without a null/optional
concept ART doesn't have yet: every field, including a self-referential
`next: Node<T>`, is required, so an object literal can never terminate
one; making one requires mutating an already-constructed value's field
after the fact instead (`a.next = a;`), a real, separate limitation
predating generics.

`declare type Box<T>;` (a generic opaque type) works the same way,
paired with generic `declare function`s that produce/consume it - e.g.
`declare function makeBox<T>(x: T): Box<T>; declare function
unbox<T>(b: Box<T>): T;` - a real, useful pattern for a type-safe opaque
handle at the FFI boundary even though nothing about its own
representation actually varies by `T`. Like any generic `declare
function`, each instantiation used needs its own separately provided,
separately named extern symbol (`makeBox$number`, ...) - see "Generic
functions" above.

### Modules (import/export)

A single `.art` file is still a complete, self-contained program - nothing
below is required. But a project can also be split across multiple files
and wired together with real `import`/`export`, TS-style:

```ts
// math.art
export function add(a: number, b: number): number {
  return a + b;
}

function helper(): number { // not exported - private to math.art
  return 1;
}
```

```ts
// app.art
import { add } from "./math";

function main(): number {
  return add(1, 2);
}
```

`import`s are always relative (`./`/`../`, `.art` optional) and must come
before any other top-level declaration in a file. `export` may prefix a
`function`, `interface`, `declare function`, `declare type`, or top-level
`let`/`const` - anything left unmarked is private to the file that
declares it, invisible even to a file that imports something else from
the same one. This is access control only, not real per-file
namespacing: every top-level name in a whole project (across every file
reachable from the entry point) must still be globally unique, the same
as a single flat file today - `import` decides who's *allowed* to
reference a name, not which of several same-named things they get.

The entry point passed to `art` (or `app.art` itself, for
`artisan-cli build`) only needs `import`s at all for this multi-file
resolution to kick in - compiling a plain file with none is unchanged
from before this existed, same error messages included. When imports are
present, `art` first walks the whole file graph (detecting missing
files, missing/private exports, and circular imports up front, each
reported with the real file and line involved) before type-checking a
single merged program - so a name that exists somewhere in the project
but wasn't actually imported into the file trying to use it is rejected
with a hint pointing at the likely missing `import`, not a plain
"undefined identifier". Generic functions/interfaces work across files
too: a generic's own body/fields always resolve against the file that
*declared* it, not whichever file happens to instantiate it - so a
private helper `math.art` uses internally stays unreachable from
`app.art` even through a generic function `app.art` calls into.

### Classes

`class`/`declare class` add methods - `obj.method(args)` - on top of what
`interface`/`declare type` already give you (a heap-allocated struct of
fields). A class is deliberately *not* real OOP: no inheritance, no
virtual/dynamic dispatch, and a method can't be generic (yet) - a method
call is pure call-site sugar, always resolved statically against the
receiver's own declared type, for a plain call to that method with the
receiver spliced in as the real first argument:

```ts
class Counter {
  count: number;

  function increment(): void {
    this.count = this.count + 1;
  }
  function add(amount: number): void {
    this.count = this.count + amount;
  }
}

function main(): number {
  let c: Counter = { count: 0 }; // construction is unchanged - a class is
                                  // still just an interface underneath
  c.increment();
  c.add(10);
  return c.count; // fields stay directly readable too - 11
}
```

`this` is implicit - never written in a method's own parameter list -
and always typed as the class itself; there's no `new`, so a class
instance is built the exact same way an interface's already is (an
object literal against an explicit target type).

`declare class` is the opaque counterpart (no accessible fields, same as
`declare type`), for wrapping a `declare function`-based FFI surface into
ergonomic method calls instead of `ArtDoSomething(handle, ...)` C-style
calls - this is exactly how `artisan-cli new --lang art`'s scaffold
exposes the DOM bridge:

```ts
declare function ArtFindById(root: Node, id: string): Node;
declare function ArtIsNull(node: Node): boolean;

declare class Node {
  function getElementById(id: string): Node { return ArtFindById(this, id); }
  function isNull(): boolean { return ArtIsNull(this); }
}

function setupApp(): void {
  let button: Node = document.getElementById("my-button");
  if (!button.isNull()) {
    // ...
  }
}
```

A `declare class` method's body is real, ordinary ART code (not itself
extern-bound) - typically a one-line forward to the matching
`declare function`, but free to do more (validate an argument, combine
several bridge calls, ...). The underlying `Art*` functions keep their
own names and still work as plain free functions too - the class is
purely an additional, optional way to call them.

A property (accessor method) name can't be a reserved word either -
`get type(): string { ... }` doesn't parse any more than `function
type()` would (`type` is `declare type`'s keyword), which is why the
scaffold's `Event` class calls it `eventType` instead.

#### Getters and setters

`get`/`set` add real property syntax on top of a plain method - `get
name(): T { ... }`/`set name(value: T): void { ... }` inside a class
body, accessed as `obj.name`/`obj.name = value` (never `obj.name(...)`
- calling one like a plain method is a compile error, same as trying to
read a plain method without calling it is):

```ts
class Box {
  raw: number;

  get value(): number { return this.raw * 2; }
  set value(v: number) { this.raw = v / 2; }
}

function main(): number {
  let b: Box = { raw: 5 };
  let doubled: number = b.value; // 10 - calls the getter
  b.value = 100;                 // calls the setter - b.raw becomes 50
  return b.value;                // 100
}
```

Exactly like a plain method, this is pure call-site sugar with no
runtime cost or dynamic dispatch: `obj.name` compiles straight into a
call to the getter with `obj` as its receiver, and `obj.name = value`
into a call to the setter - Codegen never generates a real memory
address for either. `get`/`set` are contextual, not reserved words -
recognized only in this specific position (`get`/`set`, another
identifier, then `(`), so a class can still have an ordinary field or
method actually named `get`/`set` elsewhere, and the words stay usable
as identifiers everywhere else in the language too (unlike `type`
above).

A class's fields, plain methods, and get/set-accessed properties all
share one namespace - `obj.name` can only ever mean one thing. A getter
and setter may share a name (together forming one read/write property,
the only legal kind of duplicate here); anything else colliding with an
already-used name - two getters, a getter and a plain method, a getter
and a field, ... - is a compile error. A getter with no matching setter
is a read-only property (assigning to it is an error); a setter with no
matching getter is write-only (reading it is an error, the same
declared-but-inaccessible shape a private import has - see "Modules"
above). Neither `obj.prop++`/`obj.prop--` is supported on a get/set
property (only on a plain field, array element, or variable) - doing
that properly would mean reading through the getter, adding one, then
writing back through the setter, a distinct codegen path this doesn't
build; it's rejected with a clear error rather than silently
miscompiling. A property can't be generic yet, same as a plain method.

`declare class` accessors work the same way, and are exactly how the
scaffold exposes DOM properties that are naturally properties in a real
browser too - `node.textContent` (get/set) and `event.key`/`.target`/
`.clientX`/... (get-only) - while `getElementById`/`setAttribute`/
`addEventListener`/... stay ordinary methods, matching how those are
methods in a real browser as well.

#### Generic classes

`class`/`declare class` can take type parameters exactly like `interface`/
`declare type` already can - `class Name<T> { ... }`, instantiated as
`Name<Type>` at a type reference (same monomorphized, explicit-only deal
generic interfaces have - see "Generic interfaces" above - not `::<T>`,
which is only for a *call*): each distinct `Name<Type>` actually used
gets its own real, independently-checked struct layout and its own real,
separately-compiled copies of every method/accessor, the same way each
generic *function* instantiation does. A method/accessor still can't be
individually generic (`function foo<U>(...)` inside a class body is
still rejected) - only the class itself can - and `this` inside one is
typed as `Name<T>` (the class's own type parameters, referenced bare),
so it resolves back to whichever concrete instantiation is actually
running:

```ts
class Signal<T> {
  raw: T;

  get value(): T { return this.raw; }
  set value(v: T) { this.raw = v; }
}

function main(): number {
  let count: Signal<number> = { raw: 0 };
  let name: Signal<string> = { raw: "start" };

  count.value = count.value + 5;
  name.value = name.value + "!";

  return count.value; // 5 - Signal<number> and Signal<string> are two
                       // completely independent, coexisting types
}
```

This is the pattern to reach for anywhere you'd otherwise hand-write one
near-identical class per value type (`NumberSignal`, `StringSignal`, ...) -
a single generic definition, monomorphized per type actually used, same
tradeoffs as generic functions/interfaces already have (no type
inference, explicit instantiation only, one real compiled copy per
distinct type argument combination - see "Generic functions" above).

A top-level `let`/`const` is a handler's actual memory across calls -
`clicks`/`enabled`/etc. below keep their value between one `onClick` and
the next, the same way any other global does:

```ts
let clicks: number = 0;
const maxClicks: number = 5;

function onClick(): void {
  if (clicks < maxClicks) {
    clicks = clicks + 1;
  }
}
```

A global's type and initializer can be anything a local variable's can -
a call, an object/array literal, another (earlier) global, a bare
function reference, ... A bare number/boolean/string literal is still
special-cased as a real compile-time `llvm::Constant` (`Codegen::
GenGlobalDecl`); anything else is computed by running real code once, in
declaration order, via a second LLVM module constructor
(`Codegen::GenGlobalInit`) - the same static-initialization-before-`main`
mechanism the garbage collector's own `GC_init` already uses, just
extended to cover arbitrary global initializers too, always scheduled to
run *after* `GC_init` (an initializer that allocates - an object/array
literal - needs `GC_malloc` already safe to call). A later global can
reference an earlier one (already stored by the time its own initializer
runs) but not the reverse - ordinary top-to-bottom "declare before use"
semantics, the same rule most languages with static initialization have,
not specially enforced beyond that. This is what makes it possible to
hold real, persistent app state at the top level - a `Node` a handler
found earlier, a whole struct, a `Signal<T>` (see "Signals" below) -
without threading it through every function that needs it.

A top-level *statement* (anything that isn't a declaration or a `let`/
`const` - an `if`, a `while`, a bare call, a block) is different: it
doesn't declare anything, so there's nothing to persist - it's collected
(across every file in the merged program, dependency-first, same order
`globals` already merges in) into the body of a generated `setupApp`,
run every time that's called, not once at process start. This is the
procedural style shown under "Quick start" above - the compiler
generates the exact same `setupApp` C symbol either way, so it composes
with `declare class`/`declare function`/everything else exactly like an
explicit `function setupApp()` would; the only rule is a project can't
have both (a clear compile error, not a silent pick-one).

The two lists exist because they're genuinely different lifetimes, not
two ways to write the same thing - and mixing them up the natural way
(a bare `let` where you meant a per-run local) fails loudly rather than
quietly: a global that directly calls the ambient `document` (or
anything backed by it) is a compile error, since a global initializes
once, before any page has ever loaded, and `document` is unusable there
- not the narrow "no page loaded yet" window `.isNull()` normally covers
elsewhere, but the *permanent* state at that point. Wrap it in a block
instead:

```ts
{
  let button: Node = document.getElementById("my-button"); // a real local -
  if (!button.isNull()) {                                  // re-run fresh
    button.addEventListener("click", onButtonClick, false); // every setupApp
  }                                                          // call
}
```

(This check only catches a *direct* call in the global's own
initializer, not one reached indirectly through another function it
calls into - real, but a narrower guarantee than "provably safe".)

`numberToString(n: number): string` - a real built-in, not a
`declare function` (it needs no C++ counterpart in a project's own code,
unlike everything `art_bridge.h` exposes - it's available in any ART
program, even one with no DOM involved at all) - is the other half of
displaying a counter like `clicks` above:
`ArtSetTextContent(label, numberToString(clicks) + " clicks")`. It
formats via libc's `snprintf("%.15g", ...)` rather than reimplementing
double-to-string from scratch, which gets ordinary values exactly right
(`42` -> `"42"`, `3.14` -> `"3.14"`, `-7` -> `"-7"`) but isn't a
guaranteed match for real JS's own `Number`-to-string algorithm at the
edges - `NaN`/`Infinity` print as `"nan"`/`"inf"` (libc's spelling, not
JS's), `-0` prints as `"-0"` rather than `"0"`, and extremely large/small
magnitudes may round or switch to scientific notation slightly
differently.

The language itself: `function`/`interface`/`let`/`const` (locals and
top-level), `if`/`else`, `while`, C-style `for`, `for...of` over an array,
`number` (a double, same as real TS) with the `numberToString` builtin
above, `boolean`, `string` (with `+` concatenation, `==`/`!=`, `.length`,
and `s[i]` indexing - immutable, no `s[i] = ...`), `T[]` arrays,
structural interfaces (an object literal must match a declared
`interface` exactly - no excess or missing fields), `class`/`declare
class` (methods and `get`/`set` accessor properties on top of an
interface/opaque type - see "Classes" above; no inheritance or dynamic
dispatch), the parameterized handler type described above (`(p0: T0,
p1: T1, ...) => void`, structural like everything else - parameter names
are decorative, only the types and their order are checked), and
generics - functions, interfaces/`declare type`, and classes/`declare
class` alike (`function`/`declare function`/`interface`/`declare type`/
`class`/`declare class`, monomorphized, explicit instantiation only:
`::<T>` at a function call site, plain `<T>` at a type reference - see
"Generic functions"/"Generic interfaces"/"Generic classes" above for why
those differ; a method/accessor still can't be individually generic,
only the class itself can); both
prefix (`++x`/`--x`, evaluates to the new value) and postfix (`x++`/
`x--`, evaluates to the old value) forms of increment/decrement, on the
same targets assignment already allows (a plain variable, an array
element, a struct field, or a setter-backed property - except a
get/set-*accessor* property specifically, which supports plain
assignment but not increment/decrement, see "Getters and setters" above
for why) - real TS's chaining rules apply here too (`x++.foo`/`x++()`
aren't expressions; postfix always ends the expression it's attached
to). No inheritance, real closures, `any`, or union types. See the doc
comments in `art/*.h`/`art/*.cpp` for the exact grammar and
type-checking rules.

Every array/object/string is a heap allocation, garbage-collected by the
[Boehm-Demers-Weiser collector](https://www.hboehm.info/gc/) (`libgc`) -
ART code never explicitly frees anything (there's no `delete`/`free` in
the language), but unreachable allocations do get reclaimed
automatically. The collector is conservative: it scans the native
stack/registers/globals for anything that looks like a pointer into its
own heap rather than tracking types precisely, so it can - rarely -
retain a little garbage a precise collector wouldn't, but it never frees
something still reachable. This is also why a heap-allocated ART value
handed across the FFI boundary (e.g. an `ArtString*` passed into a
`declare function`) is safe without any extra bookkeeping: `art_bridge.h`
copies it into artisan's own (unrelated) memory before doing anything
that could outlive the call.

### Signals

A `Signal<T>` is a reactive value: reading `.value` inside an `effect()`
automatically subscribes that effect to it, and writing `.value`
automatically re-runs every effect that ever read it - no manual "now go
update the label" call anywhere. This isn't a compiler feature - it's an
ordinary generic class, built entirely from what's already covered
above (generics, `get`/`set` accessors, non-literal globals, and one
more small piece: calling a Handler-*valued* expression, not just a bare
function name - see below):

```ts
const MAX_SUBSCRIBERS: number = 16;
function noopEffect(): void {}

// Which effect is currently running, if any - ambient by necessity: any
// signal, anywhere, needs to see it, not just whichever function happens
// to have it in scope. Only possible now that a global can hold a
// Handler value at all - see the top-level `let`/`const` note above.
let currentEffect: () => void = noopEffect;

export class Signal<T> {
  raw: T;
  subscribers: () => void[];
  subscriberCount: number;

  get value(): T {
    if (currentEffect != noopEffect) { this.subscribe(currentEffect); }
    return this.raw;
  }
  set value(v: T) {
    this.raw = v;
    this.notify();
  }

  function subscribe(fn: () => void): void {
    let i: number = 0;
    while (i < this.subscriberCount) {
      if (this.subscribers[i] == fn) { return; } // already subscribed
      i = i + 1;
    }
    if (this.subscriberCount < MAX_SUBSCRIBERS) {
      this.subscribers[this.subscriberCount] = fn;
      this.subscriberCount = this.subscriberCount + 1;
    }
  }

  function notify(): void {
    let i: number = 0;
    while (i < this.subscriberCount) {
      let fn: () => void = this.subscribers[i]; // an array element, not a
      fn();                                      // named function - see below
      i = i + 1;
    }
  }
}

export function makeSignal<T>(initial: T): Signal<T> {
  return { raw: initial, subscriberCount: 0, subscribers: [
    noopEffect, noopEffect, noopEffect, noopEffect, noopEffect, noopEffect,
    noopEffect, noopEffect, noopEffect, noopEffect, noopEffect, noopEffect,
    noopEffect, noopEffect, noopEffect, noopEffect
  ] };
}

export function effect(fn: () => void): void {
  let saved: () => void = currentEffect;
  currentEffect = fn;
  fn(); // runs once now, tracking whatever it reads along the way
  currentEffect = saved; // restores correctly even if effects nest
}
```

Used like this - note `onButtonClick` never touches the DOM at all:

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

`this.subscribers[i]()` above is the one genuinely new piece: calling a
Handler stored in a variable or array element, not a bare function name
or a class method. A bare name alone could never express "whichever
handler happens to be in this slot" - `notify()` doesn't know at compile
time which functions will have subscribed. Codegen emits this as a real
indirect call (through the computed function-pointer value, not a
symbol lookup - see `Expr::isIndirectCall`) - full run-time cost of one
call, same as any other, just resolved through a value instead of a
name.

Two deliberate limitations, both to keep this simple rather than because
of some deeper wall: no `unsubscribe` (a binding lives for the app's
whole lifetime, which is what a small desktop app almost always wants
anyway), and each signal's subscriber list has a fixed capacity
(`MAX_SUBSCRIBERS`) rather than growing without bound - ART has no
runtime-sized array allocation yet (every array literal still spells out
every element), so `makeSignal<T>` pre-fills a fixed-size placeholder
array once, hidden from callers, instead.

Building an ART app needs LLVM 18 (`llvm-18-dev` or equivalent) and the
Boehm GC (`libgc-dev`) installed - unlike Skia/lexbor/QuickJS neither is
a git submodule, and unlike the rest of this framework's dependencies
they're only ever required when `ARTISAN_APP_ART_SOURCE`/`app.art` is
actually configured: a C++/Go/JS-only project never pulls either in at
all (see the root `CMakeLists.txt`'s conditional `add_subdirectory(art)`
and `find_library(ARTISAN_GC_LIB ...)`).

## Using JavaScript

An `app.js` at the project root is auto-discovered and embedded, same as
`goapp/` - no flag needed. It runs against a DOM-like API, close to (but
not) a browser's:

```js
var list = document.querySelector("#list");
var items = document.querySelectorAll(".item");
var item = document.createElement("li");
item.textContent = "New";
list.insertBefore(item, items[0]);

var removed = list.removeChild(items[items.length - 1]); // still alive - re-appendable
list.appendChild(removed);

document.getElementById("agree").addEventListener("change", function (event) {
  console.log("checked:", event.target.hasAttribute("checked"));
});

document.getElementById("link").addEventListener("click", function (event) {
  event.preventDefault(); // suppresses the <a>'s navigation
});

item.classList.add("highlight");
item.style.color = "red";

requestAnimationFrame(function (timestampMs) {
  document.getElementById("status").textContent = "frame at " + timestampMs;
});
```

`Node` methods/properties: `tagName`, `nodeType` (1 element / 3 text -
also `Node.ELEMENT_NODE`/`Node.TEXT_NODE`), `textContent`,
`getAttribute`/`setAttribute`/`hasAttribute`/`removeAttribute`,
`classList.add`/`remove`/`toggle`/`contains`/`length`, `style.color`/
`backgroundColor`/`fontWeight`/`borderColor`/`borderWidth`/`cssText`
(exactly the five properties [Using CSS](#using-css) below supports,
merged into the cascade at the highest precedence, same as real inline
style), `dataset.fooBar` (real read/write/delete property syntax over a
node's `data-*` attributes - `fooBar` <-> `data-foo-bar`; `delete
node.dataset.foo` removes the attribute, and `Object.keys`/`for-in` see
exactly the node's `data-*` keys, camelCased - not iterable, though, no
`for-of`/spread, same as `children` below), plus the older method-based
`getData(name)`/`setData(name, value)` pair reaching the same
attributes, kept alongside `dataset` for existing callers,
`appendChild`/`insertBefore`/
`removeChild`/`remove()` (the removed node stays alive and
re-appendable), `cloneNode(deep)`, `matches(selector)`/
`closest(selector)`, `parentNode`/`nextSibling`/`previousSibling`/
`children`, `querySelector`/`querySelectorAll` (same selector grammar as
[Using CSS](#using-css) - combinators, attribute selectors, structural
pseudo-classes, comma-lists), `addEventListener(type, fn,
captureOrOptions)`/`removeEventListener` - any type string works;
`"click"`, `"change"` (checkbox/radio), `"input"` (text fields), and
`"keydown"` (whenever a focused input gets a key press) fire on their
own, and a script can fire any other type itself with `dispatchEvent`
below. `fn` receives a real event object (`type`/`target`/`bubbles`/
`cancelable`/`detail`/`preventDefault()`/`stopPropagation()`/
`stopImmediatePropagation()`/`defaultPrevented`, plus MouseEvent/
KeyboardEvent data - `clientX`/`clientY`/`ctrlKey`/`shiftKey`/`altKey`/
`metaKey`/`key`/`code`, `0`/`false`/`""` on an event that isn't one of
those - populated on `"click"` and `"keydown"`; calling
`preventDefault()` on `"keydown"` suppresses the built-in editing that
key would otherwise do - character insertion, backspace/delete, arrow/
home/end cursor movement, and the ctrl+a/c/x/v shortcuts alike - same as
a real browser's default keydown handling on an `<input>`), dispatched
through the usual capturing/target/bubbling
phases. `element.dispatchEvent(event)` fires a script-built
`new Event(type, {bubbles, cancelable})` or `new CustomEvent(type,
{detail, bubbles, cancelable})` (both default `bubbles`/`cancelable` to
`false`, matching real DOM) through that same machinery, returning
`false` if the event was cancelable and some listener called
`preventDefault()`, `true` otherwise. `document` has
`getElementById`/`querySelector`/`querySelectorAll`/`createElement`/
`createTextNode`. Globals: `console.log`/`warn`/`error`, `setTimeout`/
`setInterval`/`clearTimeout`/`clearInterval`, `requestAnimationFrame`/
`cancelAnimationFrame` (runs once before the next repaint, timestamp in
ms - a callback that calls `requestAnimationFrame` again, the normal way
to animate, schedules for the *next* one, never the current call).

`querySelectorAll` returns a plain array, a snapshot at call time -
appending/removing a child afterward doesn't retroactively change an
array you already have (matching real DOM: `querySelectorAll` is
specified as static, not live). `children` is the opposite on purpose -
a live HTMLCollection-like object whose `.length` and `children[i]`
both re-read the tree on every access, so a `var kids = node.children`
held from before an append/remove sees it happen. It isn't a real
`Array` - `Array.isArray(node.children)` is `false`, and there's no
`for...of`/`.forEach()`/spread (no `Symbol.iterator` - same as real
HTMLCollection, historically) - but `Object.keys`/`for...in`/
`Array.from` all work, and writing to an index (`node.children[0] = x`)
is a silent no-op rather than mutating the tree. Node identity does
work, though: `document.getElementById(id)` called twice, a dispatched
event's `.target`, a node handed back by `createElement` and then
re-found via `querySelector` - every one of those is the *same* wrapped
JS object as any other live reference to that node, so `===` between
them holds. The one place identity still doesn't extend is the `Event`
object itself - within one `dispatchEvent(event)` call, each listener
gets its own fresh event object (carrying the same
type/detail/bubbles/cancelable/target, not `event` itself), so
`event === event` across two listeners of the same dispatch is `false`
even though `event.target` on both is the same node. Separately, a
*second*, independently-obtained reference to a node removed through a
different reference can dangle if the reference that now owns it gets
garbage-collected first - hold onto the reference `removeChild`/
`remove()` actually returned if you plan to keep using the node.

## Using CSS

A `<style>` block anywhere in a page's markup can style elements by tag,
`.class`, `#id`, or a compound of these (`div.card`, `p#intro.warning`):

```html
<style>
  div.card { background-color: #f5f5f5; border-color: #ccc; border-width: 2px; }
  h1 { color: #2563eb; }
  #warning { color: red; font-weight: bold; }
</style>
<div class="card">
  <h1>Title</h1>
  <p id="warning">Careful!</p>
</div>
```

Selectors can chain with combinators - `div p` (descendant), `div > p`
(child), `a + b` (adjacent sibling), `a ~ b` (general sibling) - and a
compound can carry attribute selectors (`[href]` presence,
`[type="text"]` exact match, `[class~="card"]` one of a whitespace-
separated list of tokens - the general form of what `.card` does
specifically for the `class` attribute, `[href^="https://"]` prefix,
`[href$=".pdf"]` suffix, `[href*="example"]` substring anywhere,
`[lang|="en"]` exactly that value or that value followed by a hyphen
(`"en"` or `"en-US"`, not `"english"` - `lang` is real CSS's original
use case for this operator) - any of these can add a trailing `i` (or
`I`) flag, `[type="text" i]`, to compare case-insensitively (ASCII only)
instead of this file's usual case-sensitive default; there's no `s`
flag to opt back into case-sensitivity, since nothing here is
case-insensitive by default in the first place), structural
pseudo-classes (`:first-child`, `:last-child`, `:nth-child(2)`,
`:nth-child(even)`/`:nth-child(odd)`, and `:nth-of-type` with the same
argument grammar - literal integers and the even/odd keywords only, not
the full `an+b` algebra. `:nth-of-type` counts only same-tag siblings
- `p:nth-of-type(2)` is the second `<p>` among its siblings, whatever
else is mixed in between; `:nth-child` counts every element sibling
regardless of tag. There's no separate `:first-of-type`/`:last-of-type`
- write `:nth-of-type(1)` for the former; there's no general "last" form
without the full `an+b` algebra), `:not(...)` (negates a full,
comma-separated selector list, real CSS Level 4 semantics -
`li:not(.done)`, `:not(#hero)`, `:not([disabled])`, `:not(:first-child)`,
`:not(div > p)` (combinators inside `:not()` work), and `:not(a, b)`
(matches unless *either* alternative matches) all work, including
nesting - `:not(:not(.a))` is equivalent to `.a`. Specificity follows
real CSS: `:not(...)` contributes whichever of its alternatives is
*most* specific, not a flat pseudo-class point - `:not(#x)` counts as an
id (100), `:not(div)` as a tag (1), `:not(#x, div)` as an id (100, the
larger of the two)), and three interactive pseudo-classes,
`:hover`, `:focus`, and `:focus-within` - a `<style>` block rule like
`.card:hover { background-color: ... }` or
`#name:focus { border-color: ... }` re-resolves live as the mouse moves
or focus changes, same as real CSS, including bubbling to ancestors for
`:hover` (`.card:hover .title` matches while hovering anywhere inside
`.card`, not just `.card` itself) and `:focus-within` (`.form:focus-within
{ ... }` matches while focus is anywhere inside `.form`, not just on
`.form` itself) - plain `:focus` doesn't bubble the same way, only the
exact focused element. Comma-separated lists (`h1, .card`) work the same
way in `querySelector`/`querySelectorAll`/`matches`/`closest` as they do
in a `<style>` block - `:hover`/`:focus`/`:focus-within` parse there too,
but always evaluate false: those entry points have no live mouse/focus
state to match against, unlike a `<style>` block's cascade. Not
supported: anything beyond this bounded grammar.

`::before`/`::after` (the legacy single-colon `:before`/`:after` forms
work identically) generate text content via `content: "..."` (or
`'...'`) - `.card::before { content: "★ "; color: gold; }` prepends a
gold star to every `.card`; `content: none` explicitly generates
nothing, and (being a real, cascade-competing value rather than simply
"unset") can override a lower-specificity rule's own non-empty
`content`. Only a plain quoted string is supported - real CSS's
`attr()`, `counter()`, `open-quote`/`close-quote`, and every other
dynamic content form are not. The generated text becomes a plain text
node prepended/appended to the element's real children - it inherits
`color`/`font-weight` from that element the same way any of its real
text does, but (being plain text, not its own element) never has its
own box model: `background-color`/`border-color`/width/height/padding/
margin on a `::before`/`::after` rule are silently inert, the same
limitation a bare text node always has (see the box model paragraph
below). A pseudo-element combines with a pseudo-class on the same
compound normally (`.tab:hover::after { content: " ▾"; }`), but a
`:hover`-only `content`/`color` change on a *single-line* element (the
only kind whose own `:hover`/`:focus-within` hit region this renderer
computes at all - see below) that shifts its `color` away from that
element's own real text can make its hit region disappear the instant
it's hovered (this flow model hard-breaks a line wherever its text
color changes, mid-element or not, and a hit region is only ever
computed for a still-single-line element after that element finishes
rendering) - which then immediately un-hovers it next frame, flickering
indefinitely. Keep a `:hover`-triggered `::before`/`::after`'s `color`
matching its element's own to avoid this; only `content` and other
non-color properties changing under `:hover` are unaffected. `::before`/
`::after` only apply to elements built via the normal nested-children
path (`<div>`, `<span>`, `<li>`, headings, `<td>`/`<th>`, ...) - not
`<button>`/`<a>`/`<label>`/`<input>`, whose content is flattened to a
single string at a different point in the pipeline that generated
content isn't wired into.
`:hover`/`:focus`/`:focus-within` styling also only reaches elements the
renderer actually hit-tests - every
`<input>`/`<button>`/`<a>`/`<label>`/checkbox/radio, block-level
containers (`<div>`, `<p>`, `<li>`, ...), `<tr>`/`<td>`/`<th>`, and now
an inline element (`<span>`) too, as long as its own text fits on a
single line - a `<span>` whose content wraps across more than one line
gets no hit region at all for that render (rather than a wrong or
partial one), the one remaining gap in hit-testing coverage. A `<span>`
still separately inherits `:hover`/`:focus-within` from a
hoverable/focus-containing *ancestor* regardless (e.g. `p:hover span`),
same as ever. Painting is unaffected either way: an inline element never
applies box-model properties (see below) even under `:hover`, so
`background-color`/`border-color`/width/height/padding/margin changes on
a `:hover`'d `<span>` have no visual effect - only `color`/`font-weight`
(and anything else a `<span>`'s inline text painting already honors)
actually show up.

Properties: `color`, `background-color`, `font-weight` (`bold`/
`normal`), `border-color`, `border-width`, and a box model/flexbox pass -
`width` (px or `%`), `height` (px only), `padding`/`margin` (single-value
shorthand or the four `-top`/`-right`/`-bottom`/`-left` longhands), and,
for `display: flex` containers, `flex-direction` (`row`/`column`),
`justify-content` (`flex-start`/`center`/`flex-end`/`space-between`),
`align-items` (`flex-start`/`center`/`flex-end`/`stretch` - `stretch` is
the default, matching real CSS), `align-content`
(`flex-start`/`center`/`flex-end`/`space-between`/`space-around`/
`stretch` - unlike real CSS, `flex-start` rather than `stretch` is this
engine's own default for the property being *unset entirely*, so a page
that never mentions `align-content` keeps rendering exactly as it did
before this property existed; write `align-content: stretch` explicitly
to get real CSS's actual default behavior), `gap`, `flex-wrap`
(`nowrap`/`wrap`), and, on a flex item itself, `flex-grow`/`flex-shrink`
(unitless numbers;
default `0`/`1`, matching real CSS - items don't grow but do shrink to
fit unless told otherwise) and `flex-basis` (px, or `auto` to fall back
to the item's own `width`/`height` for the main axis, or its natural
content size). Colors accept `#rgb`, `#rrggbb`, `rgb(r, g, b)`, or a
small set of common keywords (`red`, `navy`, `gray`, ... - not the full
~150-name CSS list, see `ParseColor` in `css.cpp` for the exact set).
Cascade and specificity work the way you'd expect - `id` beats `class`
beats `tag`, a combinator chain's specificity sums across every compound
in it, later rules win ties, and each property resolves independently.
`color`/`font-weight` inherit to children that don't set their own;
every other property (including the whole box model/flexbox set) never
does. `node.style` (JS) exposes every one of these as a read/write
camelCase property (`style.paddingTop`, `style.flexGrow`, ...), same as
the original five - see [Using JavaScript](#using-javascript).

The box model and flexbox reach `kContainer` (`<div>`, `<p>`, `<span>`,
etc.), `<input>`/`<button>`/checkbox/radio, `<a>`/`<label>`,
`<table>`/`<td>`/`<th>`, `<hr>`, and `<img>`. A bare text node is the one
deliberate exception, and always will be: it has no tag/class/id of its
own for a selector to match, so it's never resolved its own CSS in the
first place (see `MakeText` in `widget_tree_builder.cpp`) - the same
reason real CSS doesn't give a text run its own independent box model
either. A text `<input>`/`<button>` (and a table cell's own `padding`, see below)
has its own built-in default padding (independent of CSS) so it stays
usable unstyled - `padding` on one of these *adds* to that default
rather than replacing it, unlike `kContainer`/`<a>`/`<label>`/`<table>`
where unset padding is genuinely zero (none of those is a boxy control
with a built-in inset of its own). Explicit `width`/`height` on any of
these overrides their normal content-driven (shrink-to-fit label)
sizing outright, the same as on a `kContainer`, and all participate
correctly as flex items (`flex-grow`/`flex-shrink`/`align-items: stretch`
all resize them, not just a `kContainer` child). `background-color`/
`border-color`/`border-width` now paint on `<a>`/`<label>`/`<table>`
too - a link's underline is drawn inside its padding box, same relative
position as before this existed. A checkbox/radio's fixed-size indicator
only honors `margin` - its width/height/padding aren't meaningful for a
native-style toggle glyph.

A `<table>`'s own box model works like `<a>`/`<label>`'s: it shrinks to
fit its grid's actual content width rather than filling available space
like a `kContainer` does, so an explicit `width` wider than the grid's
natural content leaves visible empty space inside the border rather than
stretching columns to fill it (real browsers redistribute the extra
width across columns instead - out of scope here). A `<td>`/`<th>`'s own
explicit `width`/`height` becomes a floor for its whole column/row (every
cell sharing that column/row is sized to match, same as real HTML table
layout), and a spanning cell's explicit width is honored too, distributed
across the columns it crosses exactly like its natural content width
already was. A cell's `margin` has no effect (matching real CSS - margin
doesn't apply to table cells); a `<table>`'s own `margin` does. A
`<caption>` is unaffected by any of this - it still renders as plain
text above the grid, at the table's original position rather than inside
its margin/padding/border box, a simplification real CSS doesn't make.

A `<hr>` honors margin, explicit `width`/`height` (overriding its default
of filling available width at a fixed 2px height), and
`background-color`/`border-color`/`border-width` - no `padding`, since a
rule has no content of its own to inset around. An `<img>`'s CSS
`width`/`height` override the `width`/`height` *attribute* outright, per
axis independently (real CSS precedence: the attribute is only a
UA-stylesheet-level default any stylesheet rule beats), and margin/
padding work the same as everywhere else. `background-color`/
`border-color` on an `<img>` only paint when both width and height are
already resolved *before* the image is decoded - from CSS and/or the
attribute, in any combination - since they need to land behind the image
rather than on top of it; when either axis instead relies on the image's
own natural aspect ratio (only known once decoding happens), background/
border are silently skipped for that render rather than painted in the
wrong order.

Flexbox itself is still bounded: `align-content` only has
anything to do in row direction with `flex-wrap: wrap` *and* an explicit
CSS `height` taller than what the wrapped lines need on their own - an
auto-height container's cross size already just is its content's, with
nothing left over to redistribute, matching real CSS. `flex-wrap` has no
effect in column direction (column's main axis - height - has no fixed
budget to wrap against, same reason its `justify-content`/`flex-grow`/
`flex-shrink` already don't apply there - `align-content` degenerates to
a no-op there too, for the same reason), and `flex-grow`/`flex-shrink`
resolve in a single pass, not the iterative min/max-width-clamping
algorithm real CSS uses (this engine has no `min-width`/`max-width`
property to clamp against, so a single pass is already exact). No margin
collapsing (adjacent vertical margins simply sum, rather than collapsing
to the larger one). Explicit `width`/`height` clamp to whatever space is
actually available rather than overflowing/scrolling - there's no
scroll-within-content concept beyond the page-level scroll `main.cpp`
already has.

`display: grid` is a bounded subset of CSS Grid, a second, independent
layout mode alongside flexbox (kContainer only, same as everything
above). `grid-template-columns`/`grid-template-rows` each take a plain
space-separated track list - a fixed pixel size (`100px`, or a bare
number), an `Nfr` fractional unit, an `N%` percentage (resolved against
the container's own available width, same basis `fr` itself already
uses - see below), or the `min-content`/`max-content` keywords, e.g.
`grid-template-columns: max-content 1fr;` (a classic content-sized-
sidebar-plus-flexible-remainder pattern) - or `repeat(N,
track-list)`, expanding inline to `N` copies of its (possibly
multi-track) inner list at that position in the outer list, e.g.
`grid-template-columns: 50px repeat(2, 1fr) 100px;` is exactly
`50px 1fr 1fr 100px`; `N` has to be a literal positive integer - real
CSS's `auto-fill`/`auto-fit` repeat counts need to know how much space
is available to decide how many copies fit, which isn't known yet at
parse time, so both are unsupported and the whole `repeat()` is skipped
like any other unrecognized token, and nesting `repeat()`/`minmax()`
inside `repeat()`'s own inner list isn't supported either - or
`minmax(min, max)`, resolving `max` exactly as an ordinary track of
whatever it is would (a fixed px, `Nfr`, `N%`, or `min-content`/
`max-content` - not real CSS's `auto`, and no nesting a second
`minmax()`/`repeat()` inside it either), then clamped up to `min` -
always a fixed px in this bounded subset, real CSS's own min-content/
max-content/`auto`/percentage for this half isn't supported - if that
leaves it smaller, e.g. `grid-template-columns: minmax(200px, 1fr) 3fr;`
(a classic never-shrink-below-200px sidebar). Unlike real CSS's own
iterative algorithm, a `minmax()` track's floor taking over more space
than its own share would've given it doesn't reclaim that shortfall
from *other* `fr` tracks - so several `minmax()` tracks whose floors
alone already exceed the container can sum wider than it, the same "a
floor reserves space, it doesn't negotiate for it" precedent a fixed
`grid-template-rows` track's own floor (below) already set. Otherwise
this still isn't real CSS's `auto` or named lines; a token matching
none of these forms (bare, or inside a `repeat()`) is skipped rather
than failing the whole list. `min-content`/`max-content`, `fr`, and `%`
only resolve for columns, against the container's own available width
(always known); on a row track, all four are treated as `auto`
(content-sized, the same as an unspecified row) instead, since
resolving any of them meaningfully on the row axis would need a known
total container height most grids never set - a `minmax()` row track is
the one exception: its own `min` half still applies as a floor,
identically to a plain fixed-px row track (see below), only its `max`
half is ignored the same way a bare `fr`/min-content/max-content/`%`
row track already is. A `min-content` column sizes to the narrowest a
single-column-span cell placed in it could be without overflowing (for
text, the width of its single widest unbreakable word); `max-content`
sizes to the widest such cell would be with no wrapping at all - a
spanning cell crossing either kind of column doesn't grow it, and
simply wraps at whatever width its crossed columns add up to, same as
it always has. `gap` (reused from
flexbox) applies between both columns and rows by default; `column-gap`/
`row-gap` each independently override it for just that one axis when
set (grid only - flexbox doesn't recognize either, only its own shared
`gap`), e.g. `column-gap: 40px; row-gap: 4px;` for wide column gutters
with tight row spacing. A child with no placement of its own (see
`grid-template-areas`/`grid-area` and `grid-column`/`grid-row` below)
auto-places into the next cell in document order, wrapping along
whichever axis `grid-auto-flow` names (`row`, the default, wraps to a
new row whenever the current one runs out of columns; `column` wraps to
a new column whenever the current one runs out of rows - see
`grid-auto-flow` below for the details). Every item stretches to fill
its own cell(s) on both axes by default - see `justify-items`/
`align-items` below for overriding that. A row named by
`grid-template-rows` is a *floor*, not an exact height (content taller
than it still reserves its own full height, same as every other
explicit-height property in this renderer); any row beyond however many
`grid-template-rows` named -
including every row when it's unset entirely, which also makes an unset
`grid-template-columns` fall back to a single full-width column -
auto-sizes to its own tallest (non-row-spanning) cell's natural content
height.

`grid-template-areas` names cells with plain quoted-string rows, e.g.
```css
grid-template-areas:
  "header header"
  "sidebar content"
  "footer footer";
```
(a literal `.` names an explicitly empty cell). A child's own
`grid-area: <name>` places it at whatever cells that name occupies -
including more than one, when a name appears in more than one cell, the
classic "header spans two columns" pattern above. `grid-area` also
accepts real CSS's numeric line-based shorthand instead of a name -
`grid-area: <row-start> / <column-start> / <row-end> / <column-end>`
(4 values) or `grid-area: <row-start> / <column-start>` (2 values, each
side an implicit span of 1) - each of the up-to-4 slash-separated parts
the same bare line number or `span N` `grid-column`/`grid-row` already
accept below; it expands directly into that same `grid-row`/
`grid-column` pair (literally the same fields, and cascade precedence,
a separate `grid-row: ...; grid-column: ...;` declaration on the same
selector would use), so everything the line-based longhands below say -
including their own limitations - applies here too. Real CSS's 1-value
and 3-value shorthand forms aren't supported (they infer a missing end
from the *start* side, a rule only meaningful for named lines, which
this bounded subset doesn't support either); a value with 1 or 3 slash-
separated parts is treated as a plain area name instead, same as any
other value the numeric form doesn't recognize. `grid-template-columns`/
`grid-template-rows` still size the resulting columns/rows exactly as
above when their own track count matches the area template's; otherwise
(including when they're unset entirely) columns fall back to equal
width and rows to auto content-sizing, so `grid-template-areas` alone
(no explicit tracks at all) is enough to get a working layout. A child
whose own `grid-area` doesn't match any name in the template - or that
has none at all - still auto-places into the next free cell in document
order the same as it would without any area template, so named and
unnamed children can mix; unlike an earlier version of this engine, the
unnamed ones do correctly route around cells a named child already
claimed (see `grid-auto-flow` below for the placement algorithm this
now shares with every other kind of explicit placement).

`grid-column`/`grid-row` place a child by numeric, 1-indexed grid line
instead of a name - line 1 is before the first track, line 2 between
the first and second, and so on. `grid-column: 2` places at column line
2 (span 1); `grid-column: 2 / 4` spans from line 2 to line 4 (2
columns); `grid-column: span 2` spans 2 columns with no explicit start,
auto-placed the same way an item with no placement at all is, just at
that span instead of always 1x1; `grid-column: 2 / span 2` combines an
explicit start with an explicit span. `grid-row` works identically for
rows - and so does `grid-area`'s own numeric line-based shorthand (see
above), which expands into exactly this same grid-column/grid-row pair.
A line number may be negative - real CSS's own "count backward from the
explicit grid's last line" form: `-1` is that last line, `-2` the one
before it, and so on, e.g. `grid-column: -2 / -1` (the actual last
column, the common "pin to the end" idiom) or `grid-column: span 2 / -1`
(the last two columns). "Explicit grid" here means however many tracks
`grid-template-columns`/`grid-template-rows` names (or the unset-falls-
back-to-1 default) - not the final, placement-extended column/row count
a later explicit placement might still grow. A bare negative value used
alone still means exactly what real CSS says it does, which is easy to
misread: `grid-column: -1` places the item's *start* at the last line
with the default span-1 *end*, landing it one track past the explicit
grid entirely (growing the grid by one more column), not in the last
column - use the two-value `-2 / -1` form for that instead. Mixing a
positive and a negative explicit line number in the same `<line> /
<line>` pair (e.g. `2 / -1`) isn't supported - that combination is
skipped like any other unrecognized value; a `span N` combined with
either sign is fine (`span 2 / -1` above). Real CSS's named lines aren't
supported on any of these. A `grid-template-columns`/
`grid-template-rows` track list too short for an explicit placement (or
a `grid-template-areas`-derived one) grows to fit - e.g. `grid-column:
5` with only 3 columns declared adds two more, equal-width, the same
fallback an unmatched track count anywhere else in this bounded subset
already uses. When only one of `grid-column`/`grid-row` has an explicit
start, the other defaults to line 1 rather than being auto-placed on
its own axis - real CSS's actual algorithm for a partially-explicit
item is a lot more elaborate than this implements. A `grid-area` match
always wins over `grid-column`/`grid-row` on the same element; below
that, every explicit placement (named or line-based) is resolved first,
in document order, and every auto-placed sibling is placed only after,
scanning for cells none of them already claimed (see `grid-auto-flow`
below for the full placement algorithm).

`justify-items`/`align-items` control whether a grid item stretches to
fill its own cell(s) (the default, and what every item did before these
existed) or keeps its own natural, fit-content size and is
start/center/end-positioned within the cell instead -
`justify-items: center` on a grid of narrower-than-their-cell buttons
centers each one horizontally, `align-items: end` bottom-aligns them
vertically, and so on. `align-items` is genuinely the same property
flexbox's own cross-axis alignment already uses (interpreted
differently depending on which layout mode is active on that
container - a container is always exactly one or the other), so it
accepts either keyword spelling, flexbox's `flex-start`/`flex-end` or
grid's own `start`/`end`, in both contexts; `justify-items` (grid only -
flexbox has no equivalent, since `justify-content` distributes free
space among items along the main axis rather than positioning one item
within its own single line) only accepts grid's `start`/`end`. Real
CSS's per-item `justify-self`/`align-self` override either of these for
one item alone, the same four keywords (grid's `start`/`end` spelling
only - unlike `align-items`, this engine never wires `align-self` into
flexbox, so there's no `flex-start`/`flex-end` form to also accept
here) - `auto` (the only other value `justify-self`/`align-self` accept
here) means "unset", falling back to the container's own
`justify-items`/`align-items` for that item, exactly as it always did
before these two properties existed. A non-stretched item's own width
still feeds back into row auto-sizing correctly (a narrower item can
wrap its text across more lines than a stretched one would, becoming
taller) - it's only ever the *positioning* within the cell that's
skipped when stretch doesn't apply, not the sizing math surrounding it.

`grid-auto-flow` controls which axis auto-placement fills first:
`row` (the default, and the only behavior before this property existed)
wraps to a new row once the current one runs out of columns;
`column` wraps to a new column once the current one runs out of rows -
using however many rows `grid-template-rows` declares as the wrap
point (falling back to 1 - one item per column - when it's unset,
mirroring how an unset `grid-template-columns` falls back to a single
column for row-flow). Placement now happens in two passes, matching
real CSS's own algorithm: every item with a definite position (a
`grid-area` match, or an explicit `grid-column`/`grid-row`) is placed
first, in document order, each claiming its own cells in a shared
occupancy grid; every remaining item is then auto-placed, in document
order, scanning that same occupancy grid for the next free cell of its
own size - so an auto-placed item correctly skips cells a definite
sibling already claimed, rather than just counting forward by raw item
index (a gap this bounded subset used to have entirely). The `dense`
packing modifier (`grid-auto-flow: row dense`/`column dense`, or bare
`dense`, implying row) is now supported too: it restarts that scan from
the very beginning of the grid for every auto-placed item instead of
only ever continuing forward from the previous one's own position (the
default, "sparse" behavior) - so a later, smaller item can fill a hole
an earlier, wider one left open, rather than leaving it empty forever.

`grid-template-columns: subgrid` lets a nested grid adopt its parent's
column tracks instead of sizing its own - columns only, mirroring the
`fr`/`min-content`/`max-content` columns-only precedent above (a nested
`grid-template-rows: subgrid` isn't supported; a subgrid's rows are
still ordinary content-auto-sized rows). It only takes effect on a
child that's both `display: grid` and placed into the parent via
`grid-column` (a `grid-area` match works too, since that resolves to
the same underlying column range) - an un-placed or non-grid child
ignores the keyword and gets ordinary auto-placement/layout instead.
The parent computes its own column widths as usual, then hands the
slice spanned by the subgrid child's own placement down as that
child's fixed column tracks, so e.g. a 3-column-wide subgrid child
placed at `grid-column: 1 / 4` inside a `1fr 2fr 1fr` parent gets
exactly those three column widths for its own `1fr 2fr 1fr`-equivalent
tracks - its own cells' boundaries then line up exactly with the
parent's, which is the entire point of subgrid. A subgrid child's own
item count is clamped to however many columns it inherited (extra
items past that just pile into the last inherited column rather than
adding new ones of their own), since - unlike every other track-list
mismatch in this bounded subset, which grows to fit - there's no
sensible fallback width for a subgrid column beyond what the parent
explicitly handed down. Real CSS's subgrid also inherits line names
and can subgrid only one axis while sizing the other normally in the
same declaration; neither is supported here, only the single
all-or-nothing `subgrid` keyword on `grid-template-columns`.

`justify-content`/`align-content` distribute any leftover space the
resolved columns/rows don't already consume, as a *group* - genuinely
the same properties flexbox's own main-axis `justify-content` and
multi-line `align-content` already are (same fields, same accepted
keywords: `flex-start`/`start`/`center`/`flex-end`/`end`/
`space-between`/`space-around`/`space-evenly` for `justify-content`,
plus `stretch` for `align-content` only - main-axis space in this
bounded subset is never "grown into", only repositioned/gapped, so
`justify-content` has no `stretch` of its own, matching real CSS),
interpreted differently here the same way `align-items` already is
depending on which layout mode is active - `start`/`end` are accepted
alongside `flex-start`/`flex-end` as the exact same value (this engine
has no writing-mode/RTL distinction for either to differ over, unlike
real CSS). `justify-content` only ever has anything to redistribute
when the columns don't already fill the container's own width on their
own - a `fr` track or the equal-width fallback (unset/mismatched
`grid-template-columns`) already do, by construction, so it only
matters with fixed-pixel, min-content/max-content, or subgrid-adopted
columns narrower than the container; `space-between` widens the gaps
between columns rather than the columns themselves, `space-around` adds
a half-size gap at each outer edge too, and `space-evenly` makes every
gap - outer edges included - exactly equal. `align-content` only ever
has anything to redistribute when the container has an explicit CSS
`height` taller than what the rows naturally needed - this bounded
subset's rows are otherwise always content-auto-sized (see
`grid-template-rows` above), so there's no other source of extra height
for it to distribute; `stretch` grows every row track itself by an
equal share of that leftover height (so an `align-items: stretch` item
placed in one genuinely gets taller, not just repositioned), the other
keywords (now including `space-evenly`, alongside the pre-existing
`space-around`) just reposition/gap the rows without changing their own
height. Real CSS's `normal` keyword isn't supported by either property
here.

`<style>` only works inside `<body>` - a `<head><style>` never reaches
the document at all, since `<head>` itself is discarded (see
`html_document.cpp`). There's also no shared stylesheet across pages -
each page's markup is compiled independently, so a `<style>` block
meant for every page needs pasting into each one (or turned into a
`components`-style fragment, see `artisan-cli component`).

## Using forms

`<input type="checkbox">` and `<input type="radio">` render as a small
toggle indicator (a square for checkbox, a circle for radio) - checked
state comes from (and clicking one toggles) the `checked` attribute, read
via the same `GetAttribute`/`SetAttribute` every other element uses:

```html
<input type="checkbox" id="agree">
<label for="agree">I agree</label>

<input type="radio" name="color" id="red" checked>
<label for="red">Red</label>
<input type="radio" name="color" id="blue">
<label for="blue">Blue</label>
```

```cpp
Node *agree = document.FindById("agree");
bool checked = agree->GetAttribute("checked") != nullptr;
```

Radios sharing a `name` attribute are mutually exclusive - checking one
unchecks the others in that group automatically. `<label for="...">`
makes its target clickable too (toggle a checkbox/radio, focus a text
input, or click a button) - useful since checkbox/radio have no label
text of their own.

A few things this doesn't do yet: `<label>` only works via `for="id"` -
wrapping an input directly (`<label><input ...> text</label>`) isn't
supported. Every box-type widget (`<input>`, `<button>`, checkbox/radio,
`<label>`) is block-level, same as it's always been for input/button - a
label doesn't yet sit on the same visual line as the control it labels.
If markup marks more than one radio in the same `name` group as `checked`,
all of them render checked until the user actually clicks one (a real
browser resolves this at parse time; this doesn't). `<select>` and
`<textarea>` aren't supported yet.

## APT repository (Debian / Ubuntu)

Every tagged release also publishes to an APT repository, updated
automatically:

```bash
curl -fsSL https://geovannyavelar.github.io/artisan/pubkey.gpg | sudo gpg --dearmor -o /usr/share/keyrings/artisan.gpg
echo "deb [signed-by=/usr/share/keyrings/artisan.gpg] https://geovannyavelar.github.io/artisan stable main" | sudo tee /etc/apt/sources.list.d/artisan.list
sudo apt update
sudo apt install artisan-cli
```

There's also an `unstable` distribution, rebuilt from the latest commit on
`main` on every push (see the "Unstable (latest main)" release) — always a
single package, no version history, and may be broken. Use the same key,
pointed at `unstable` instead of `stable`:

```bash
curl -fsSL https://geovannyavelar.github.io/artisan/pubkey.gpg | sudo gpg --dearmor -o /usr/share/keyrings/artisan.gpg
echo "deb [signed-by=/usr/share/keyrings/artisan.gpg] https://geovannyavelar.github.io/artisan unstable main" | sudo tee /etc/apt/sources.list.d/artisan-unstable.list
sudo apt update
sudo apt install artisan-cli
```

Don't add both `.list` files at once — they publish the same package name at
different, conflicting versions, and apt will just pick whichever it prefers.

## Building a .deb (manual, local)

```bash
./package/build_deb.sh
```

Produces `dist/artisan-cli_<version>_amd64.deb`. Requires Skia already
built (`./build_skia.sh`) - the script packages what's there, it doesn't
build Skia itself.

`artisan-cli` isn't self-contained: every `artisan-cli build` re-invokes
cmake against this whole checkout, Skia's prebuilt static libraries
included, so the .deb bundles a copy of the tree (installed to
`/usr/lib/artisan`) rather than just the binary - expect several hundred
MB. Installing it (`sudo dpkg -i dist/artisan-cli_*.deb`) pulls in the
toolchain `artisan-cli build` itself shells out to (cmake, ninja, g++,
pkg-config, and the freetype/fontconfig/sdl2 dev packages) via the
package's declared dependencies, so `apt install ./artisan-cli_*.deb` on a
clean machine is enough to get straight to `artisan-cli new && artisan-cli
build --run`.

The version embeds the commit the package was built from (e.g.
`0.0.1~git20260825.d90e8f8`) so repeated local builds are distinguishable
- this is a local packaging script only, not tied to any CI or publishing
process.
