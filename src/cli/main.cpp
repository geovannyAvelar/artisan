// artisan-cli - the single entry point that ties artisan's whole
// workflow together: scaffolding a new project (`new`), and compiling
// markup (artisanc), a compiled ART app, and an embedded JS/JSX script
// (embed_text) into one native binary (`build`).
//
// `build` does not reimplement compilation or linking itself - Skia,
// lexbor, and QuickJS all need to be found and wired together correctly,
// and the project's own CMakeLists.txt already knows how to do that. What
// it actually does is discover a project directory's files (see
// DiscoverProject) and turn them into the right CMake configure + build
// invocation, substituted in via cache variables (ARTISAN_UI_SOURCES,
// ARTISAN_APP_ART_SOURCE, ARTISAN_APP_JS_SOURCE, ARTISAN_APP_JSX_SOURCE,
// ARTISAN_JS_PRELUDE_SOURCE) that CMakeLists.txt exposes for exactly this
// purpose. There's no mode for naming individual files by hand - always a
// project directory, so there's exactly one way a project is laid out.
//
// `new` writes out a starting point using ART (app.tsx, compiled ahead
// of time via LLVM - see art/), artisan's primary app language. A
// project can also add its own app.js/app.jsx (interpreted at runtime
// via QuickJS - see DiscoverProject) alongside it, for JS code that
// can't be ported to ART or a real UI library (e.g. React) - see
// README.md's "Using JavaScript" section - but that's opt-in, not part
// of the default scaffold.

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
               "  new <project-dir>   Scaffold a new project.\n"
               "  build               Compile a project into a binary.\n\n"
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
         "  <project-dir>/app.tsx           an ART app (see art/) - one\n"
         "        file (app.ts instead, if you don't need JSX in the\n"
         "        entry point itself), compiled ahead of time. Optional -\n"
         "        a project can be JS-only.\n"
         "  <project-dir>/app.js            optional embedded script (or\n"
         "        app.jsx, transformed from real JSX at build time - see\n"
         "        tools/jsx_transform - mutually exclusive with app.js) -\n"
         "        independent of app.tsx above, works with or without it.\n"
         "  <project-dir>/js-prelude.js     optional second embedded\n"
         "        script, run before app.js/app.jsx - e.g. a vendored UI\n"
         "        library (see third_party/react/ for a ready-made\n"
         "        example, and README.md's \"Using JavaScript\" section).\n\n"
         "  --build-dir     Where to configure/build (default: ./build).\n"
         "  -o, --output    Copy the built binary here.\n"
         "  --run           Run the binary after a successful build.\n";
}

// Single-quotes a path for /bin/sh - safe against spaces and the ';'
// ARTISAN_UI_SOURCES packs multiple paths with. Not bulletproof against
// a literal "'" in a path, which is vanishingly rare for a build tool's
// own inputs and not worth more complexity to handle perfectly.
std::string ShellQuote(const std::string &text) { return "'" + text + "'"; }

// Joins strings with ';', the separator CMake list-valued cache
// variables (ARTISAN_UI_SOURCES) expect - entries that are already
// formatted strings (bare paths or "name=path" - see DiscoverPages)
// rather than fs::paths.
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
  fs::path jsPath;             // Empty if app.js doesn't exist.
  fs::path jsxPath;            // Empty if app.jsx doesn't exist.
  fs::path jsPreludePath;      // Empty if js-prelude.js doesn't exist.
  fs::path artPath;            // Empty if neither app.ts nor app.tsx exists.
};

