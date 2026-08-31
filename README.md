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
compound can carry attribute selectors (`[href]`, `[type="text"]`),
structural pseudo-classes (`:first-child`, `:last-child`,
`:nth-child(2)`, `:nth-child(even)`/`:nth-child(odd)` - literal
integers and the even/odd keywords only, not the full `an+b` algebra),
and the two interactive pseudo-classes, `:hover` and `:focus` - a
`<style>` block rule like `.card:hover { background-color: ... }` or
`#name:focus { border-color: ... }` re-resolves live as the mouse moves
or focus changes, same as real CSS, including `:hover` bubbling to
ancestors (`.card:hover .title` matches while hovering anywhere inside
`.card`, not just `.card` itself) - `:focus` doesn't bubble the same way
(no `:focus-within`). Comma-separated lists (`h1, .card`) work the same
way in `querySelector`/`querySelectorAll`/`matches`/`closest` as they do
in a `<style>` block - `:hover`/`:focus` parse there too, but always
evaluate false: those entry points have no live mouse/focus state to
match against, unlike a `<style>` block's cascade. Not supported:
anything beyond this bounded grammar (`:nth-of-type`, `~=`/`^=`/`$=`
attribute operators, `:focus-within`, etc). `:hover`/`:focus` styling
also only reaches elements the renderer actually hit-tests - every
`<input>`/`<button>`/`<a>`/`<label>`/checkbox/radio, and block-level
containers (`<div>`, `<p>`, `<li>`, ...), but not yet an inline element
(`<span>`) or a table row/cell on its own (though a `<td>` still
inherits `:hover` correctly from a hoverable *ancestor*, e.g.
`.row:hover td` - it just can't be the direct target of the mouse
itself).

Properties: `color`, `background-color`, `font-weight` (`bold`/
`normal`), `border-color`, `border-width`, and a box model/flexbox pass -
`width` (px or `%`), `height` (px only), `padding`/`margin` (single-value
shorthand or the four `-top`/`-right`/`-bottom`/`-left` longhands), and,
for `display: flex` containers, `flex-direction` (`row`/`column`),
`justify-content` (`flex-start`/`center`/`flex-end`/`space-between`),
`align-items` (`flex-start`/`center`/`flex-end`/`stretch` - `stretch` is
the default, matching real CSS), `gap`, `flex-wrap` (`nowrap`/`wrap`),
and, on a flex item itself, `flex-grow`/`flex-shrink` (unitless numbers;
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

The box model and flexbox are `kContainer`-only for now (a `<div>`,
`<p>`, `<span>`, etc. - not yet `<input>`/`<table>`/text directly).
Flexbox itself is still bounded: no CSS Grid, no `align-content` (a
wrapped flex container's multiple lines just stack in DOM order along
the cross axis, no extra distribution control), `flex-wrap` has no
effect in column direction (column's main axis - height - has no fixed
budget to wrap against, same reason its `justify-content`/`flex-grow`/
`flex-shrink` already don't apply there), and `flex-grow`/`flex-shrink`
resolve in a single pass, not the iterative min/max-width-clamping
algorithm real CSS uses (this engine has no `min-width`/`max-width`
property to clamp against, so a single pass is already exact). No margin
collapsing (adjacent vertical margins simply sum, rather than collapsing
to the larger one). Explicit `width`/`height` clamp to whatever space is
actually available rather than overflowing/scrolling - there's no
scroll-within-content concept beyond the page-level scroll `main.cpp`
already has.

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
