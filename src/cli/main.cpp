// artisan-cli - the single entry point that ties artisan's whole
// workflow together: scaffolding a new project (`new`), and compiling
// markup (artisanc), native C++ app code, and an optional embedded
// JavaScript file (embed_text) into one native binary (`build`).
//
// `build` does not reimplement compilation or linking itself - Skia,
// lexbor, and QuickJS all need to be found and wired together correctly,
// and the project's own CMakeLists.txt already knows how to do that. What
// it actually does is turn a simple command line into the right CMake
// configure + build invocation, with the caller's markup/C++/JS files
// substituted in via cache variables (ARTISAN_UI_SOURCES,
// ARTISAN_APP_CPP_SOURCES, ARTISAN_APP_JS_SOURCE) that CMakeLists.txt
// exposes for exactly this purpose. Compiling native code for artisan and
// setting up a QuickJS script are the same command - the split happens
// entirely inside the one build it drives.
//
// `new` writes out a starting point for one of those --cpp files -
// native and script are two different ways to drive the same Node tree
// (see app.h/js_engine.h), not two flavors of the same thing, so a
// scaffolded project is one or the other, never a blend of both.
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

struct Options {
  std::vector<std::string> htmlPaths;
  std::vector<std::string> cppPaths;
  std::string jsPath;
  std::string buildDir = "build";
  std::string outputPath;
  bool run = false;
};

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
         "                         [-o <output>] [--run]\n"
         "   or: artisan-cli build --html <file.html> [--html <file.html>]...\n"
         "                         [--cpp <file.cpp>]... [--js <file.js>]\n"
         "                         [--build-dir <dir>] [-o <output>] [--run]\n\n"
         "Given a project directory (the layout artisan-cli new scaffolds),\n"
         "its files are discovered automatically - no need to name each one:\n"
         "  <project-dir>/pages/**/*.html   every page in the bundle,\n"
         "        named after its path relative to pages/, folders and all\n"
         "        (Next.js-style): pages/about.html becomes \"about\";\n"
         "        pages/settings/profile.html becomes \"settings/profile\";\n"
         "        pages/settings/index.html becomes \"settings\" (a folder's\n"
         "        own index.html is that folder's route, same as the\n"
         "        project root's). pages/index.html, if present, is the\n"
         "        page the app opens on. An <a href=\"...\"> anywhere in\n"
         "        the bundle can navigate to any other page by name.\n"
         "  <project-dir>/src/**/*.cpp      every native C++ source,\n"
         "        nesting is just organization here (not a route).\n"
         "  <project-dir>/app.js            optional embedded script.\n\n"
         "Or list individual files by hand for full manual control:\n"
         "  --html <file>   A page's markup - same naming/ordering rules as\n"
         "                  pages/*.html above. Repeatable, at least one\n"
         "                  required.\n"
         "  --cpp <file>    Native C++ source - repeatable. Defaults to the\n"
         "                  project's own src/app.cpp if omitted.\n"
         "  --js <file>     Optional script embedded and run at startup.\n\n"
         "  --build-dir     Where to configure/build (default: ./build).\n"
         "  -o, --output    Copy the built binary here.\n"
         "  --run           Run the binary after a successful build.\n";
}

// Resolves `path` to an absolute path and exits with an error if it
// doesn't exist - every file this tool hands to CMake needs to be
// absolute, since the build directory (and so CMake's notion of "here")
// isn't necessarily the caller's current directory.
fs::path ResolveExisting(const std::string &path) {
  fs::path abs = fs::absolute(path);
  if (!fs::exists(abs)) {
    std::cerr << "artisan-cli: " << abs << " does not exist\n";
    std::exit(1);
  }
  return abs;
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
  fs::path jsPath; // Empty if app.js doesn't exist.
};

// The layout `artisan-cli new` scaffolds (see RunNew below): every page's
// markup under pages/ (nested folders form nested routes - see
// DiscoverPages), every native C++ source under src/, and one optional
// shared app.js at the project root. Exits the process with an error if
// pages/ is missing or empty - a bundle needs at least one page.
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

  return discovered;
}