// The layout `artisan-cli new` scaffolds (see RunNew below): every page's
// markup under pages/ (nested folders form nested routes - see
// DiscoverPages), an optional compiled ART entry point, and an optional
// embedded JS/JSX script. Exits the process with an error if pages/ is
// missing or empty - a bundle needs at least one page.
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

  // app.js and app.jsx are mutually exclusive, same reasoning as
  // app.ts/app.tsx below - app.jsx needs the JSX transform (see
  // tools/jsx_transform), app.js doesn't.
  fs::path jsPath = projectDir / "app.js";
  fs::path jsxPath = projectDir / "app.jsx";
  bool hasJs = fs::exists(jsPath);
  bool hasJsx = fs::exists(jsxPath);
  if (hasJs && hasJsx) {
    std::cerr << "artisan-cli: " << projectDir
               << " has both app.js and app.jsx - a project has exactly one script entry point. Remove the "
                  "one you don't want.\n";
    std::exit(1);
  }
  if (hasJs) {
    discovered.jsPath = fs::absolute(jsPath);
  } else if (hasJsx) {
    discovered.jsxPath = fs::absolute(jsxPath);
  }

  fs::path jsPreludePath = projectDir / "js-prelude.js";
  if (fs::exists(jsPreludePath)) {
    discovered.jsPreludePath = fs::absolute(jsPreludePath);
  }

  // An ART app's entry point is a single file, app.ts or app.tsx (the
  // latter needed only to use JSX syntax in the entry point itself - see
  // Parser::jsxEnabled) - see ModuleResolver (art/module_resolver.cpp)
  // for how it pulls in any other .ts/.tsx files it imports from there.
  // Optional - a project can be JS-only.
  fs::path artTsPath = projectDir / "app.ts";
  fs::path artTsxPath = projectDir / "app.tsx";
  bool hasArtTs = fs::exists(artTsPath);
  bool hasArtTsx = fs::exists(artTsxPath);
  if (hasArtTs && hasArtTsx) {
    std::cerr << "artisan-cli: " << projectDir
               << " has both app.ts and app.tsx - a project has exactly one entry point. Remove the one "
                  "you don't want.\n";
    std::exit(1);
  }
  if (hasArtTs) {
    discovered.artPath = fs::absolute(artTsPath);
  } else if (hasArtTsx) {
    discovered.artPath = fs::absolute(artTsxPath);
  }

  return discovered;
}

// The tail RunBuild shares across every project build: configure, build,
// optionally copy the binary out and/or run it.
// `htmlEntries` items are each either a bare absolute path (artisanc
// derives the page name from its stem) or "name=path" (an explicit name,
// for a nested route - see DiscoverPages).
int ConfigureAndBuild(const std::vector<std::string> &htmlEntries,
                       const fs::path &jsAbs, const fs::path &jsxAbs,
                       const fs::path &jsPreludeAbs, const fs::path &artAbs,
                       const std::string &buildDirStr,
                       const std::string &outputPathStr, bool run) {
  fs::path projectSourceDir = ARTISAN_PROJECT_SOURCE_DIR;
  fs::path buildDir = fs::absolute(buildDirStr);

  std::ostringstream configureCmd;
  configureCmd << "cmake -S " << ShellQuote(projectSourceDir.string())
               << " -B " << ShellQuote(buildDir.string())
               << " -DARTISAN_UI_SOURCES="
               << ShellQuote(JoinStrings(htmlEntries));

  configureCmd << " -DARTISAN_APP_JS_SOURCE="
               << ShellQuote(jsAbs.empty() ? "" : jsAbs.string());

  configureCmd << " -DARTISAN_APP_JSX_SOURCE="
               << ShellQuote(jsxAbs.empty() ? "" : jsxAbs.string());

  configureCmd << " -DARTISAN_JS_PRELUDE_SOURCE="
               << ShellQuote(jsPreludeAbs.empty() ? "" : jsPreludeAbs.string());

  // Always set, even when empty: ARTISAN_APP_ART_SOURCE is a CACHE
  // STRING with its own default (this repo's own demo assets/app.tsx,
  // for building it directly with plain cmake) that would otherwise
  // stick around from a previous configure of this same build dir - a
  // JS-only project (empty artAbs) needs that cleared, not silently
  // left as whatever it was, to actually be JS-only.
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
  std::cerr << "usage: artisan-cli new <project-dir>\n\n"
               "Scaffolds a new artisan project at <project-dir>:\n"
               "  pages/index.html   a bare mount point (<div id=\"root\">)\n"
               "                     - the real UI is built in app.tsx\n"
               "                     below. Add more pages/*.html (or\n"
               "                     pages/some-folder/*.html for a nested\n"
               "                     route, Next.js-style) and link between\n"
               "                     them with <a href=\"...\">\n\n"
               "  app.tsx            an ART app (see art/) - a statically\n"
               "                     typed, TypeScript-like language\n"
               "                     compiled ahead of time with the ART\n"
               "                     compiler, with JSX (<div>...</div>)\n"
               "                     for building UI. Top-level statements\n"
               "                     run once per page load, and code\n"
               "                     reaches the DOM through the ambient\n"
               "                     `document` - see README.md's \"Using\n"
               "                     ART\" section.\n\n"
               "A project can also add an app.js/app.jsx of its own,\n"
               "interpreted at runtime via QuickJS alongside app.tsx - see\n"
               "README.md's \"Using JavaScript\" section for how the two\n"
               "combine on one page.\n"
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

// A new project's own index.html - app.tsx builds its actual UI at
// runtime, the same way a bundler-based React app's own index.html is
// normally just a mount point too - the interesting markup lives in
// code, not here. Still real HTML artisanc compiles ahead of time like
// any other page (DiscoverPages requires at least one), just about as
// bare as one can be while still giving app.tsx an id to find and
// build into. If a project also adds its own app.js/app.jsx (see
// README.md's "Using JavaScript" section), give it a separate mount
// point of its own here rather than reusing "root" -
// document.getElementById searches the *whole* document, not a
// caller-scoped subtree, so two independent scripts sharing one id
// would silently read/write each other's element instead of their own.
constexpr const char *kIndexHtmlTemplate = R"html(<!doctype html>
<html>
  <head>
    <title>My artisan app</title>
  </head>
  <body>
    <div id="root"></div>
  </body>
</html>
)html";

