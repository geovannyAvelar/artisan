// artisan-cli - the single entry point that ties artisan's whole
// workflow together: scaffolding a new project (`new`), and compiling
// markup (artisanc), native C++ app code, and an optional embedded
// JavaScript file (embed_text) into one native binary (`build`).
//
// `build` does not reimplement compilation or linking itself - Skia,
// lexbor, and QuickJS all need to be found and wired together correctly,
// and the project's own CMakeLists.txt already knows how to do that. What
// it actually does is discover a project directory's files (see
// DiscoverProject) and turn them into the right CMake configure + build
// invocation, substituted in via cache variables (ARTISAN_UI_SOURCES,
// ARTISAN_APP_CPP_SOURCES, ARTISAN_APP_JS_SOURCE, ARTISAN_APP_GO_SOURCE)
// that CMakeLists.txt exposes for exactly this purpose. There's no mode
// for naming individual files by hand - always a project directory, so
// there's exactly one way a project is laid out.
//
// `new` writes out a starting point for one native language - C++
// (src/main.cpp, the default), with --lang go, Go (goapp/), or with
// --lang art, ART (app.art, see art/) - a project uses exactly one of
// these, never more than one (see DiscoverProject). Script (app.js) is
// orthogonal to that choice and can layer on any of them.
//
// `component` scaffolds a smaller, reusable pairing within an existing
// project: a markup fragment (components/<name>.html, meant to be pasted
// into a page - it isn't itself a routable page) and a matching
// Setup_<name>(Node&) (src/components/<name>.cpp) the page's own SetupApp
// calls explicitly with the Node the fragment landed in. There's no
// automatic markup-embedding here (artisanc compiles each page
// independently) - this is a naming/wiring convention, not new machinery.

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

void PrintTopUsage() {
  std::cerr << "usage: artisan-cli <command> [<args>]\n\n"
               "commands:\n"
               "  new <project-dir>              Scaffold a new native project.\n"
               "  component <project-dir> <name> Scaffold a reusable\n"
               "                                  component in an existing\n"
               "                                  project.\n"
               "  build                           Compile a project into a binary.\n\n"
               "Run `artisan-cli <command>` with no further arguments for\n"
               "that command's own usage.\n";
}

void PrintBuildUsage() {
  std::cerr
      << "usage: artisan-cli build <project-dir> [--build-dir <dir>]\n"
         "                         [-o <output>] [--run]\n\n"
         "<project-dir> is the layout artisan-cli new scaffolds - its files\n"
         "are discovered automatically, never named one by one:\n"
         "  <project-dir>/pages/**/*.html   every page in the bundle,\n"
         "        named after its path relative to pages/, folders and all\n"
         "        (Next.js-style): pages/about.html becomes \"about\";\n"
         "        pages/settings/profile.html becomes \"settings/profile\";\n"
         "        pages/settings/index.html becomes \"settings\" (a folder's\n"
         "        own index.html is that folder's route, same as the\n"
         "        project root's). pages/index.html, if present, is the\n"
         "        page the app opens on. An <a href=\"...\"> anywhere in\n"
         "        the bundle can navigate to any other page by name.\n"
         "  <project-dir>/src/**/*.cpp      native C++ source, if the\n"
         "        project uses that language - nesting is just\n"
         "        organization here (not a route).\n"
         "  <project-dir>/goapp/            a native Go app instead, if the\n"
         "        project uses that language - its own go.mod/main.go (see\n"
         "        go/artisango), compiled ahead of time, never combined\n"
         "        with src/**/*.cpp or app.art in the same project.\n"
         "  <project-dir>/app.art           a native ART app instead (see\n"
         "        art/) - one file, compiled ahead of time, never combined\n"
         "        with src/**/*.cpp or goapp/ in the same project.\n"
         "  <project-dir>/app.js            optional embedded script -\n"
         "        orthogonal to the native-language choice above, works\n"
         "        with any of them.\n\n"
         "  --build-dir     Where to configure/build (default: ./build).\n"
         "  -o, --output    Copy the built binary here.\n"
         "  --run           Run the binary after a successful build.\n";
}

// Single-quotes a path for /bin/sh - safe against spaces and the ';'
// ARTISAN_APP_CPP_SOURCES packs multiple paths with. Not bulletproof
// against a literal "'" in a path, which is vanishingly rare for a build
// tool's own inputs and not worth more complexity to handle perfectly.
std::string ShellQuote(const std::string &text) { return "'" + text + "'"; }

// Joins paths with ';', the separator CMake list-valued cache variables
// (ARTISAN_UI_SOURCES, ARTISAN_APP_CPP_SOURCES) expect.
std::string JoinPaths(const std::vector<fs::path> &paths) {
  std::ostringstream joined;
  for (size_t i = 0; i < paths.size(); ++i) {
    if (i > 0) {
      joined << ";";
    }
    joined << paths[i].string();
  }
  return joined.str();
}

