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

  // Every concrete instantiation of a generic function actually called
  // (`name::<Type,...>(...)`), keyed by its mangled name (see
  // MangleInstantiation) - Codegen generates one ordinary, fully-concrete
  // LLVM function per entry (a body-less one, same as any `declare
  // function`, if `body` is null - i.e. the template itself had no body).
  const std::unordered_map<std::string, FunctionDecl *> &Instantiations() const { return instantiations; }

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

  // Generic function/declare-function templates (`typeParams` non-empty),
  // keyed by name - kept separate from `functions` since a template isn't
  // directly callable on its own, only through a concrete instantiation
  // (see CheckGenericCall). Never body-checked in template form - see
  // Check()'s function-body loop, which skips these.
  std::unordered_map<std::string, FunctionDecl *> genericFunctions;

  // Every already-instantiated (name, concrete type args) pair, keyed by
  // MangleInstantiation's name - each a fully concrete, independently
  // checked clone of its template (see CheckGenericCall). Owns the clones
  // `instantiations` points into.
  std::unordered_map<std::string, FunctionDecl *> instantiations;
  std::vector<std::unique_ptr<FunctionDecl>> instantiationStorage;

  // Non-null only while checking a generic instantiation's own signature/
  // body (see CheckGenericCall) - ResolveType consults this before
  // treating a Named type as an interface/opaque-type reference, so `T`
  // resolves straight to the concrete type substituted at this call site
  // rather than ever producing a TypeParam node during instantiation
  // checking (TypeParam only ever appears when there's no substitution
  // active - see ResolveType).
  const std::unordered_map<std::string, ResolvedType> *currentSubstitution = nullptr;

  void Error(SourceLoc loc, const std::string &message);

  ResolvedType ResolveType(TypeNode *node);
  std::string MangleType(const ResolvedType &t);
  std::string MangleInstantiation(const std::string &name, const std::vector<ResolvedType> &typeArgs);

  // Resolves a Call's callee/type arguments and, on first use of a given
  // (name, concrete type args) pair, clones and checks the template's
  // signature/body with that substitution active - see the member doc
  // comments above for the pieces this coordinates. Returns the
  // instantiation's return type (Unknown on error) and sets
  // expr->resolvedCalleeName to the mangled name Codegen should call.
  ResolvedType CheckGenericCall(Expr *expr);

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
