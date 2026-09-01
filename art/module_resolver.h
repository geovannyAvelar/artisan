#ifndef ART_MODULE_RESOLVER_H
#define ART_MODULE_RESOLVER_H

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ast.h"

namespace ART {

// Resolves an entry file's `import { ... } from "./path";` graph into one
// merged Program - parses every (transitively) imported file, detects
// missing files/circular imports/importing a non-exported name, and
// flattens everything into the same interfaces/functions/externFunctions/
// globals vectors a single-file Program already has, each declaration
// tagged with which file it came from (FunctionDecl::sourceFile, etc.).
//
// Deliberately not a "real" per-file namespace: every top-level name
// still has to be globally unique across the whole merged program (Sema's
// existing duplicate-declaration checks, unchanged, enforce this) - `export`
// controls *access* (can another file reference this name at all), not
// name shadowing/privacy the way real ES modules also provide. See
// Visibility() below, which is what actually enforces that access
// control - Sema consults it at every identifier/type lookup.
class ModuleResolver {
public:
  // Empty on any resolution error - check Diagnostics().empty() before
  // using either this or Visibility().
  Program ResolveAndMerge(const std::string &entryPath);

  const std::vector<std::string> &Diagnostics() const { return diagnostics; }

  // canonical file path -> every name legally referenceable from that
  // file (its own top-level declarations, plus whatever it actually
  // imported - never anything merely declared/exported elsewhere but not
  // imported). Sema treats builtins (numberToString, ...) as always
  // visible regardless of this map.
  const std::unordered_map<std::string, std::unordered_set<std::string>> &Visibility() const { return visibility; }

private:
  std::vector<std::string> diagnostics;
  std::unordered_map<std::string, std::unordered_set<std::string>> visibility;

  // canonical path -> that file's own freshly parsed Program, not yet
  // merged into anything - populated by Resolve, consumed by
  // ResolveAndMerge once every file is done.
  std::unordered_map<std::string, Program> parsedFiles;
  std::vector<std::string> inProgress; // cycle detection
  std::vector<std::string> order;      // dependencies before dependents

  void Error(SourceLoc loc, const std::string &file, const std::string &message);

  // Resolves `importPath` (as written in `fromFile`'s own import
  // statement) to a canonical, existing file path. Empty string (with a
  // diagnostic already recorded) on failure.
  std::string ResolveImportPath(const std::string &fromFile, const std::string &importPath, SourceLoc loc);

  void Resolve(const std::string &canonicalPath); // recursive DFS
};

} // namespace ART

#endif