// Same as JoinPaths, for entries that are already formatted strings
// (bare paths or "name=path" - see DiscoverPages) rather than fs::paths.
std::string JoinStrings(const std::vector<std::string> &items) {
  std::ostringstream joined;
  for (size_t i = 0; i < items.size(); ++i) {
    if (i > 0) {
      joined << ";";
    }
    joined << items[i];
  }
  return joined.str();
}

int RunCommand(const std::string &command) {
  std::cout << "$ " << command << "\n";
  return std::system(command.c_str());
}

// Every absolute file with `extension`, anywhere under `dir` (however
// deeply nested), sorted for a deterministic build across runs/
// filesystems - directory_iterator order isn't guaranteed. Used for
// src/**/*.cpp, where nesting is just organization - unlike pages/ below,
// a .cpp's location doesn't name anything, so no route logic is needed.
std::vector<fs::path> SortedFilesWithExtensionRecursive(const fs::path &dir,
                                                         const std::string &extension) {
  std::vector<fs::path> paths;
  if (fs::is_directory(dir)) {
    for (const auto &entry : fs::recursive_directory_iterator(dir)) {
      if (entry.is_regular_file() && entry.path().extension() == extension) {
        paths.push_back(fs::absolute(entry.path()));
      }
    }
  }
  std::sort(paths.begin(), paths.end());
  return paths;
}

struct PageEntry {
  std::string name;
  fs::path path;
};

// Every .html file under `pagesDir`, however deeply nested, named after
// its path relative to pagesDir with the extension stripped and '/' as
// the separator - a Next.js-style folder-per-route convention:
// pages/settings/profile.html becomes the page "settings/profile", and
// an <a href="settings/profile"> anywhere in the bundle can navigate to
// it. A file named index.html stands for its own directory's own route,
// same as the project root's pages/index.html becoming "index" (not
// "index/index") - so pages/settings/index.html becomes "settings".
// Sorted by name for a deterministic build, with "index" (the page the
// app opens on - see main.cpp's Navigate()) moved to the front.
std::vector<PageEntry> DiscoverPages(const fs::path &pagesDir) {
  std::vector<PageEntry> pages;

  if (fs::is_directory(pagesDir)) {
    for (const auto &entry : fs::recursive_directory_iterator(pagesDir)) {
      if (!entry.is_regular_file() || entry.path().extension() != ".html") {
        continue;
      }

      fs::path relative = fs::relative(entry.path(), pagesDir);
      relative.replace_extension();
      std::string name = relative.generic_string();

      const std::string kIndexSuffix = "/index";
      if (name.size() > kIndexSuffix.size() &&
          name.compare(name.size() - kIndexSuffix.size(), kIndexSuffix.size(),
                        kIndexSuffix) == 0) {
        name.erase(name.size() - kIndexSuffix.size());
      }

      pages.push_back({name, fs::absolute(entry.path())});
    }
  }

  std::sort(pages.begin(), pages.end(), [](const PageEntry &a, const PageEntry &b) {
    return a.name < b.name;
  });

  auto indexIt = std::find_if(pages.begin(), pages.end(), [](const PageEntry &page) {
    return page.name == "index";
  });
  if (indexIt != pages.end()) {
    std::iter_swap(pages.begin(), indexIt);
  }

  return pages;
}

struct DiscoveredProject {
  std::vector<std::string> htmlEntries; // "name=absolute/path.html", one per page.
  std::vector<fs::path> cppPaths;
  fs::path jsPath;  // Empty if app.js doesn't exist.
  fs::path goPath;  // Empty if goapp/go.mod doesn't exist.
  fs::path artPath; // Empty if app.art doesn't exist.
};

// The layout `artisan-cli new` scaffolds (see RunNew below): every page's
// markup under pages/ (nested folders form nested routes - see
// DiscoverPages), every native C++ source under src/, one optional
// shared app.js at the project root, and one optional shared Go app
// under goapp/. Exits the process with an error if pages/ is missing or
// empty - a bundle needs at least one page.
DiscoveredProject DiscoverProject(const fs::path &projectDir) {
  DiscoveredProject discovered;

  fs::path pagesDir = projectDir / "pages";
  std::vector<PageEntry> pages = DiscoverPages(pagesDir);
  if (pages.empty()) {
    std::cerr << "artisan-cli: " << pagesDir
               << " doesn't exist or has no .html files - a project needs "
                  "at least one page\n";
    std::exit(1);
  }
  for (const PageEntry &page : pages) {
    discovered.htmlEntries.push_back(page.name + "=" + page.path.string());
  }

  discovered.cppPaths =
      SortedFilesWithExtensionRecursive(projectDir / "src", ".cpp");

  fs::path jsPath = projectDir / "app.js";
  if (fs::exists(jsPath)) {
    discovered.jsPath = fs::absolute(jsPath);
  }

  // A Go app is its own module (go.mod), not a bag of loose .cpp-style
  // files - unlike src/**/*.cpp, "every .go file under goapp/" isn't a
  // meaningful unit to compile on its own, so this only recognizes the
  // whole directory, gated on go.mod actually being there.
  fs::path goPath = projectDir / "goapp";
  if (fs::exists(goPath / "go.mod")) {
    discovered.goPath = fs::absolute(goPath);
  }

  // An ART app is a single file, like app.js - not a directory/module the
  // way a Go app is, since ART has no multi-file/import system yet.
  fs::path artPath = projectDir / "app.art";
  if (fs::exists(artPath)) {
    discovered.artPath = fs::absolute(artPath);
  }

  // A project drives its DOM natively from exactly one language, never
  // more than one at once - each of C++/Go/ART runs its own SetupApp-
  // equivalent against the same page on load, so any two at once would
  // be independent, unordered sources of truth for the same startup
  // behavior. Script (app.js) doesn't compete here since it's already
  // designed to layer on top of whichever native language runs.
  int nativeLangCount = (!discovered.cppPaths.empty() ? 1 : 0) + (!discovered.goPath.empty() ? 1 : 0) +
                        (!discovered.artPath.empty() ? 1 : 0);
  if (nativeLangCount > 1) {
    std::cerr << "artisan-cli: " << projectDir
               << " has more than one of src/**/*.cpp, goapp/, and app.art "
                  "- a project can only use one native language at a "
                  "time. Remove the ones you don't want.\n";
    std::exit(1);
  }

  return discovered;
}