// setupApp is the ART counterpart to SetupApp/ArtisanSetupApp above - see
// art_bridge.h for the full `declare`-able DOM API and README.md's "Using
// ART" section. Unlike the Go/C++ templates, there's no boilerplate
// trampoline to write by hand - the artisan build itself adapts a plain
// `function setupApp(): void` into ArtisanSetupApp.
constexpr const char *kAppArtTemplate = R"art(// Your app's own ART code goes here - see art/stdlib/art.ts for the DOM
// bridge (Node/Event) this imports from ART's standard library, and
// README.md's "Using ART" section for the language itself, including
// "JSX (.tsx)" for the <tag>...</tag> element-literal syntax below - a
// real expression, not a template: <div>...</div> compiles straight to
// document.createElement/.setAttribute/.appendChild calls.
import { Node, Event } from "art";

// A top-level `let`/`const` is state that survives across calls - e.g. a
// click handler's own counter. Its initializer runs once, before any
// page has ever loaded, so it can be any expression EXCEPT one that
// touches `document` (which doesn't exist yet at that point) - see
// README.md's "Using ART" section for why.
let clickCount: number = 0;

// Every node.addEventListener handler - click included, since there's no
// separate zero-argument "onclick"-style listener kind - takes the Event
// itself as its one parameter. It gets no Node of its own (unlike
// setupApp), so it reaches the DOM through the ambient `document`
// instead, e.g. `document.getElementById(id)`.
function onButtonClick(event: Event): void {
  clickCount++;
  let label: Node = document.getElementById("count-label");
  if (!label.isNull()) {
    label.textContent = `Clicked ${numberToString(clickCount)} times`;
  }
}

// Your native startup code goes here - no `function setupApp(): void`
// wrapper needed. These are ordinary top-level statements, run once per
// page load (main.cpp calls the exact same "setupApp" symbol it always
// has - the compiler just generates it from this code instead of a
// user-written function now). If you'd rather keep the explicit
// function form (e.g. to match older ART code), write `function
// setupApp(): void { ... }` instead - the two are interchangeable, but
// not combinable in the same project.
//
// A `let` here, unlike `clickCount` above, touches `document` - so it's
// automatically a per-page-load local instead of a persistent global,
// re-run fresh every time this code runs. No `{ }` needed just for that
// (still legal, e.g. to deliberately scope a group of locals - just not
// required here).
//
// pages/index.html is deliberately just a mount point (a bare <div
// id="root">) - the actual UI is built here, in code, the same way a
// bundler-based React app's own index.html usually just has one too.
let root: Node = document.getElementById("root");
if (!root.isNull()) {
  root.appendChild(
    <div>
      <p id="count-label">{"Clicked 0 times"}</p>
      <button onclick={onButtonClick}>{"Click me"}</button>
    </div>
  );
}
)art";

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
  if (argc != 3) {
    PrintNewUsage();
    return 1;
  }

  fs::path projectDir = argv[2];

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
  WriteFile(projectDir / "app.tsx", kAppArtTemplate);

  std::cout << "artisan-cli: created a new project at "
            << fs::absolute(projectDir).string() << "\n\n"
            << "Next steps:\n"
            << "  artisan-cli build " << fs::absolute(projectDir).string()
            << " --run\n";
  return 0;
}

} // namespace

// `artisan-cli build <project-dir> [--build-dir <dir>] [-o <output>]
// [--run]` - always a project directory; its app.tsx/app.js(x) get
// discovered instead of named one by one (see DiscoverProject).
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

  return ConfigureAndBuild(discovered.htmlEntries, discovered.jsPath,
                            discovered.jsxPath, discovered.jsPreludePath,
                            discovered.artPath, buildDirStr, outputPathStr,
                            run);
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
  if (command == "build") {
    return RunBuild(argc, argv);
  }

  std::cerr << "artisan-cli: unknown command \"" << command << "\"\n";
  PrintTopUsage();
  return 1;
}
