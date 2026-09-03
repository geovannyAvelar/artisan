#ifndef ART_SEMA_H
#define ART_SEMA_H

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ast.h"

namespace ART {

class Sema {
public:
  // Type-checks the whole program in place, filling in every Expr's
  // resolvedType, every Stmt's resolvedVarType, and every
  // Param/InterfaceField's resolvedType. Returns true iff Diagnostics() is
  // empty.
  //
  // `visibility`, if given (see ModuleResolver::Visibility), maps each
  // declaration's own sourceFile to the set of names legally referenceable
  // from it (its own top-level names plus whatever it actually imported) -
  // every identifier/type/call lookup is checked against it in addition
  // to existing name resolution, so a name that exists somewhere in the
  // merged program but wasn't imported into the file trying to use it is
  // still rejected as inaccessible. Null (the default, and always the
  // case for a single file with no imports at all) disables this check
  // entirely - the original, fully flat/global behavior.
  bool Check(Program &program,
             const std::unordered_map<std::string, std::unordered_set<std::string>> *visibility = nullptr);

  const std::vector<std::string> &Diagnostics() const { return diagnostics; }

  // Available after a successful Check() - Codegen uses these instead of
  // re-deriving interface/function lookup tables itself.
  const std::unordered_map<std::string, InterfaceDecl *> &Interfaces() const { return interfaces; }
  const std::unordered_map<std::string, FunctionDecl *> &Functions() const { return functions; }

  struct VarInfo {
    ResolvedType type;
    bool isConst;
    // Non-null for exactly one of these - the declaration site this
    // binding came from - used only by Lookup's own capture-marking
    // side effect (see its doc comment). Both null for a global
    // (Globals()' own entries are built directly by CheckGlobalDecl
    // without going through Declare - see Lookup's own fallback, which
    // never applies capture-marking there, matching a global's real
    // "never capturable" semantics).
    Param *declParam = nullptr;
    Stmt *declStmt = nullptr;
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

  // Every ExprKind::FunctionExpr's own `fn`, appended the one time
  // CheckExpr actually visits that node (so a generic template's own,
  // never-checked-in-template-form closure is correctly excluded, and
  // each concrete instantiation's own clone contributes its own separate
  // entry) - Codegen uses this to know every closure that needs a thunk
  // generated, the same role Instantiations() already plays for generic-
  // function instantiations.
  const std::vector<FunctionDecl *> &Closures() const { return closures; }

private:
  std::vector<std::string> diagnostics;
  std::unordered_map<std::string, InterfaceDecl *> interfaces;
  std::unordered_map<std::string, FunctionDecl *> functions;
  std::unordered_map<std::string, VarInfo> globals;
  std::vector<std::unordered_map<std::string, VarInfo>> scopes;
  FunctionDecl *currentFunction = nullptr;

  // One entry per enclosing function/method/closure currently being
  // checked, outermost first - a real STACK (not just `currentFunction`
  // above, a single field, kept in sync as `frameStack.back()` or null
  // when empty - every existing read of `currentFunction` is unaffected)
  // because a closure nested inside a closure needs to see every
  // ancestor frame at once, to tell "my own frame's variable" from "an
  // ancestor's" and, when it's the latter, from which ancestor exactly -
  // see Lookup's own doc comment.
  std::vector<FunctionDecl *> frameStack;
  // Parallel to `scopes` above - records frameStack.size() at the moment
  // each entry of `scopes` was pushed. Folded directly into
  // PushScope()/PopScope() so every existing call site gets this for
  // free with no changes of its own.
  std::vector<size_t> scopeFrameDepth;
  int nextClosureId = 0;
  std::vector<FunctionDecl *> closures; // see Closures() above

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

  // Generic interface/declare-type templates (`typeParams` non-empty),
  // kept separate from `interfaces` the same way genericFunctions is kept
  // separate from `functions` - a template names a family of types, not
  // a type on its own, so it's never itself a valid ResolvedType. An
  // instantiation (e.g. `Box<number>`) resolves into a fully concrete
  // clone registered directly into `interfaces` under its mangled name
  // (see InstantiateInterface) - unlike function instantiations, these
  // need no separate storage/lookup map: `interfaces` already serves
  // both roles once Codegen only ever sees concrete names either way.
  std::unordered_map<std::string, InterfaceDecl *> genericInterfaces;
  std::vector<std::unique_ptr<InterfaceDecl>> interfaceInstantiationStorage;

  // Non-null only while checking a generic instantiation's own signature/
  // body (see CheckGenericCall) - ResolveType consults this before
  // treating a Named type as an interface/opaque-type reference, so `T`
  // resolves straight to the concrete type substituted at this call site
  // rather than ever producing a TypeParam node during instantiation
  // checking (TypeParam only ever appears when there's no substitution
  // active - see ResolveType).
  const std::unordered_map<std::string, ResolvedType> *currentSubstitution = nullptr;

  // See Check()'s own doc comment. Null unless a module graph was
  // resolved. `currentFile` is which declaration's own sourceFile is
  // presently being checked - always kept in lexical sync with whatever
  // TypeNode/Expr/Stmt is actually being resolved right now, including
  // through a generic instantiation's clone (checked against the
  // *template's* sourceFile, not the call site's - a generic function's
  // free identifiers resolve against where it was written, same as any
  // other function's would).
  const std::unordered_map<std::string, std::unordered_set<std::string>> *visibility = nullptr;
  std::string currentFile;
  std::unordered_set<std::string> builtinNames; // always visible regardless of `visibility` - see SeedBuiltins