// The tail RunBuild shares across every project build: configure, build,
// optionally copy the binary out and/or run it.
// `htmlEntries` items are each either a bare absolute path (artisanc
// derives the page name from its stem) or "name=path" (an explicit name,
// for a nested route - see DiscoverPages).
int ConfigureAndBuild(const std::vector<std::string> &htmlEntries,
                       const std::vector<fs::path> &cppAbs,
                       const fs::path &jsAbs, const fs::path &goAbs,
                       const fs::path &artAbs,
                       const std::string &buildDirStr,
                       const std::string &outputPathStr, bool run) {
  fs::path projectSourceDir = ARTISAN_PROJECT_SOURCE_DIR;
  fs::path buildDir = fs::absolute(buildDirStr);

  std::ostringstream configureCmd;
  configureCmd << "cmake -S " << ShellQuote(projectSourceDir.string())
               << " -B " << ShellQuote(buildDir.string())
               << " -DARTISAN_UI_SOURCES="
               << ShellQuote(JoinStrings(htmlEntries));

  // Always set, even when empty: ARTISAN_APP_CPP_SOURCES is a CACHE
  // STRING with its own default (this repo's own demo src/app.cpp, for
  // building it directly with plain cmake) that would otherwise stick
  // around from a previous configure of this same build dir - a Go-only
  // project (empty cppAbs) needs that cleared, not silently left as
  // whatever it was, to actually be Go-only.
  configureCmd << " -DARTISAN_APP_CPP_SOURCES="
               << ShellQuote(JoinPaths(cppAbs));

  configureCmd << " -DARTISAN_APP_JS_SOURCE="
               << ShellQuote(jsAbs.empty() ? "" : jsAbs.string());

  configureCmd << " -DARTISAN_APP_GO_SOURCE="
               << ShellQuote(goAbs.empty() ? "" : goAbs.string());

  configureCmd << " -DARTISAN_APP_ART_SOURCE="
               << ShellQuote(artAbs.empty() ? "" : artAbs.string());

  if (RunCommand(configureCmd.str()) != 0) {
    std::cerr << "artisan-cli: cmake configure failed\n";
    return 1;
  }

  std::ostringstream buildCmd;
  buildCmd << "cmake --build " << ShellQuote(buildDir.string())
           << " --target artisan";
  if (RunCommand(buildCmd.str()) != 0) {
    std::cerr << "artisan-cli: build failed\n";
    return 1;
  }

  fs::path builtBinary = buildDir / "artisan";

  if (!outputPathStr.empty()) {
    fs::path outAbs = fs::absolute(outputPathStr);
    std::error_code ec;
    fs::copy_file(builtBinary, outAbs, fs::copy_options::overwrite_existing,
                   ec);
    if (ec) {
      std::cerr << "artisan-cli: failed to copy output to " << outAbs << ": "
                << ec.message() << "\n";
      return 1;
    }
    fs::permissions(outAbs,
                     fs::perms::owner_all | fs::perms::group_read |
                         fs::perms::group_exec | fs::perms::others_read |
                         fs::perms::others_exec,
                     fs::perm_options::add, ec);
    builtBinary = outAbs;
  }

  std::cout << "artisan-cli: built " << builtBinary.string() << "\n";

  if (run) {
    return RunCommand(ShellQuote(builtBinary.string()));
  }

  return 0;
}

