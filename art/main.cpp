#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/wait.h>

#include "ast.h"
#include "codegen.h"
#include "module_resolver.h"
#include "parser.h"
#include "sema.h"
#include "tokenizer.h"

#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/TargetParser/Host.h"

namespace {

struct Options {
  std::string inputPath;
  std::string outputPath;
  std::string targetTriple;
  bool emitLLVM = false;
  bool emitObj = false;
};

void PrintUsage() {
  std::cerr << "usage: art <input.ts> [-o <output>] [--target <triple>] [--emit-llvm] [--emit-obj]\n";
}

bool ParseArgs(int argc, char *argv[], Options &opts) {
  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];
    if (arg == "-o") {
      if (++i >= argc) { std::cerr << "error: -o requires a path\n"; return false; }
      opts.outputPath = argv[i];
    } else if (arg == "--target") {
      if (++i >= argc) { std::cerr << "error: --target requires a triple\n"; return false; }
      opts.targetTriple = argv[i];
    } else if (arg == "--emit-llvm") {
      opts.emitLLVM = true;
    } else if (arg == "--emit-obj") {
      opts.emitObj = true;
    } else if (arg == "-h" || arg == "--help") {
      PrintUsage();
      std::exit(0);
    } else if (!arg.empty() && arg[0] == '-') {
      std::cerr << "error: unknown option '" << arg << "'\n";
      return false;
    } else if (opts.inputPath.empty()) {
      opts.inputPath = arg;
    } else {
      std::cerr << "error: unexpected extra argument '" << arg << "'\n";
      return false;
    }
  }
  if (opts.inputPath.empty()) {
    std::cerr << "error: input file is absent\n";
    return false;
  }
  return true;
}

std::string ReadFile(const std::string &path) {
  if (!std::filesystem::exists(path)) {
    throw std::runtime_error("file does not exist: " + path);
  }
  std::ifstream file(path);
  if (!file.is_open()) {
    throw std::runtime_error("could not open file: " + path);
  }
  std::ostringstream sstr;
  sstr << file.rdbuf();
  return sstr.str();
}

void ReportDiagnostics(const std::string &path, const std::vector<std::string> &diagnostics) {
  for (const auto &d : diagnostics) std::cerr << path << ":" << d << "\n";
}

// Runs `command`, returning its exit code (or -1 if it could not be started).
int RunCommand(const std::string &command) {
  int status = std::system(command.c_str());
  if (status == -1) return -1;
  if (WIFEXITED(status)) return WEXITSTATUS(status);
  return status;
}

} // namespace

