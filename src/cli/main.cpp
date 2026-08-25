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

#include <algorithm>
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
               "  new <project-dir>   Scaffold a new native project.\n"
               "  build               Compile a project into a binary.\n\n"
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
         "  <project-dir>/pages/*.html   every page in the bundle, named\n"
         "                               after its stem (\"about.html\"\n"
         "                               becomes \"about\"); index.html, if\n"
         "                               present, is the page the app opens\n"
         "                               on, otherwise the first\n"
         "                               alphabetically. An <a href=\"...\">\n"
         "                               anywhere in the bundle can\n"
         "                               navigate to any other page.\n"
         "  <project-dir>/src/*.cpp      every native C++ source.\n"
         "  <project-dir>/app.js         optional embedded script.\n\n"
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

int RunCommand(const std::string &command) {
  std::cout << "$ " << command << "\n";
  return std::system(command.c_str());
}

// Every absolute .html file directly inside `dir`, sorted for a
// deterministic build across runs/filesystems - directory_iterator order
// isn't guaranteed. "index.html", if present, is moved to the front,
// since it's the page the app opens on (see main.cpp's Navigate()).
std::vector<fs::path> SortedFilesWithExtension(const fs::path &dir,
                                                const std::string &extension) {
  std::vector<fs::path> paths;
  if (fs::is_directory(dir)) {
    for (const auto &entry : fs::directory_iterator(dir)) {
      if (entry.is_regular_file() && entry.path().extension() == extension) {
        paths.push_back(fs::absolute(entry.path()));
      }
    }
  }
  std::sort(paths.begin(), paths.end());
  return paths;
}

struct DiscoveredProject {
  std::vector<fs::path> htmlPaths;
  std::vector<fs::path> cppPaths;
  fs::path jsPath; // Empty if app.js doesn't exist.
};

// The layout `artisan-cli new` scaffolds (see RunNew below): every page's
// markup under pages/, every native C++ source under src/, and one
// optional shared app.js at the project root. Exits the process with an
// error if pages/ is missing or empty - a bundle needs at least one page.
DiscoveredProject DiscoverProject(const fs::path &projectDir) {
  DiscoveredProject discovered;

  fs::path pagesDir = projectDir / "pages";
  discovered.htmlPaths = SortedFilesWithExtension(pagesDir, ".html");
  if (discovered.htmlPaths.empty()) {
    std::cerr << "artisan-cli: " << pagesDir
               << " doesn't exist or has no .html files - a project needs "
                  "at least one page\n";
    std::exit(1);
  }

  auto indexIt = std::find_if(
      discovered.htmlPaths.begin(), discovered.htmlPaths.end(),
      [](const fs::path &path) { return path.filename() == "index.html"; });
  if (indexIt != discovered.htmlPaths.end()) {
    std::iter_swap(discovered.htmlPaths.begin(), indexIt);
  }

  discovered.cppPaths = SortedFilesWithExtension(projectDir / "src", ".cpp");

  fs::path jsPath = projectDir / "app.js";
  if (fs::exists(jsPath)) {
    discovered.jsPath = fs::absolute(jsPath);
  }

  return discovered;
}

// The tail both build modes (project-directory and explicit --html/--cpp/
// --js) share once they've settled on the same three lists of files:
// configure, build, optionally copy the binary out and/or run it.
int ConfigureAndBuild(const std::vector<fs::path> &htmlAbs,
                       const std::vector<fs::path> &cppAbs,
                       const fs::path &jsAbs, const std::string &buildDirStr,
                       const std::string &outputPathStr, bool run) {
  fs::path projectSourceDir = ARTISAN_PROJECT_SOURCE_DIR;
  fs::path buildDir = fs::absolute(buildDirStr);

  std::ostringstream configureCmd;
  configureCmd << "cmake -S " << ShellQuote(projectSourceDir.string())
               << " -B " << ShellQuote(buildDir.string())
               << " -DARTISAN_UI_SOURCES=" << ShellQuote(JoinPaths(htmlAbs));

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
               "                     opens on; add more pages/*.html and\n"
               "                     link between them with <a href=\"...\">\n"
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

  return ConfigureAndBuild(discovered.htmlPaths, discovered.cppPaths,
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

  std::vector<fs::path> htmlAbs;
  for (const std::string &path : options.htmlPaths) {
    htmlAbs.push_back(ResolveExisting(path));
  }

  std::vector<fs::path> cppAbs;
  for (const std::string &path : options.cppPaths) {
    cppAbs.push_back(ResolveExisting(path));
  }

  fs::path jsAbs;
  if (!options.jsPath.empty()) {
    jsAbs = ResolveExisting(options.jsPath);
  }

  return ConfigureAndBuild(htmlAbs, cppAbs, jsAbs, options.buildDir,
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
  if (command == "build") {
    return RunBuild(argc, argv);
  }

  std::cerr << "artisan-cli: unknown command \"" << command << "\"\n";
  PrintTopUsage();
  return 1;
}
