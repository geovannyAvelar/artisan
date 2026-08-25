// artisan-cli - the single entry point that ties artisan's whole
// workflow together: compiling markup (artisanc), compiling native C++
// app code, and embedding an optional JavaScript file (embed_text), into
// one native binary.
//
// This tool does not reimplement compilation or linking itself - Skia,
// lexbor, and QuickJS all need to be found and wired together correctly,
// and the project's own CMakeLists.txt already knows how to do that. What
// this tool actually does is turn a simple command line into the right
// CMake configure + build invocation, with the caller's markup/C++/JS
// files substituted in via cache variables (ARTISAN_UI_SOURCE,
// ARTISAN_APP_CPP_SOURCES, ARTISAN_APP_JS_SOURCE) that CMakeLists.txt
// exposes for exactly this purpose. Compiling native code for artisan and
// setting up a QuickJS script are the same command - the split happens
// entirely inside the one build it drives.

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

struct Options {
  std::string htmlPath;
  std::vector<std::string> cppPaths;
  std::string jsPath;
  std::string buildDir = "build";
  std::string outputPath;
  bool run = false;
};

void PrintUsage() {
  std::cerr
      << "usage: artisan-cli build --html <file.html> [--cpp <file.cpp>]...\n"
         "                         [--js <file.js>] [--build-dir <dir>]\n"
         "                         [-o <output>] [--run]\n\n"
         "  --html <file>   UI markup compiled into the app's initial\n"
         "                  document (required).\n"
         "  --cpp <file>    Native C++ source compiled into the app -\n"
         "                  repeatable. Defaults to the project's own\n"
         "                  src/app.cpp if omitted.\n"
         "  --js <file>     Optional script embedded and run at startup.\n"
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

int RunCommand(const std::string &command) {
  std::cout << "$ " << command << "\n";
  return std::system(command.c_str());
}

} // namespace

int main(int argc, char *argv[]) {
  if (argc < 2 || std::string(argv[1]) != "build") {
    PrintUsage();
    return 1;
  }

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
      options.htmlPath = next();
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
      PrintUsage();
      return 1;
    }
  }

  if (options.htmlPath.empty()) {
    std::cerr << "artisan-cli: --html is required\n";
    PrintUsage();
    return 1;
  }

  fs::path htmlAbs = ResolveExisting(options.htmlPath);

  std::vector<fs::path> cppAbs;
  for (const std::string &path : options.cppPaths) {
    cppAbs.push_back(ResolveExisting(path));
  }

  fs::path jsAbs;
  if (!options.jsPath.empty()) {
    jsAbs = ResolveExisting(options.jsPath);
  }

  fs::path projectDir = ARTISAN_PROJECT_SOURCE_DIR;
  fs::path buildDir = fs::absolute(options.buildDir);

  std::ostringstream configureCmd;
  configureCmd << "cmake -S " << ShellQuote(projectDir.string()) << " -B "
               << ShellQuote(buildDir.string())
               << " -DARTISAN_UI_SOURCE=" << ShellQuote(htmlAbs.string());

  if (!cppAbs.empty()) {
    std::ostringstream cppList;
    for (size_t i = 0; i < cppAbs.size(); ++i) {
      if (i > 0) {
        cppList << ";";
      }
      cppList << cppAbs[i].string();
    }
    configureCmd << " -DARTISAN_APP_CPP_SOURCES="
                 << ShellQuote(cppList.str());
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

  if (!options.outputPath.empty()) {
    fs::path outAbs = fs::absolute(options.outputPath);
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

  if (options.run) {
    return RunCommand(ShellQuote(builtBinary.string()));
  }

  return 0;
}
