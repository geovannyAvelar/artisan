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
style), `getData(name)`/`setData(name, value)` (a `data-*` attribute,
`fooBar` <-> `data-foo-bar` - method-based, not true
`dataset.fooBar` property syntax), `appendChild`/`insertBefore`/
`removeChild`/`remove()` (the removed node stays alive and
re-appendable), `cloneNode(deep)`, `matches(selector)`/
`closest(selector)`, `parentNode`/`nextSibling`/`previousSibling`/
`children`, `querySelector`/`querySelectorAll` (same bounded selector
grammar as Using CSS - one compound selector, no combinators),
`addEventListener(type, fn, captureOrOptions)`/`removeEventListener` -
any type string works; `"click"`, `"change"` (checkbox/radio), and
`"input"` (text fields) fire on their own, and a script can fire any
other type itself with `dispatchEvent` below. `fn` receives a real event
object (`type`/`target`/`bubbles`/`cancelable`/`detail`/
`preventDefault()`/`stopPropagation()`/`stopImmediatePropagation()`/
`defaultPrevented`), dispatched through the usual capturing/target/
bubbling phases. `element.dispatchEvent(event)` fires a script-built
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

A few real gaps: `children`/`querySelectorAll` return a
plain array, a snapshot at call time, not a live list - and every
wrapped node is a fresh JS object per call, so `===` between two
references to the same underlying node is always `false` (this includes
a dispatched event's `.target`, and - within one `dispatchEvent(event)`
call - `event` itself vs. what each listener actually receives); compare
`tagName`/`getAttribute` values, or hold onto one reference, instead of
relying on identity. That also means a *second*, independently-obtained
reference to a node removed through a different reference can dangle if
the reference that now owns it gets garbage-collected first - hold onto
the reference `removeChild`/`remove()` actually returned if you plan to
keep using the node.

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

Five properties are supported: `color`, `background-color`, `font-weight`
(`bold`/`normal`), `border-color`, `border-width`. Colors accept `#rgb`,
`#rrggbb`, `rgb(r, g, b)`, or a common keyword (`red`, `navy`, `gray`,
...). Cascade and specificity work the way you'd expect - `id` beats
`class` beats `tag`, later rules win ties, and each property resolves
independently (a tag rule's `color` and a later class rule's
`background-color` on the same element both apply). `color`/`font-weight`
inherit to children that don't set their own; `background-color`/`border`
never do.

A few things this doesn't do: no combinators (`div p`, `div > p`), no
pseudo-classes/attribute selectors, and `<style>` only works inside
`<body>` - a `<head><style>` never reaches the document at all, since
`<head>` itself is discarded (see `html_document.cpp`). There's also no
shared stylesheet across pages - each page's markup is compiled
independently, so a `<style>` block meant for every page needs pasting
into each one (or turned into a `components`-style fragment, see
`artisan-cli component`).

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
