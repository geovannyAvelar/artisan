#ifndef ART_SEMA_H
#define ART_SEMA_H

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "ast.h"

namespace ART {

class Sema {
public:
  // Type-checks the whole program in place, filling in every Expr's
  // resolvedType, every Stmt's resolvedVarType, and every
  // Param/InterfaceField's resolvedType. Returns true iff Diagnostics() is
  // empty.
  bool Check(Program &program);

  const std::vector<std::string> &Diagnostics() const { return diagnostics; }

  // Available after a successful Check() - Codegen uses these instead of
  // re-deriving interface/function lookup tables itself.
  const std::unordered_map<std::string, InterfaceDecl *> &Interfaces() const { return interfaces; }
  const std::unordered_map<std::string, FunctionDecl *> &Functions() const { return functions; }

  struct VarInfo {
    ResolvedType type;
    bool isConst;
  };

  // Every top-level `let`/`const`, keyed by name - Codegen builds one
  // LLVM global per entry, using the same resolved type/constness Sema
  // already computed here.
  const std::unordered_map<std::string, VarInfo> &Globals() const { return globals; }

private:
  std::vector<std::string> diagnostics;
  std::unordered_map<std::string, InterfaceDecl *> interfaces;
  std::unordered_map<std::string, FunctionDecl *> functions;
  std::unordered_map<std::string, VarInfo> globals;
  std::vector<std::unordered_map<std::string, VarInfo>> scopes;
  FunctionDecl *currentFunction = nullptr;

  // Built-in functions with no ART source/body of their own - Codegen
  // generates their LLVM definitions directly (see
  // Codegen::GenBuiltinNumberToString). Owned here only so `functions`
  // has somewhere valid to point; never added to Program::functions.
  std::vector<std::unique_ptr<FunctionDecl>> builtins;
  void SeedBuiltins();

  void Error(SourceLoc loc, const std::string &message);

  ResolvedType ResolveType(TypeNode *node);

  void PushScope();
  void PopScope();
  VarInfo *Lookup(const std::string &name);
  void Declare(SourceLoc loc, const std::string &name, ResolvedType type, bool isConst);

  void CheckGlobalDecl(Stmt *stmt);
  void RegisterFunctionSignature(FunctionDecl *fn);
  void CheckFunctionBody(FunctionDecl *decl);
  void CheckStmt(Stmt *stmt);
  ResolvedType CheckExpr(Expr *expr, const ResolvedType *expected);

  // Resolves and validates an assignment/increment target (Identifier not
  // declared const, an array element, or a struct field - never a string
  // character, which is immutable). Used by both Assign and IncDec.
  ResolvedType CheckLValueTarget(Expr *target, SourceLoc opLoc);

  bool AlwaysReturns(Stmt *stmt);
};

} // namespace ART

#endif