int main(int argc, char *argv[]) {
  Options opts;
  if (!ParseArgs(argc, argv, opts)) {
    PrintUsage();
    return 1;
  }

  std::string source;
  try {
    source = ReadFile(opts.inputPath);
  } catch (const std::exception &e) {
    std::cerr << "error: " << e.what() << "\n";
    return 1;
  }

  ART::Tokenizer tokenizer(source);
  std::vector<ART::Token> tokens = tokenizer.Tokenize();
  if (!tokens.empty() && tokens.back().kind == ART::TokenKind::Error) {
    const ART::Token &t = tokens.back();
    std::cerr << opts.inputPath << ":" << t.loc.line << ":" << t.loc.col << ": " << t.text << "\n";
    return 1;
  }

  ART::Parser parser(std::move(tokens));
  ART::Program program = parser.ParseProgram();
  if (!parser.Diagnostics().empty()) {
    ReportDiagnostics(opts.inputPath, parser.Diagnostics());
    return 1;
  }

  // A file with no `import`s at all goes through exactly the single-file
  // path this always has - the ModuleResolver below (and the per-file
  // visibility enforcement it enables) is opt-in, triggered only by a
  // program actually using it, so nothing about compiling a plain,
  // import-free .ts file changes at all, error message formatting
  // included (ModuleResolver's own diagnostics are prefixed with each
  // file's canonical path, not necessarily the same spelling `opts.
  // inputPath` used on the command line).
  const std::unordered_map<std::string, std::unordered_set<std::string>> *visibility = nullptr;
  std::unordered_map<std::string, std::unordered_set<std::string>> visibilityStorage;
  if (!program.imports.empty()) {
    ART::ModuleResolver resolver;
    program = resolver.ResolveAndMerge(opts.inputPath);
    if (!resolver.Diagnostics().empty()) {
      for (const std::string &d : resolver.Diagnostics()) std::cerr << d << "\n";
      return 1;
    }
    visibilityStorage = resolver.Visibility();
    visibility = &visibilityStorage;
  }

  ART::Sema sema;
  if (!sema.Check(program, visibility)) {
    ReportDiagnostics(opts.inputPath, sema.Diagnostics());
    return 1;
  }

  if (opts.targetTriple.empty()) opts.targetTriple = llvm::sys::getDefaultTargetTriple();

  llvm::InitializeAllTargetInfos();
  llvm::InitializeAllTargets();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmParsers();
  llvm::InitializeAllAsmPrinters();

  std::string moduleName = std::filesystem::path(opts.inputPath).stem().string();
  ART::Codegen codegen(moduleName, opts.targetTriple, sema);

  std::unique_ptr<llvm::Module> module;
  try {
    module = codegen.Generate(program);
  } catch (const std::exception &e) {
    std::cerr << opts.inputPath << ": codegen error: " << e.what() << "\n";
    return 1;
  }

  std::string verifyErrors;
  llvm::raw_string_ostream verifyStream(verifyErrors);
  if (llvm::verifyModule(*module, &verifyStream)) {
    std::cerr << opts.inputPath << ": internal compiler error - generated IR failed verification:\n"
              << verifyErrors;
    return 1;
  }

  std::filesystem::path stem = std::filesystem::path(opts.inputPath).stem();

  if (opts.emitLLVM) {
    std::string outPath = opts.outputPath.empty() ? stem.string() + ".ll" : opts.outputPath;
    std::error_code ec;
    llvm::raw_fd_ostream out(outPath, ec);
    if (ec) {
      std::cerr << "error: could not open '" << outPath << "': " << ec.message() << "\n";
      return 1;
    }
    module->print(out, nullptr);
    std::cout << "wrote " << outPath << "\n";
    return 0;
  }

  std::string objPath = opts.emitObj ? (opts.outputPath.empty() ? stem.string() + ".o" : opts.outputPath)
                                      : stem.string() + ".art.o.tmp";
  {
    std::error_code ec;
    llvm::raw_fd_ostream objStream(objPath, ec, llvm::sys::fs::OF_None);
    if (ec) {
      std::cerr << "error: could not open '" << objPath << "': " << ec.message() << "\n";
      return 1;
    }
    llvm::legacy::PassManager passManager;
    if (codegen.GetTargetMachine()->addPassesToEmitFile(passManager, objStream, nullptr,
                                                         llvm::CodeGenFileType::ObjectFile)) {
      std::cerr << "error: target machine cannot emit an object file for '" << opts.targetTriple << "'\n";
      return 1;
    }
    passManager.run(*module);
  }

  if (opts.emitObj) {
    std::cout << "wrote " << objPath << "\n";
    return 0;
  }

  std::string outputPath = opts.outputPath.empty() ? "a.out" : opts.outputPath;
  std::string linkCmd = "clang --target=" + opts.targetTriple + " -o " + outputPath + " " + objPath + " -lgc";
  int linkStatus = RunCommand(linkCmd);
  std::filesystem::remove(objPath);
  if (linkStatus != 0) {
    std::cerr << "error: linking failed (exit " << linkStatus << ") - is a linker for '" << opts.targetTriple
              << "' available?\n";
    return 1;
  }

  std::cout << "wrote " << outputPath << "\n";
  return 0;
}