void PrintNewUsage() {
  std::cerr << "usage: artisan-cli new <project-dir> [--lang cpp|go|art]\n\n"
               "Scaffolds a new artisan project at <project-dir>:\n"
               "  pages/index.html   starter markup - the page the app\n"
               "                     opens on; add more pages/*.html (or\n"
               "                     pages/some-folder/*.html for a nested\n"
               "                     route, Next.js-style) and link between\n"
               "                     them with <a href=\"...\">\n\n"
               "  --lang cpp (default)\n"
               "    src/main.cpp     native SetupApp(Node&) - see\n"
               "                     include/app.h. Add more src/*.cpp as\n"
               "                     the project grows - every one gets\n"
               "                     compiled in, no need to list them.\n\n"
               "  --lang go\n"
               "    goapp/           a Go app (go.mod/main.go) exporting\n"
               "                     ArtisanSetupApp - see go/artisango and\n"
               "                     README.md's \"Using Go\" section.\n\n"
               "  --lang art\n"
               "    app.art          an ART app (see art/) - a statically\n"
               "                     typed, TypeScript-like language\n"
               "                     compiled ahead of time with the ART\n"
               "                     compiler. Defines `function setupApp\n"
               "                     (document: Node): void` and calls\n"
               "                     into the DOM through `declare\n"
               "                     function`s - see include/art_bridge.h\n"
               "                     and README.md's \"Using ART\" section.\n\n"
               "A project uses one native language, never more than one.\n"
               "Build it afterward with:\n"
               "  artisan-cli build <project-dir>\n";
}

void WriteFile(const fs::path &path, const std::string &contents) {
  std::ofstream out(path);
  if (!out) {
    std::cerr << "artisan-cli: failed to write " << path << "\n";
    std::exit(1);
  }
  out << contents;
}

constexpr const char *kIndexHtmlTemplate = R"html(<!doctype html>
<html>
  <head>
    <title>My artisan app</title>
  </head>
  <body>
    <h1>Hello, artisan</h1>
    <p>Edit index.html and main.cpp to get started.</p>
  </body>
</html>
)html";

// SetupApp(Node&) is the native counterpart to a script's top-level code
// (see include/js_engine.h) - plain compiled C++ against the same Node
// API, run once when the app starts. include/app.h has the full picture;
// dom_node.h documents Node itself (FindById, SetAttribute,
// SetTextContent, SetOnClick, AppendChild, ...).
constexpr const char *kMainCppTemplate = R"cpp(#include "app.h"

namespace artisan {

void SetupApp(Node &document) {
  // Your native startup code goes here, e.g.:
  //
  //   Node *button = document.FindById("my-button");
  //   if (button != nullptr) {
  //     button->SetOnClick([]() {
  //       // ...
  //     });
  //   }
}

} // namespace artisan
)cpp";

// setupApp is the ART counterpart to SetupApp/ArtisanSetupApp above - see
// art_bridge.h for the full `declare`-able DOM API and README.md's "Using
// ART" section. Unlike the Go/C++ templates, there's no boilerplate
// trampoline to write by hand - the artisan build itself adapts a plain
// `function setupApp(document: Node): void` into ArtisanSetupApp.
constexpr const char *kAppArtTemplate = R"art(declare type Node;
declare type Event;

declare function ArtDocument(): Node;
declare function ArtFindById(root: Node, id: string): Node;
declare function ArtIsNull(node: Node): boolean;
declare function ArtGetTextContent(node: Node): string;
declare function ArtSetTextContent(node: Node, text: string): void;
declare function ArtGetAttribute(node: Node, name: string): string;
declare function ArtHasAttribute(node: Node, name: string): boolean;
declare function ArtSetAttribute(node: Node, name: string, value: string): void;
declare function ArtQuerySelector(root: Node, selector: string): Node;
declare function ArtChildCount(node: Node): number;
declare function ArtChildAt(node: Node, index: number): Node;
declare function ArtSetOnClick(node: Node, handler: () => void): void;

// General event listening - "keydown", "change", "input", or any other
// type Node::DispatchEvent fires. Unlike ArtSetOnClick above, this stacks
// (multiple listeners on the same node/type all run) rather than
// replacing, and the handler receives the Event itself - see the
// ArtEvent* functions below for what you can read/call on it.
declare function ArtAddEventListener(node: Node, eventType: string, handler: (event: Event) => void, capture: boolean): void;
// Removes a listener previously added with the exact same node/eventType/
// handler/capture - a mismatched call (wrong handler, or one never added)
// is a safe no-op.
declare function ArtRemoveEventListener(node: Node, eventType: string, handler: (event: Event) => void, capture: boolean): void;
// Fires your own event (any type, not just built-in ones) at `node`,
// carrying `detail` (pass "" for none) - readable back with
// ArtEventDetail below, but only in a listener on an event ART itself
// dispatched (see its own doc comment for why). Returns false if the
// event was cancelable and some listener called ArtEventPreventDefault.
declare function ArtDispatchEvent(node: Node, eventType: string, bubbles: boolean, cancelable: boolean, detail: string): boolean;
declare function ArtEventType(event: Event): string;
declare function ArtEventTarget(event: Event): Node;
declare function ArtEventDetail(event: Event): string;
declare function ArtEventBubbles(event: Event): boolean;
declare function ArtEventCancelable(event: Event): boolean;
declare function ArtEventPreventDefault(event: Event): void;
declare function ArtEventDefaultPrevented(event: Event): boolean;
declare function ArtEventStopPropagation(event: Event): void;
declare function ArtEventStopImmediatePropagation(event: Event): void;
declare function ArtEventClientX(event: Event): number;
declare function ArtEventClientY(event: Event): number;
declare function ArtEventCtrlKey(event: Event): boolean;
declare function ArtEventShiftKey(event: Event): boolean;
declare function ArtEventAltKey(event: Event): boolean;
declare function ArtEventMetaKey(event: Event): boolean;
declare function ArtEventKey(event: Event): string;
declare function ArtEventCode(event: Event): string;

