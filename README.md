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
number) and/or an `Nfr` fractional unit, e.g.
`grid-template-columns: 100px 1fr 2fr;` - not real CSS's `repeat()`,
`minmax()`, `auto`, percentage tracks, or named lines; a token matching
neither form is skipped rather than failing the whole list. `fr` only
resolves for columns, against the container's own available width
(always known); a fractional row track is treated as `auto`
(content-sized) instead, since resolving it meaningfully would need a
known total container height most grids never set. `gap` (reused from
flexbox) applies between both columns and rows - there's no separate
`row-gap`/`column-gap`. A child with no placement of its own (see
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
classic "header spans two columns" pattern above - real CSS's numeric
`grid-area: row / col / row-end / col-end` line-based shorthand isn't
supported, only the named form. `grid-template-columns`/
`grid-template-rows` still size the resulting columns/rows exactly as
above when their own track count matches the area template's; otherwise
(including when they're unset entirely) columns fall back to equal
width and rows to auto content-sizing, so `grid-template-areas` alone
(no explicit tracks at all) is enough to get a working layout. A child
whose own `grid-area` doesn't match any name in the template - or that
has none at all - still auto-places into the next free-by-index cell in
document order the same as it would without any area template, so named
and unnamed children can mix, though placement for the unnamed ones
doesn't try to route around cells a named child already claimed.

`grid-column`/`grid-row` place a child by numeric, 1-indexed grid line
instead of a name - line 1 is before the first track, line 2 between
the first and second, and so on. `grid-column: 2` places at column line
2 (span 1); `grid-column: 2 / 4` spans from line 2 to line 4 (2
columns); `grid-column: span 2` spans 2 columns with no explicit start,
auto-placed the same way an item with no placement at all is, just at
that span instead of always 1x1; `grid-column: 2 / span 2` combines an
explicit start with an explicit span. `grid-row` works identically for
rows. Real CSS's named lines and negative (counted-from-the-end) line
numbers aren't supported, and neither is `grid-area`'s equivalent
numeric `row / col / row-end / col-end` form - only these two named
longhands parse line-based placement. A `grid-template-columns`/
`grid-template-rows` track list too short for an explicit placement (or
a `grid-template-areas`-derived one) grows to fit - e.g. `grid-column:
5` with only 3 columns declared adds two more, equal-width, the same
fallback an unmatched track count anywhere else in this bounded subset
already uses. When only one of `grid-column`/`grid-row` has an explicit
start, the other defaults to line 1 rather than being auto-placed on
its own axis - real CSS's actual algorithm for a partially-explicit
item is a lot more elaborate than this implements. A `grid-area` match
always wins over `grid-column`/`grid-row` on the same element; below
that, explicit placement of any kind (named or line-based) is exactly
as prone to overlapping an auto-placed sibling as `grid-template-areas`
already documents above, for the same reason (auto-placement is a
simple next-free-index counter, not occupancy-aware).

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
CSS's per-item `justify-self`/`align-self` overrides aren't supported -
only these two container-level properties, the same scope flexbox's own
`align-items` already has. A non-stretched item's own width still feeds
back into row auto-sizing correctly (a narrower item can wrap its text
across more lines than a stretched one would, becoming taller) - it's
only ever the *positioning* within the cell that's skipped when
stretch doesn't apply, not the sizing math surrounding it.

`grid-auto-flow` controls which axis auto-placement fills first:
`row` (the default, and the only behavior before this property existed)
wraps to a new row once the current one runs out of columns;
`column` wraps to a new column once the current one runs out of rows -
using however many rows `grid-template-rows` declares as the wrap
point (falling back to 1 - one item per column - when it's unset,
mirroring how an unset `grid-template-columns` falls back to a single
column for row-flow). Real CSS's `dense` packing modifier (re-filling
earlier holes an explicit placement left open, rather than always
moving forward) isn't supported - this engine's auto-placement has
never tracked cell occupancy at all (see `grid-template-areas` and
`grid-column`/`grid-row` above for where that same gap already applies
to explicit placement), so dense packing has nothing to do; a `dense`
keyword still parses without breaking the property (`grid-auto-flow:
column dense` still flows by column), it just has no additional effect
of its own.

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
