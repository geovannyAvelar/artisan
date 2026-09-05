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
  std::unordered_map<std::string, EnumDecl *> enums;
  std::unordered_map<std::string, FunctionDecl *> functions;
  std::unordered_map<std::string, VarInfo> globals;
  std::vector<std::unordered_map<std::string, VarInfo>> scopes;
  FunctionDecl *currentFunction = nullptr;

  // How many loops/switches CheckStmt is currently nested inside,
  // textually - what validates `break`/`continue` (break: loopDepth > 0
  // || switchDepth > 0; continue: loopDepth > 0 - a `continue` only ever
  // means "the nearest enclosing LOOP", so it doesn't count switches at
  // all, matching real JS: `continue` inside a `switch` inside a `while`
  // continues the while, and a bare `continue` directly inside a switch
  // with no enclosing loop is still an error). Both reset to 0 (saved and
  // restored, same pattern currentFunction already uses) at every
  // function/method/closure body boundary - see CheckFunctionBody and the
  // three other body-checking sites (generic function/method
  // instantiation, FunctionExpr) - because break/continue can never jump
  // out of a nested function even if it's lexically inside a loop textually,
  // same as real JS.
  int loopDepth = 0;
  int switchDepth = 0;
  // How many `finally` blocks CheckStmt is currently nested inside,
  // textually (a count, not a bool, since a try/catch/finally can nest
  // inside another try's own finally) - `return`/`throw` are rejected
  // unconditionally whenever this is nonzero (see CheckStmt's own Return/
  // Throw cases): both inherently escape the ENTIRE finally, unlike
  // `break`/`continue`, which stay legal as long as they target a loop/
  // switch that's itself inside the same finally - see CheckStmt's own
  // Try case, which resets loopDepth/switchDepth to 0 around checking a
  // finallyBody (the same boundary a function body's own gets) to make
  // that distinction fall out for free, with no separate tracking of its
  // own. Real TS/JS instead let return/throw/break/continue inside a
  // `finally` silently override whatever the try/catch was already doing
  // (swallowing an in-flight exception or return value) - a widely-
  // flagged footgun (see e.g. ESLint's `no-unsafe-finally` rule) ART
  // deliberately doesn't reproduce: rejecting these outright, rather than
  // giving them real-but-confusing override semantics, keeps a `finally`
  // block simple to reason about (it always runs to completion, then
  // whatever was already happening continues) at the cost of a real, rare
  // pattern (an early return specifically FROM a finally) not compiling.
  int finallyDepth = 0;

  // Names currently known non-null within an `if (x != null) { ... }`'s
  // own `then` body, or in the code following an `if (x == null) {
  // <always exits> }` with no `else` (see CheckStmt's If case and
  // TryGetNullCheckedVar) - checked by CheckExpr's own Identifier case,
  // which resolves a member of this set (whose OWN declared type is
  // Nullable(T)) as plain T instead, marking Expr::isNarrowedNonNull so
  // Codegen knows to unbox it. Deliberately just a flat set of names,
  // not itself scope-aware - every insertion is always paired with a
  // removal in the same CheckStmt call that added it (or, for the
  // "narrows the rest of the block" case, by the owning Block's own
  // loop - see pendingNarrowAfterStmt), so it never needs its own
  // separate save/restore machinery the way `scopes` does.
  std::unordered_set<std::string> narrowedNonNull;
  // Set by CheckStmt's own If case when `if (x == null) { <always
  // exits> }` has no `else` - the enclosing Block's own statement loop
  // reads this immediately after checking that If and, if non-empty,
  // narrows that name for its own remaining statements (un-narrowing it
  // again once the block itself ends). Empty means "no pending narrow" -
  // always cleared by whoever reads it, never left set across a
  // statement that didn't just produce one.
  std::string pendingNarrowAfterStmt;
  // True iff `cond` is exactly `identifier != null`/`identifier ==
  // null` (either operand order) AND that identifier's own declared
  // type is actually Nullable(T) - the ONE condition shape this narrows
  // at all (see art/README.md's own "Nullable types" section for why
  // deliberately just this: no `&&`-chained conditions, no arbitrary
  // expressions, no `!x`-style truthiness). Fills `outName`/
  // `outEqualsNull` on success.
  bool TryGetNullCheckedVar(Expr *cond, std::string &outName, bool &outEqualsNull);
  // Like AlwaysReturns, but answers a different question: does control
  // ever fall through PAST this statement at all, regardless of whether
  // it produces a return value - true for Return/Throw (AlwaysReturns
  // already covers these) but ALSO Break/Continue, which AlwaysReturns
  // deliberately does NOT count (a break doesn't return a value, but it
  // just as surely means "nothing after this point in the current block
  // runs"). This is what decides whether an `if (x == null) { ... }`
  // with no `else` safely narrows `x` for the rest of the block - see
  // CheckStmt's own If case.
  bool AlwaysExits(Stmt *stmt);

  // The `any`-flavored counterpart to narrowedNonNull/pendingNarrowAfterStmt
  // above, for `typeof x === "..."`/`typeof x !== "..."` instead of `x ==
  // null`/`x != null` - kept as a genuinely separate, parallel mechanism
  // rather than generalizing the null-narrowing one to cover both, so
  // this addition can't regress the already-shipped, already-tested
  // nullable narrowing. Maps a currently-narrowed name to WHICH concrete
  // AnyTag it's narrowed to (there's more than one possible target here,
  // unlike Nullable(T)'s single "the wrapped T"), read by CheckExpr's own
  // Identifier case to resolve as that concrete type instead of `any`,
  // marking Expr::isNarrowedAny. Same flat-set-of-names, always-paired-
  // insert/remove discipline narrowedNonNull already documents.
  std::unordered_map<std::string, AnyTag> narrowedAny;
  // Parallel to pendingNarrowAfterStmt, for the `typeof x === "..."`
  // early-exit shape (`if (typeof x !== "string") { <always exits> }`
  // with no `else`) - the enclosing Block's own loop reads BOTH this and
  // pendingNarrowAfterStmt after each statement (a single statement can
  // only ever set one of the two, never both). Empty name means "no
  // pending narrow", same convention.
  std::string pendingAnyNarrowAfterStmt;
  AnyTag pendingAnyNarrowTag = AnyTag::Number; // meaningless unless pendingAnyNarrowAfterStmt is non-empty
  // True iff `cond` is exactly `typeof identifier === "tag"`/`typeof
  // identifier !== "tag"` (either operand order, and ART's `==`/`!=` -
  // see art/README.md's own "Dynamic typing" section for why there's no
  // separate `===`) where `identifier`'s own declared type is Any AND
  // "tag" is a string literal spelling one of "number"/"boolean"/
  // "string"/"object"/"function" - the ONE condition shape this
  // recognizes, same deliberately-narrow scope TryGetNullCheckedVar's own
  // doc comment explains for its own case. Fills `outName`/`outTag`/
  // `outEqualsTag` on success; "object"/"function" are recognized as
  // valid `typeof` results (the comparison itself always type-checks) but
  // never narrow anything (see TypeTag::Any's own doc comment for why) -
  // this returns false for those, same as if the condition shape didn't
  // match at all.
  bool TryGetTypeofCheckedVar(Expr *cond, std::string &outName, AnyTag &outTag, bool &outEqualsTag);
  // True iff `t` is one of the tags a concrete value can be implicitly
  // boxed into `any` FROM (see TypeTag::Any's own doc comment) - Void,
  // Unknown, Nullable, and Any itself all excluded (Any needs no boxing
  // at all; the other three are deliberate non-goals for v1, each with
  // its own reason documented there). Shared by the boxing decision in
  // CheckExpr's own tail check and GenBoxAny's own source-type switch, so
  // the two can never disagree about what's boxable.
  static bool IsAnyBoxable(TypeTag t);

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
  // Same deal as `builtins` above, for a builtin *interface* - right now
  // just `Error` (see SeedBuiltins), the one throwable/catchable type
  // `try`/`catch`/`throw` recognize (see StmtKind::Try's own doc
  // comment). Never added to Program::interfaces; owned here only so
  // `interfaces["Error"]` has somewhere valid to point.
  std::vector<std::unique_ptr<InterfaceDecl>> builtinInterfaces;
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
  void RegisterFunctionSignature(FunctionDecl *fn, bool allowRestParam = false);
  void CheckFunctionBody(FunctionDecl *decl);
  void CheckStmt(Stmt *stmt);
  ResolvedType CheckExpr(Expr *expr, const ResolvedType *expected);
  // True iff a value of type `from` can be used wherever `to` is
  // expected - either they're the exact same type (ResolvedType's own
  // operator==), or both are Struct and `from`'s own class is `to`'s
  // class or (transitively, via baseClass) a subclass of it - real
  // upcasting, the one new kind of assignability inheritance adds.
  // Deliberately NOT recursive into Array/Handler element types (a
  // Dog[] is NOT assignable to an Animal[] here, even though a bare Dog
  // is to a bare Animal) - real covariant collections are a real gap,
  // scoped out for now the same way every other inheritance limit is.
  bool IsAssignable(const ResolvedType &from, const ResolvedType &to);

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
  // A static field/method's own namespace - deliberately separate from
  // the instance one MangleMethod/MangleGetter/MangleSetter mangle into,
  // so `ClassName.foo` (static) and `instance.foo` (a field/method/
  // accessor) can share a bare name with no collision, matching real
  // TS/JS's separate static/instance namespaces.
  static std::string MangleStatic(const std::string &className, const std::string &propName);
  static std::string MangleEnumMember(const std::string &enumName, const std::string &memberName);
  // True iff `name` is a class Sema knows about with no local/global
  // *variable* of the same name shadowing it - the exact condition that
  // makes `name.member`/`name.member(...)` a static access
  // (`ClassName.foo`) rather than an ordinary instance one on some
  // variable (`obj.foo`). Checked wherever a Member/Call's own `lhs` is a
  // bare Identifier - see CheckExpr's Member and Call cases, and
  // CheckLValueTarget's Member case (for `ClassName.foo = value`).
  bool IsStaticAccessTarget(const std::string &name);

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