// A top-level `let`/`const` is state that survives across calls - e.g. a
// click handler's own counter. Its initializer must be a literal number/
// boolean/string (no calls, arithmetic, or other globals - see
// README.md's "Using ART" section for why).
let clickCount: number = 0;

// A click handler is just a plain zero-argument, void-returning function -
// pass its bare name (not a call) to ArtSetOnClick below. It gets no Node
// of its own (unlike setupApp), so it reaches the DOM through
// ArtDocument() instead, e.g.:
//
//   let root: Node = ArtDocument();
//   if (!ArtIsNull(root)) {
//     let label: Node = ArtFindById(root, "my-label");
//     ...
//   }
function onButtonClick(): void {
  clickCount = clickCount + 1;
  // Your click-time code goes here, e.g. showing the new count:
  //
  //   let root: Node = ArtDocument();
  //   if (!ArtIsNull(root)) {
  //     let label: Node = ArtFindById(root, "my-label");
  //     if (!ArtIsNull(label)) {
  //       ArtSetTextContent(label, numberToString(clickCount) + " clicks");
  //     }
  //   }
}

// An ArtAddEventListener handler takes the Event itself as its one
// parameter - register it the same way, by bare name, e.g.
// `ArtAddEventListener(input, "keydown", onKeyDown, false)`.
function onKeyDown(event: Event): void {
  // Your key-press code goes here, e.g.:
  //
  //   if (ArtEventKey(event) == "Enter") {
  //     ArtEventPreventDefault(event);
  //     ArtDispatchEvent(ArtDocument(), "form-submitted", true, true, "ok");
  //   }
}

// Your own event's detail only round-trips through ArtEventDetail
// correctly in a listener on an event ART itself dispatched (see
// ArtDispatchEvent's doc comment above).
function onFormSubmitted(event: Event): void {
  let payload: string = ArtEventDetail(event); // "ok", from the dispatch above
}

function setupApp(document: Node): void {
  // Your native startup code goes here, e.g.:
  //
  //   let button: Node = ArtFindById(document, "my-button");
  //   if (!ArtIsNull(button)) {
  //     ArtSetOnClick(button, onButtonClick);
  //   }
}
)art";

// go.mod's replace directive needs an absolute path to go/artisango - the
// same checkout artisan-cli itself was built from (ARTISAN_PROJECT_SOURCE_DIR,
// already how ConfigureAndBuild finds this repo's own CMakeLists.txt), since
// artisango isn't a published module a project could otherwise `go get`.
std::string GoModTemplate() {
  fs::path artisangoPath =
      fs::path(ARTISAN_PROJECT_SOURCE_DIR) / "go" / "artisango";
  std::ostringstream out;
  out << "module goapp\n\n"
      << "go 1.21\n\n"
      << "require artisango v0.0.0\n\n"
      << "replace artisango => " << artisangoPath.string() << "\n";
  return out.str();
}