  // True if `name` may be referenced from `currentFile` right now - see
  // the `visibility` member's own doc comment.
  bool IsVisible(const std::string &name) const;
  // "" if `name` is simply undeclared anywhere; otherwise a clarifying
  // suffix for an "undefined identifier" message, since `name` does
  // exist somewhere in the program but isn't visible from `currentFile` -
  // almost always a missing import rather than a typo.
  std::string VisibilityHint(const std::string &name) const;

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

  // Resolves a generic interface/declare-type reference (e.g. the
  // `Box<number>` a TypeNode's Named case with non-empty genericArgs
  // represents) to a Struct ResolvedType, cloning and resolving the
  // template's fields (with the substitution active) into `interfaces`
  // under the mangled name on first use of a given (name, concrete type
  // args) pair. Registered before its own fields are resolved, so a
  // self-referential generic interface (`interface Node<T> { next:
  // Node<T>; ... }`) resolves the recursive reference to the same
  // (already-registered, still being filled in) clone instead of
  // recursing forever - safe because every field is just a `ptr` in
  // Codegen either way, never needing the pointee to be complete.
  ResolvedType InstantiateInterface(InterfaceDecl *tmpl, const std::vector<ResolvedType> &typeArgs, SourceLoc loc);

  void PushScope();
  void PopScope();
  // Beyond a plain lookup, this has one side effect: if `name` resolves
  // to a scope that belongs to an ANCESTOR frame relative to whichever
  // frame is currently being checked (frameStack.back()) - i.e. a real
  // lexical capture, not just a deeper block scope within the SAME
  // function/closure - this marks the capture on both ends: (a) the
  // declaring Param/Stmt itself (isCapturedByClosure = true, read by
  // Codegen to decide boxed-vs-plain storage), and (b) every frame
  // strictly between the declaring one and the current one, inclusive of
  // the current one (FunctionDecl::captures) - not just the innermost
  // closure, so an intermediate closure that never itself reads `name`
  // still carries its cell pointer through its own env for whatever's
  // nested further inside it to find. Both of Lookup's call sites
  // (CheckExpr's Identifier case, CheckLValueTarget) get this
  // automatically - no call-site-specific logic needed. A real,
  // deliberate departure from "lookup doesn't mutate" - kept here rather
  // than split across each call site so the thread-through loop isn't
  // duplicated.
  VarInfo *Lookup(const std::string &name);
  void Declare(SourceLoc loc, const std::string &name, ResolvedType type, bool isConst,
               Param *declParam = nullptr, Stmt *declStmt = nullptr);

  void CheckGlobalDecl(Stmt *stmt);
  void RegisterFunctionSignature(FunctionDecl *fn);
  void CheckFunctionBody(FunctionDecl *decl);
  void CheckStmt(Stmt *stmt);
  ResolvedType CheckExpr(Expr *expr, const ResolvedType *expected);

  // Resolves and validates an assignment/increment target (Identifier not
  // declared const, an array element, a struct field, or a setter-backed
  // property - never a string character, which is immutable, and never a
  // getter-only property, which is read-only). Used by both Assign and
  // IncDec - IncDec additionally rejects a setter-backed property itself
  // right after calling this (see its own case in CheckExpr): there's no
  // way to support `obj.prop++` on one without also reading it back
  // through a getter first, which GenLValue's plain address-of model
  // can't express, so it's a deliberate, documented gap rather than a
  // half-built increment-only path.
  ResolvedType CheckLValueTarget(Expr *target, SourceLoc opLoc);

  // A class's field/getter/setter/plain-method namespace is shared - see
  // Check()'s own class-registration pass for the duplicate/ambiguity
  // rules these back. `name` is always the property's own bare,
  // unqualified name (see MangleGetter/MangleSetter/MangleMethod for how
  // it becomes the actual `functions`-map key once qualified).
  static const InterfaceField *FindField(const InterfaceDecl *iface, const std::string &name);
  FunctionDecl *FindGetter(InterfaceDecl *iface, const std::string &name);
  FunctionDecl *FindSetter(InterfaceDecl *iface, const std::string &name);
  FunctionDecl *FindPlainMethod(InterfaceDecl *iface, const std::string &name);
  static std::string MangleGetter(const std::string &className, const std::string &propName);
  static std::string MangleSetter(const std::string &className, const std::string &propName);
  static std::string MangleMethod(const std::string &className, const std::string &propName);

  bool AlwaysReturns(Stmt *stmt);

  // True if `expr` (already checked - this walks the already-rewritten
  // tree, so it sees the ambient `document` sugar's real target) directly
  // calls ArtDocument anywhere within it, recursively. Used by
  // CheckGlobalDecl to reject a global whose initializer would run
  // `document`/anything backed by it at process start, before any page
  // has ever loaded - unlike the well-documented "narrow window between
  // one page's tree tearing down and the next one finishing" ArtIsNull
  // covers, this is the *permanent* state until the first navigate()
  // call, so it's not a rare edge case worth just documenting, it's
  // always broken. Only catches a *direct* call in the initializer
  // itself, not one reached indirectly through another function it
  // calls - a real, narrower guarantee than "provably safe", but enough
  // to catch the obvious, easy-to-write mistake (e.g. `let button: Node
  // = document.getElementById(...)` at the bare top level, instead of
  // inside a block - see README.md's note on this).
  bool ExprUsesAmbientDocument(const Expr *expr) const;
};

} // namespace ART

#endif
