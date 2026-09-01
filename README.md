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
`app.art` - one file at the project root, like `app.js`, not a whole
module directory the way `goapp/` is (ART has no multi-file/import system
yet) - is compiled by the standalone `art` compiler and linked straight
into the binary, the same "runs compiled, not interpreted" deal
`ARTISAN_APP_CPP_SOURCES`/`ARTISAN_APP_GO_SOURCE` get.

```bash
artisan-cli new my-app --lang art
```

scaffolds `app.art` with every DOM function below already `declare`d and
an empty `setupApp`. An ART app must define exactly this function:

```ts
function onButtonClick(): void {
  // ...
}

function setupApp(document: Node): void {
  let button: Node = ArtFindById(document, "my-button");
  if (!ArtIsNull(button)) {
    ArtSetOnClick(button, onButtonClick);
    ArtSetTextContent(button, "Ready");
  }
}
```

`setupApp` runs once whenever a page loads, same as `SetupApp(Node&)`/
Go's `ArtisanSetupApp`/a script's top-level code all are. `Node` is an
opaque foreign type (`declare type Node;`) - a plain handle ART code
passes around but never constructs or looks inside; the DOM API itself is
a curated subset of `include/node_c_api.h` re-exposed as ART-callable
`declare function`s (`include/art_bridge.h` has the exact signatures -
copied into every scaffolded `app.art`): `ArtDocument`, `ArtFindById`,
`ArtQuerySelector`, `ArtIsNull` (the only way to test a lookup for "no
match" - ART has no null literal of its own to compare against),
`ArtGetTextContent`/`ArtSetTextContent`, `ArtGetAttribute`/
`ArtHasAttribute`/`ArtSetAttribute`, `ArtChildCount`/`ArtChildAt`, and
`ArtSetOnClick`.

`ArtDocument()` returns the same root `Node` `setupApp`'s own parameter
is - the current page's document, whatever it is at the moment of the
call. It exists specifically for a handler (see `ArtSetOnClick` below):
unlike `setupApp`, a handler takes no arguments, so without a way to ask
for the document directly it would have no path to the DOM at all except
whatever a top-level `let`/`const` could carry forward - and a `Node`
can't be one of those (see below). `include/art_bridge_context.h`
tracks which `Node` it currently points to, updated by `main.cpp` once
per page load/navigation (the same place `node_c_api_bridge.h`'s
`SetGoTimerContext` already is) - `ArtIsNull` on its result is `true`
only in the narrow window between one page's tree being torn down and
the next one finishing its build, which a handler invoked from a live
page will never actually observe.

`ArtSetOnClick(node, handler)` registers a click handler - `handler` isn't
a call, just a bare reference to a top-level `function ...(): void`
(`void` return - the only function-pointer-shaped value ART has, written
as a type like `() => void`). Passing a function by name this way needs
no closures or heap boxing the way a real callback usually would: ART
functions can't capture outer variables in the first place, so the
reference is just that function's own compiled address, handed straight
to `Node::SetOnClick` as a plain C function pointer - contrast Go's
`ArtisanNodeSetOnClick`, which has to carry a `uintptr_t` handle through a
Go-side registry (`cgo.Handle`) instead, since a Go closure can't produce
a raw callable address like this.

`ArtAddEventListener(node, eventType, handler, capture)` is the general
form - any event type (`"keydown"`, `"change"`, `"input"`, ...), and
unlike `ArtSetOnClick` it *stacks*: registering a second listener for the
same node/type doesn't replace the first, both run (`Node::AddEventListener`,
same as real `addEventListener`). Its `handler` takes the `Event` itself
as one parameter - `(event: Event) => void` rather than `() => void` -
which is why the handler type isn't fixed to zero arguments: `handler`'s
matching a function's actual parameter list, checked structurally the
same way any other type is (a `() => void` where a `(event: Event) =>
void` is expected, or vice versa, is a compile error, not a silent
mismatch). `Event` is another opaque foreign type (`declare type Event;`),
read through its own curated `declare function`s: `ArtEventType`,
`ArtEventTarget` (a `Node`), `ArtEventDetail` (see `ArtDispatchEvent`
below), `ArtEventBubbles`/`ArtEventCancelable`,
`ArtEventPreventDefault`/`ArtEventDefaultPrevented`,
`ArtEventStopPropagation`/`ArtEventStopImmediatePropagation` (the latter
also implies the former - stopping "immediately" means neither any
remaining listener at the current node nor any further ancestor gets a
turn, so e.g. a second `ArtAddEventListener` on the *same* node/type
never runs, not just an ancestor's - same as real DOM), and the
MouseEvent/KeyboardEvent data (`ArtEventClientX`/`ArtEventClientY`,
`ArtEventCtrlKey`/`ArtEventShiftKey`/`ArtEventAltKey`/`ArtEventMetaKey`,
`ArtEventKey`/`ArtEventCode`) - all copied into every scaffolded
`app.art`. Neither stop function has a corresponding getter (no
`ArtEventPropagationStopped`/`ArtEventImmediatePropagationStopped`) -
matching real DOM's own API, which doesn't expose "was propagation
already stopped" back to script either, only the action.

`ArtRemoveEventListener(node, eventType, handler, capture)` removes every
listener matching all four exactly - a mismatched call (wrong handler,
wrong `capture`, or one never added at all) is a safe no-op, same as real
`removeEventListener`. `Node::RemoveEventListener` has no built-in notion
of "the same handler" (`EventHandler` is a type-erased `std::function`,
not comparable on its own) - it takes a predicate instead, and
`art_bridge.cpp` supplies one via `std::function::target<T>()`, the exact
mechanism `node_c_api.cpp`'s Go path (`GoCallback`) and `js_engine.cpp`'s
JS path (`JsCallback`) already use for the same reason, just recovering a
named wrapper class around ART's raw function pointer instead of a Go
handle or a JS closure - `ArtAddEventListener` above stores one of these
(not a bare lambda, which has no nameable type to recover later)
specifically so removal can find it again.

`ArtDispatchEvent(node, eventType, bubbles, cancelable, detail)` is the
other side of general events - unlike everything above (which all react
to events something else fires), this actually fires one, any type at
all, not just the built-in click/change/input this Node model already
knows to fire (`Node::DispatchEvent`, same signature/semantics: the
usual capturing/target/bubbling walk, returning `false` if the event was
cancelable and some listener called `ArtEventPreventDefault`). `detail`
is where a real `CustomEvent`'s payload lives - genuinely untyped even in
the underlying C++ (`Event::detail` is a bare `const void*`, "carried
through the dispatch walk unexamined" no matter which binding constructs
it), so representing it for a statically-typed language without generics
means picking one concrete type rather than a real generic payload: ART
uses `string` for its own dispatches (pass `""` for none), which costs
nothing extra to wire up - an ART string value is already a heap-
allocated, never-freed pointer with a stable ABI shape, so `detail` is
just that same pointer handed to `Node::DispatchEvent` as-is, and
`ArtEventDetail` casts it straight back with no copy. This only round-
trips correctly on an event *ART itself* dispatched, though: `detail`
carries no type tag, so a listener has no way to tell a JS/Go dispatch's
own detail (typically a boxed script value there) apart from ART's
string shape - the same "each binding owns its own interpretation"
contract `detail` always had, just spelled out for ART specifically.
`ArtEventDetail` on an event with no detail at all (`nullptr` - every
event this Node model fires internally, or an `ArtDispatchEvent` call
with `detail ""`) reads back as `""`, the same "can't represent null"
convention `ArtGetAttribute` already uses for an unset attribute.

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

Its initializer must be a literal number/boolean/string, not a call,
arithmetic, an object/array literal, or another global - ART has no
static-initialization-order mechanism to run arbitrary code before
`setupApp`/`main`, and there'd be nowhere to put the result anyway for an
array/interface global, whose value is a `malloc`'d heap allocation, not
a compile-time constant. This also means a global can only be `number`/
`boolean`/`string` - no `Node`, array, or interface globals - so a
handler still can't stash a `Node` it found earlier in one directly; it
calls `ArtDocument()`/`ArtFindById` again instead (cheap - both are
simple pointer lookups, not a real query), the same way it would reach
the DOM at all in the first place.

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
`interface` exactly - no excess or missing fields), and the parameterized
handler type described above (`(p0: T0, p1: T1, ...) => void`, structural
like everything else - parameter names are decorative, only the types and
their order are checked); prefix `++`/`--` only (no postfix). No
classes, real closures, generics, `any`, union types, or garbage
collector - every array/object/string is a heap allocation (`malloc`)
that's never freed, matching this whole framework's "native,
ahead-of-time, no runtime" philosophy rather than the rest of a real
TypeScript. See the doc comments in `art/*.h`/`art/*.cpp` for the exact
grammar and type-checking rules.

Building an ART app needs LLVM 18 installed (`llvm-18-dev` or
equivalent) - unlike Skia/lexbor/QuickJS this isn't a git submodule, and
unlike the rest of this framework's dependencies it's only ever required
when `ARTISAN_APP_ART_SOURCE`/`app.art` is actually configured: a
C++/Go/JS-only project never pulls LLVM in at all (see the root
`CMakeLists.txt`'s conditional `add_subdirectory(art)`).

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