// Lets a C++ project build directly with plain cmake/ninja - no
// artisan-cli involved - by discovering the same pages/**/*.html,
// src/**/*.cpp, and app.js this project's own artisan-cli build already
// would, in CMake itself, then add_subdirectory()-ing the artisan
// checkout with those computed as CACHE variables (the exact mechanism
// ConfigureAndBuild already drives via -D flags - a CACHE variable set
// here before add_subdirectory() seeds the subdirectory's own `set(...
// CACHE ...)` calls the same way a command-line -D would).
//
// This is a second implementation of DiscoverProject/DiscoverPages
// (src/cli/main.cpp) in CMake's own glob/string/regex vocabulary, not a
// call back into artisan-cli - keep the two in sync if that discovery
// logic ever changes. One simplification versus DiscoverPages: only the
// index page's position is functionally significant (main.cpp opens on
// pages.front()), so this doesn't bother fully replicating its
// alphabetical re-sort for the rest - just a plain list(SORT) for
// reasonable determinism, with the index entry (if any) forced to the
// front afterward.
std::string CMakeListsTemplate() {
  fs::path artisanCheckout = fs::path(ARTISAN_PROJECT_SOURCE_DIR);
  std::ostringstream out;
  out << R"cmake(cmake_minimum_required(VERSION 3.20)

get_filename_component(ARTISAN_PROJECT_NAME "${CMAKE_CURRENT_SOURCE_DIR}" NAME)
project(${ARTISAN_PROJECT_NAME} LANGUAGES CXX)

# Path to the artisan framework checkout this project was scaffolded
# from - update this if that checkout moves (same constraint
# goapp/go.mod's `replace` line has for a Go project - see README).
# Needs Skia already built there (./build_skia.sh) - this file only
# builds this project, it doesn't build Skia itself.
set(ARTISAN_CHECKOUT_DIR ")cmake"
      << artisanCheckout.string() << R"cmake(" CACHE PATH
    "Path to the artisan framework checkout")

# --- Page discovery: pages/**/*.html -> route=path, Next.js-style
# nested routes (pages/settings/profile.html -> "settings/profile"; a
# folder's own index.html stands for that folder's route, e.g.
# pages/settings/index.html -> "settings"). The top-level pages/index.html
# stays "index" and is forced to the front below, since it's the page
# the app opens on.
file(GLOB_RECURSE ARTISAN_PAGE_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/pages/*.html")

set(ARTISAN_UI_SOURCES_LIST "")
set(ARTISAN_INDEX_ENTRY "")
foreach(page_file ${ARTISAN_PAGE_FILES})
    file(RELATIVE_PATH route_name "${CMAKE_CURRENT_SOURCE_DIR}/pages" "${page_file}")
    string(REGEX REPLACE "\\.html$" "" route_name "${route_name}")
    string(REGEX REPLACE "^(.+)/index$" "\\1" route_name "${route_name}")
    set(entry "${route_name}=${page_file}")
    if(route_name STREQUAL "index")
        set(ARTISAN_INDEX_ENTRY "${entry}")
    else()
        list(APPEND ARTISAN_UI_SOURCES_LIST "${entry}")
    endif()
endforeach()
list(SORT ARTISAN_UI_SOURCES_LIST)
if(NOT ARTISAN_INDEX_ENTRY STREQUAL "")
    list(PREPEND ARTISAN_UI_SOURCES_LIST "${ARTISAN_INDEX_ENTRY}")
endif()

if(ARTISAN_UI_SOURCES_LIST STREQUAL "")
    message(FATAL_ERROR "No pages/**/*.html found - a project needs at least one page")
endif()

# --- Native C++ source discovery: every src/**/*.cpp - a .cpp's
# location is just organization, no route logic needed.
file(GLOB_RECURSE ARTISAN_APP_CPP_SOURCES_LIST CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
list(SORT ARTISAN_APP_CPP_SOURCES_LIST)

set(ARTISAN_APP_JS_SOURCE_VALUE "")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/app.js")
    set(ARTISAN_APP_JS_SOURCE_VALUE "${CMAKE_CURRENT_SOURCE_DIR}/app.js")
endif()

set(ARTISAN_UI_SOURCES "${ARTISAN_UI_SOURCES_LIST}" CACHE STRING "" FORCE)
set(ARTISAN_APP_CPP_SOURCES "${ARTISAN_APP_CPP_SOURCES_LIST}" CACHE STRING "" FORCE)
set(ARTISAN_APP_JS_SOURCE "${ARTISAN_APP_JS_SOURCE_VALUE}" CACHE STRING "" FORCE)
set(ARTISAN_APP_GO_SOURCE "" CACHE STRING "" FORCE)

# Pulls in the real `artisan` executable target (and everything else the
# checkout's own CMakeLists.txt defines - artisanc, artisan_cli, ...) -
# `cmake --build build --target artisan` builds just this project's
# binary; a plain `cmake --build build` (no --target) builds those too.
# Via the ARTISAN_CHECKOUT_DIR cache variable (not the literal path
# above again) so overriding it - e.g. `cmake -B build
# -DARTISAN_CHECKOUT_DIR=...` after moving the checkout - actually takes
# effect, rather than only updating a comment nobody re-reads.
add_subdirectory("${ARTISAN_CHECKOUT_DIR}" artisan_build)
)cmake";
  return out.str();
}

// ArtisanSetupApp is the Go counterpart to SetupApp above - see
// go/artisango's own doc comment for the full picture. The trampoline
// (ArtisanSetupApp/unsafe.Pointer) is boilerplate every Go app needs
// verbatim; SetupApp itself is where a project's own code goes.
constexpr const char *kMainGoTemplate = R"go(package main

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
)go";

// Creates `dir` (including any missing parents) or exits the process with
// an error - used for both the project root and its pages/src
// subdirectories below.
void CreateDirectory(const fs::path &dir) {
  std::error_code ec;
  fs::create_directories(dir, ec);
  if (ec) {
    std::cerr << "artisan-cli: failed to create " << dir << ": "
               << ec.message() << "\n";
    std::exit(1);
  }
}