// The tail both build modes (project-directory and explicit --html/--cpp/
// --js) share once they've settled on the same three lists of files:
// configure, build, optionally copy the binary out and/or run it.
// `htmlEntries` items are each either a bare absolute path (artisanc
// derives the page name from its stem) or "name=path" (an explicit name,
// for a nested route - see DiscoverPages).
int ConfigureAndBuild(const std::vector<std::string> &htmlEntries,
                       const std::vector<fs::path> &cppAbs,
                       const fs::path &jsAbs, const std::string &buildDirStr,
                       const std::string &outputPathStr, bool run) {
  fs::path projectSourceDir = ARTISAN_PROJECT_SOURCE_DIR;
  fs::path buildDir = fs::absolute(buildDirStr);

  std::ostringstream configureCmd;
  configureCmd << "cmake -S " << ShellQuote(projectSourceDir.string())
               << " -B " << ShellQuote(buildDir.string())
               << " -DARTISAN_UI_SOURCES="
               << ShellQuote(JoinStrings(htmlEntries));

  if (!cppAbs.empty()) {
    configureCmd << " -DARTISAN_APP_CPP_SOURCES="
                 << ShellQuote(JoinPaths(cppAbs));
  }

  configureCmd << " -DARTISAN_APP_JS_SOURCE="
               << ShellQuote(jsAbs.empty() ? "" : jsAbs.string());

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
               "Scaffolds a new native artisan project at <project-dir>:\n"
               "  pages/index.html   starter markup - the page the app\n"
               "                     opens on; add more pages/*.html (or\n"
               "                     pages/some-folder/*.html for a nested\n"
               "                     route, Next.js-style) and link between\n"
               "                     them with <a href=\"...\">\n"
               "  src/main.cpp       native SetupApp(Node&) - see\n"
               "                     include/app.h. Add more src/*.cpp as\n"
               "                     the project grows - every one gets\n"
               "                     compiled in, no need to list them.\n\n"
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
  CreateDirectory(projectDir / "src");

  WriteFile(projectDir / "pages" / "index.html", kIndexHtmlTemplate);
  WriteFile(projectDir / "src" / "main.cpp", kMainCppTemplate);

  std::cout << "artisan-cli: created a new native project at "
            << fs::absolute(projectDir).string() << "\n\n"
            << "Next steps:\n"
            << "  artisan-cli build " << fs::absolute(projectDir).string()
            << " --run\n";
  return 0;
}

void PrintComponentUsage() {
  std::cerr
      << "usage: artisan-cli component <project-dir> <name>\n\n"
         "Scaffolds a reusable component in an existing project:\n"
         "  components/<name>.html     markup fragment - paste this into\n"
         "                             any page under pages/ wherever you\n"
         "                             want it to appear. Not itself a\n"
         "                             routable page.\n"
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

bool LooksLikeFlag(const std::string &arg) {
  return !arg.empty() && arg.front() == '-';
}

// `artisan-cli build <project-dir> [--build-dir <dir>] [-o <output>]
// [--run]` - argv[2] is a bare path, not a flag, so its pages/src/app.js
// get discovered instead of named one by one (see DiscoverProject).
int RunBuildFromProject(int argc, char *argv[]) {
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
    } else if (arg == "--html" || arg == "--cpp" || arg == "--js") {
      std::cerr << "artisan-cli: " << arg
                 << " can't be combined with a project directory - its "
                    "files are discovered automatically. Pass individual "
                    "--html/--cpp files instead of a directory for manual "
                    "control.\n";
      return 1;
    } else {
      std::cerr << "artisan-cli: unknown argument " << arg << "\n";
      PrintBuildUsage();
      return 1;
    }
  }

  return ConfigureAndBuild(discovered.htmlEntries, discovered.cppPaths,
                            discovered.jsPath, buildDirStr, outputPathStr,
                            run);
}

// `artisan-cli build --html ... [--cpp ...] [--js ...] ...` - the fully
// manual mode, for a layout DiscoverProject doesn't fit (or just full
// control over exactly which files are included).
int RunBuildFromFlags(int argc, char *argv[]) {
  Options options;

  for (int i = 2; i < argc; ++i) {
    std::string arg = argv[i];
    auto next = [&]() -> std::string {
      if (i + 1 >= argc) {
        std::cerr << "artisan-cli: " << arg << " needs a value\n";
        std::exit(1);
      }
      return argv[++i];
    };

    if (arg == "--html") {
      options.htmlPaths.push_back(next());
    } else if (arg == "--cpp") {
      options.cppPaths.push_back(next());
    } else if (arg == "--js") {
      options.jsPath = next();
    } else if (arg == "--build-dir") {
      options.buildDir = next();
    } else if (arg == "-o" || arg == "--output") {
      options.outputPath = next();
    } else if (arg == "--run") {
      options.run = true;
    } else {
      std::cerr << "artisan-cli: unknown argument " << arg << "\n";
      PrintBuildUsage();
      return 1;
    }
  }

  if (options.htmlPaths.empty()) {
    std::cerr << "artisan-cli: at least one --html is required\n";
    PrintBuildUsage();
    return 1;
  }

  std::vector<std::string> htmlEntries;
  for (const std::string &path : options.htmlPaths) {
    // Bare path - artisanc derives this page's name from its stem, same
    // as always. --html has no syntax for an explicit/nested name; use a
    // project directory (DiscoverPages) for that.
    htmlEntries.push_back(ResolveExisting(path).string());
  }

  std::vector<fs::path> cppAbs;
  for (const std::string &path : options.cppPaths) {
    cppAbs.push_back(ResolveExisting(path));
  }

  fs::path jsAbs;
  if (!options.jsPath.empty()) {
    jsAbs = ResolveExisting(options.jsPath);
  }

  return ConfigureAndBuild(htmlEntries, cppAbs, jsAbs, options.buildDir,
                            options.outputPath, options.run);
}

} // namespace

int RunBuild(int argc, char *argv[]) {
  if (argc >= 3 && !LooksLikeFlag(argv[2])) {
    return RunBuildFromProject(argc, argv);
  }
  return RunBuildFromFlags(argc, argv);
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
