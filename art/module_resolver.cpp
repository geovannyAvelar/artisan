#include "module_resolver.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "parser.h"
#include "tokenizer.h"

namespace ART {

namespace fs = std::filesystem;

void ModuleResolver::Error(SourceLoc loc, const std::string &file, const std::string &message) {
  std::ostringstream oss;
  oss << file << ":" << loc.line << ":" << loc.col << ": " << message;
  diagnostics.push_back(oss.str());
}

std::string ModuleResolver::ResolveImportPath(const std::string &fromFile, const std::string &importPath,
                                               SourceLoc loc) {
  fs::path candidate = fs::path(fromFile).parent_path() / importPath;
  if (candidate.extension() != ".ts") candidate += ".ts";

  std::error_code ec;
  fs::path canonical = fs::canonical(candidate, ec);
  if (ec) {
    Error(loc, fromFile, "cannot find imported file '" + importPath + "' (looked for " + candidate.string() + ")");
    return "";
  }
  return canonical.string();
}

void ModuleResolver::Resolve(const std::string &canonicalPath) {
  if (parsedFiles.count(canonicalPath)) return; // already resolved elsewhere in the graph

  if (std::find(inProgress.begin(), inProgress.end(), canonicalPath) != inProgress.end()) {
    std::ostringstream cycle;
    for (const std::string &p : inProgress) cycle << fs::path(p).filename().string() << " -> ";
    cycle << fs::path(canonicalPath).filename().string();
    diagnostics.push_back("circular import: " + cycle.str());
    return;
  }

  std::ifstream file(canonicalPath);
  if (!file.is_open()) {
    diagnostics.push_back(canonicalPath + ": could not open file");
    return;
  }
  std::ostringstream sstr;
  sstr << file.rdbuf();
  std::string source = sstr.str();

  inProgress.push_back(canonicalPath);

  Tokenizer tokenizer(source);
  std::vector<Token> tokens = tokenizer.Tokenize();
  if (!tokens.empty() && tokens.back().kind == TokenKind::Error) {
    Error(tokens.back().loc, canonicalPath, tokens.back().text);
    inProgress.pop_back();
    return;
  }

  Parser parser(std::move(tokens));
  Program program = parser.ParseProgram();
  if (!parser.Diagnostics().empty()) {
    for (const std::string &d : parser.Diagnostics()) diagnostics.push_back(canonicalPath + ":" + d);
    inProgress.pop_back();
    return;
  }

  // Depth-first: every file this one imports is fully resolved (and
  // appended to `order`) before this file itself is, so `order` ends up
  // dependencies-before-dependents - not load-bearing for Sema (which
  // already forward-references via its own two-phase register-then-check
  // design regardless of vector order), just a more predictable one for
  // diagnostics/debugging.
  for (auto &imp : program.imports) {
    std::string resolvedPath = ResolveImportPath(canonicalPath, imp->path, imp->loc);
    if (!resolvedPath.empty()) Resolve(resolvedPath);
  }

  inProgress.pop_back();

  if (!diagnostics.empty()) return; // a nested import already failed - stop accumulating more noise

  order.push_back(canonicalPath);
  parsedFiles[canonicalPath] = std::move(program);
}

Program ModuleResolver::ResolveAndMerge(const std::string &entryPath) {
  std::error_code ec;
  fs::path canonicalEntry = fs::canonical(entryPath, ec);
  if (ec) {
    diagnostics.push_back(entryPath + ": file does not exist");
    return Program{};
  }

  Resolve(canonicalEntry.string());
  if (!diagnostics.empty()) return Program{};

  // Pass 1: every file's own top-level names are always visible to
  // itself, import or not.
  for (const std::string &path : order) {
    Program &prog = parsedFiles.at(path);
    std::unordered_set<std::string> &vis = visibility[path];
    for (auto &i : prog.interfaces) vis.insert(i->name);
    for (auto &f : prog.functions) vis.insert(f->name);
    for (auto &f : prog.externFunctions) vis.insert(f->name);
    for (auto &g : prog.globals) vis.insert(g->varName);
  }

  // Pass 2: validate each import against the target file's own exports
  // and extend the importing file's visibility with whatever it actually
  // named.
  for (const std::string &path : order) {
    Program &prog = parsedFiles.at(path);
    for (auto &imp : prog.imports) {
      std::string targetPath = ResolveImportPath(path, imp->path, imp->loc);
      if (targetPath.empty()) continue; // already reported during Resolve()
      Program &target = parsedFiles.at(targetPath);

      for (auto &n : imp->names) {
        bool declared = false;
        bool exported = false;
        auto check = [&](const std::string &name, bool isExported) {
          if (name != n.name) return;
          declared = true;
          exported = exported || isExported;
        };
        for (auto &i : target.interfaces) check(i->name, i->isExported);
        for (auto &f : target.functions) check(f->name, f->isExported);
        for (auto &f : target.externFunctions) check(f->name, f->isExported);
        for (auto &g : target.globals) check(g->varName, g->isExported);

        if (!declared) {
          Error(n.loc, path, "'" + imp->path + "' has no export named '" + n.name + "'");
        } else if (!exported) {
          Error(n.loc, path, "'" + n.name + "' is declared in '" + imp->path +
                                  "' but not exported - add 'export' to its declaration there");
        } else {
          visibility[path].insert(n.name);
        }
      }
    }
  }

  if (!diagnostics.empty()) return Program{};

  Program merged;
  for (const std::string &path : order) {
    Program &prog = parsedFiles.at(path);
    for (auto &i : prog.interfaces) {
      i->sourceFile = path;
      merged.interfaces.push_back(std::move(i));
    }
    for (auto &f : prog.functions) {
      f->sourceFile = path;
      merged.functions.push_back(std::move(f));
    }
    for (auto &f : prog.externFunctions) {
      f->sourceFile = path;
      merged.externFunctions.push_back(std::move(f));
    }
    for (auto &g : prog.globals) {
      g->sourceFile = path;
      merged.globals.push_back(std::move(g));
    }
    // Dependency-first order (same as every list above) - an imported
    // file's own top-level statements run before the importing file's,
    // matching how real ES module evaluation order works.
    for (auto &s : prog.topLevelStmts) {
      s->sourceFile = path;
      merged.topLevelStmts.push_back(std::move(s));
    }
  }
  return merged;
}

} // namespace ART