int RunNew(int argc, char *argv[]) {
  if (argc < 3) {
    PrintNewUsage();
    return 1;
  }

  fs::path projectDir = argv[2];
  std::string lang = "cpp";

  for (int i = 3; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--lang") {
      if (i + 1 >= argc) {
        std::cerr << "artisan-cli: --lang needs a value\n";
        return 1;
      }
      lang = argv[++i];
    } else {
      std::cerr << "artisan-cli: unknown argument " << arg << "\n";
      PrintNewUsage();
      return 1;
    }
  }

  if (lang != "cpp" && lang != "go" && lang != "art") {
    std::cerr << "artisan-cli: --lang must be \"cpp\", \"go\", or \"art\", got \""
               << lang << "\"\n";
    return 1;
  }

  if (fs::exists(projectDir)) {
    if (!fs::is_directory(projectDir)) {
      std::cerr << "artisan-cli: " << projectDir << " exists and isn't a directory\n";
      return 1;
    }
    if (!fs::is_empty(projectDir)) {
      std::cerr << "artisan-cli: " << projectDir
                 << " already exists and isn't empty - refusing to overwrite\n";
      return 1;
    }
  } else {
    CreateDirectory(projectDir);
  }

  CreateDirectory(projectDir / "pages");
  WriteFile(projectDir / "pages" / "index.html", kIndexHtmlTemplate);

  if (lang == "go") {
    CreateDirectory(projectDir / "goapp");
    WriteFile(projectDir / "goapp" / "go.mod", GoModTemplate());
    WriteFile(projectDir / "goapp" / "main.go", kMainGoTemplate);
  } else if (lang == "art") {
    WriteFile(projectDir / "app.art", kAppArtTemplate);
  } else {
    CreateDirectory(projectDir / "src");
    WriteFile(projectDir / "src" / "main.cpp", kMainCppTemplate);
    WriteFile(projectDir / "CMakeLists.txt", CMakeListsTemplate());
  }

  std::string langLabel = lang == "go" ? "Go" : lang == "art" ? "ART" : "C++";
  std::cout << "artisan-cli: created a new " << langLabel
            << " project at " << fs::absolute(projectDir).string() << "\n\n"
            << "Next steps:\n"
            << "  artisan-cli build " << fs::absolute(projectDir).string()
            << " --run\n";
  return 0;
}

void PrintComponentUsage() {
  std::cerr
      << "usage: artisan-cli component <project-dir> <name>\n\n"
         "Scaffolds a reusable component in an existing project:\n"
         "  components/<name>.html     markup fragment (with a starter\n"
         "                             <style> block scoped to its own\n"
         "                             id) - paste this into any page\n"
         "                             under pages/ wherever you want it\n"
         "                             to appear. Not itself a routable\n"
         "                             page.\n"
         "  src/components/<name>.cpp  Setup_<name>(Node&) - call it from\n"
         "                             that page's SetupApp (or another\n"
         "                             component's) with the Node the\n"
         "                             fragment landed in. Auto-discovered\n"
         "                             and compiled in like any other\n"
         "                             src/**/*.cpp - no extra wiring.\n";
}

// Mirrors artisanc's own SanitizeIdentifier (src/compiler/main.cpp): a
// component name is arbitrary user-facing text (may have hyphens, like an
// id - "my-button"), but the C++ function built from it needs to be a
// valid identifier - "Setup_my_button".
std::string SanitizeIdentifier(const std::string &name) {
  std::string out = name;
  for (char &c : out) {
    if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') {
      c = '_';
    }
  }
  if (!out.empty() && std::isdigit(static_cast<unsigned char>(out.front()))) {
    out = "_" + out;
  }
  return out.empty() ? "_" : out;
}

std::string ComponentHtmlTemplate(const std::string &name) {
  std::string identifier = SanitizeIdentifier(name);
  std::ostringstream out;
  out << "<!--\n"
      << "  \"" << name << "\" component markup - paste this into any page\n"
      << "  under pages/ wherever you want it to appear, then wire it up\n"
      << "  by calling Setup_" << identifier << "(...) from that page's\n"
      << "  SetupApp with the Node it landed in (see\n"
      << "  src/components/" << name << ".cpp).\n"
      << "-->\n"
      << "<style>\n"
      << "  /* Selectors here apply to the whole page once pasted in, not\n"
      << "     just this fragment (see README.md's \"Using CSS\") - no\n"
      << "     descendant selectors (e.g. \"#" << name << " p\"), only\n"
      << "     tag/.class/#id and compounds of these, so keep them scoped\n"
      << "     to this component's own id/classes to avoid styling the\n"
      << "     rest of the page by accident.\n"
      << "  */\n"
      << "  #" << name << " {\n"
      << "    /* Component styles go here, e.g.: */\n"
      << "    /* background-color: #f5f5f5; */\n"
      << "  }\n"
      << "</style>\n"
      << "<div id=\"" << name << "\">\n"
      << "  <!-- Component markup goes here. -->\n"
      << "</div>\n";
  return out.str();
}

std::string ComponentCppTemplate(const std::string &name) {
  std::string identifier = SanitizeIdentifier(name);
  std::ostringstream out;
  out << "#include \"dom_node.h\"\n"
      << "#include \"hooks.h\"\n\n"
      << "namespace artisan {\n\n"
      << "// Wires up the \"" << name << "\" component - call this from\n"
      << "// SetupApp (see app.h) or another component's setup function,\n"
      << "// passing the Node this component's markup\n"
      << "// (components/" << name << ".html) landed in, e.g.:\n"
      << "//\n"
      << "//   Node *" << identifier << " = document.FindById(\"" << name
      << "\");\n"
      << "//   if (" << identifier << " != nullptr) {\n"
      << "//     Setup_" << identifier << "(*" << identifier << ");\n"
      << "//   }\n"
      << "void Setup_" << identifier << "(Node &root) {\n"
      << "  // Your component's native startup code goes here, e.g.:\n"
      << "  //\n"
      << "  //   Node *button = root.FindById(\"" << name << "-button\");\n"
      << "  //   if (button != nullptr) {\n"
      << "  //     button->SetOnClick([]() {\n"
      << "  //       // ...\n"
      << "  //     });\n"
      << "  //   }\n"
      << "  //\n"
      << "  // Need state a handler can read/update across calls? Use\n"
      << "  // UseState (hooks.h) instead of a class or a hand-rolled\n"
      << "  // shared_ptr - Setup_" << identifier << " still only runs once,\n"
      << "  // so it's the handlers that close over the state, e.g.:\n"
      << "  //\n"
      << "  //   auto [count, setCount] = UseState(0);\n"
      << "  //   Node *label = root.FindById(\"" << name << "-count\");\n"
      << "  //   if (button != nullptr && label != nullptr) {\n"
      << "  //     button->SetOnClick([=]() mutable {\n"
      << "  //       setCount(count() + 1);\n"
      << "  //       label->SetTextContent(std::to_string(count()));\n"
      << "  //     });\n"
      << "  //   }\n"
      << "}\n\n"
      << "} // namespace artisan\n";
  return out.str();
}

bool IsValidComponentName(const std::string &name) {
  return !name.empty() && name != "." && name != ".." &&
         name.find('/') == std::string::npos;
}

int RunComponent(int argc, char *argv[]) {
  if (argc != 4) {
    PrintComponentUsage();
    return 1;
  }

  fs::path projectDir = argv[2];
  std::string name = argv[3];

  if (!fs::is_directory(projectDir)) {
    std::cerr << "artisan-cli: " << projectDir << " is not a directory\n";
    return 1;
  }

  if (!IsValidComponentName(name)) {
    std::cerr << "artisan-cli: \"" << name
               << "\" isn't a valid component name - no '/', and not empty, "
                  "\".\", or \"..\"\n";
    return 1;
  }

  fs::path htmlPath = projectDir / "components" / (name + ".html");
  fs::path cppPath = projectDir / "src" / "components" / (name + ".cpp");

  if (fs::exists(htmlPath) || fs::exists(cppPath)) {
    std::cerr << "artisan-cli: " << (fs::exists(htmlPath) ? htmlPath : cppPath)
               << " already exists - refusing to overwrite\n";
    return 1;
  }

  CreateDirectory(htmlPath.parent_path());
  CreateDirectory(cppPath.parent_path());

  WriteFile(htmlPath, ComponentHtmlTemplate(name));
  WriteFile(cppPath, ComponentCppTemplate(name));

  std::cout << "artisan-cli: created component \"" << name << "\" at\n"
            << "  " << fs::absolute(htmlPath).string() << "\n"
            << "  " << fs::absolute(cppPath).string() << "\n\n"
            << "Next steps:\n"
            << "  1. Paste the markup from " << htmlPath.filename().string()
            << " into a page under pages/.\n"
            << "  2. In that page's SetupApp, find it by id and call\n"
            << "     Setup_" << SanitizeIdentifier(name) << "(...).\n";
  return 0;
}

} // namespace

// `artisan-cli build <project-dir> [--build-dir <dir>] [-o <output>]
// [--run]` - always a project directory; its pages/src or goapp/app.js
// get discovered instead of named one by one (see DiscoverProject).
int RunBuild(int argc, char *argv[]) {
  if (argc < 3) {
    PrintBuildUsage();
    return 1;
  }

  fs::path projectDir = fs::absolute(argv[2]);
  if (!fs::is_directory(projectDir)) {
    std::cerr << "artisan-cli: " << projectDir << " is not a directory\n";
    return 1;
  }

  DiscoveredProject discovered = DiscoverProject(projectDir);

  std::string buildDirStr = "build";
  std::string outputPathStr;
  bool run = false;

  for (int i = 3; i < argc; ++i) {
    std::string arg = argv[i];
    auto next = [&]() -> std::string {
      if (i + 1 >= argc) {
        std::cerr << "artisan-cli: " << arg << " needs a value\n";
        std::exit(1);
      }
      return argv[++i];
    };

    if (arg == "--build-dir") {
      buildDirStr = next();
    } else if (arg == "-o" || arg == "--output") {
      outputPathStr = next();
    } else if (arg == "--run") {
      run = true;
    } else {
      std::cerr << "artisan-cli: unknown argument " << arg << "\n";
      PrintBuildUsage();
      return 1;
    }
  }

  return ConfigureAndBuild(discovered.htmlEntries, discovered.cppPaths,
                            discovered.jsPath, discovered.goPath,
                            discovered.artPath, buildDirStr,
                            outputPathStr, run);
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    PrintTopUsage();
    return 1;
  }

  std::string command = argv[1];
  if (command == "new") {
    return RunNew(argc, argv);
  }
  if (command == "component") {
    return RunComponent(argc, argv);
  }
  if (command == "build") {
    return RunBuild(argc, argv);
  }

  std::cerr << "artisan-cli: unknown command \"" << command << "\"\n";
  PrintTopUsage();
  return 1;
}
