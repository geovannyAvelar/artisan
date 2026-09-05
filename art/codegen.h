#ifndef ART_CODEGEN_H
#define ART_CODEGEN_H

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "ast.h"
#include "sema.h"

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Target/TargetMachine.h"

namespace ART {

// Lowers a type-checked Program (Sema must have already run successfully)
// into an LLVM module for a given target triple.
//
// Value representation:
//   number                -> LLVM double
//   boolean                -> LLVM i1
//   T[]                    -> ptr to a shared { i64 length, ptr data } header,
//                             both header and data buffer malloc'd on the heap
//   an interface type       -> ptr to a per-interface named struct, GC-allocated
// Every heap allocation (see GenHeapAlloc) goes through the Boehm-Demers-
// Weiser conservative garbage collector's GC_malloc instead of libc's own
// malloc - a compiled ART program never explicitly frees anything, but
// unreachable allocations do get reclaimed (conservatively: the collector
// scans the native stack/registers/globals for anything that looks like a
// pointer into its own heap, so it can rarely retain a little garbage,
// but never frees something still reachable). See GenGCInit's own doc
// comment for how/when the collector itself gets initialized. Array/
// struct pointers are opaque (LLVM 18 default); the pointee type is
// always recovered from the corresponding Expr's Sema-resolved ResolvedType
// rather than carried on the LLVM Value itself.
class Codegen {
public:
  Codegen(std::string moduleName, std::string targetTriple, const Sema &sema);

  // Requires Sema::Check(program) to have already returned true.
  // Also exposes the TargetMachine it built for `targetTriple`, so the
  // driver can reuse it to emit an object file without re-creating one.
  std::unique_ptr<llvm::Module> Generate(Program &program);
  llvm::TargetMachine *GetTargetMachine() const { return targetMachine.get(); }

private:
  struct VarBinding {
    llvm::Value *alloca; // an AllocaInst for a local, or a GlobalVariable for a top-level let/const
    llvm::Type *type;    // the variable's own value type - never `ptr`-to-cell; that indirection is
                          // implicit via isBoxed (see LoadVar/GenLValue's Identifier branch)
    // True iff Sema marked this variable's own declaration site (Param/
    // Stmt) isCapturedByClosure - see that field's own doc comment. When
    // true, `alloca` doesn't hold the value directly: it holds a `ptr`
    // to a separate, GC-allocated cell that actually holds the value
    // (see GenStmt's VarDecl case and DeclareParamBinding for how the
    // cell is created, and LoadVar/GenLValue for the one extra
    // indirection every read/write goes through). This is what makes
    // capture-by-reference work: a closure's env holds a copy of the
    // same cell's address, not a copy of the value.
    bool isBoxed = false;
  };

  struct LValue {
    llvm::Value *addr;
    llvm::Type *storeType;
    bool isBoolArrayElem; // element lives packed as i8 in an array data buffer
  };

  std::string moduleName;
  std::string targetTriple;
  const Sema &sema;

  llvm::LLVMContext context;
  std::unique_ptr<llvm::Module> module;
  llvm::IRBuilder<> builder;
  std::unique_ptr<llvm::TargetMachine> targetMachine;

  llvm::FunctionCallee gcMallocFn; // Boehm GC's GC_malloc - see GenHeapAlloc
  llvm::FunctionCallee memcmpFn;
  // libc's own setjmp/longjmp, called directly (never through a wrapper
  // function - see GenStmt's own Try/Throw cases for why that matters:
  // setjmp only captures a resumable state for the function that calls
  // it directly). What try/catch/throw is actually built on - see
  // art/README's own note on why (ART has no destructors at all,
  // everything being GC'd, so the usual reason C++-style unwinding needs
  // to be this complicated - running cleanup code for every stack frame
  // being unwound past - simply doesn't apply here).
  llvm::FunctionCallee setjmpFn;
  llvm::FunctionCallee longjmpFn;
  llvm::FunctionCallee abortFn; // an uncaught throw (no active handler at all) - see GenStmt's own Throw case
  // `@art.exception.currentHandler` - the single global root of the
  // handler-frame linked list (see GetExceptionFrameType), null when no
  // try is currently active anywhere. Cached here once Generate() creates
  // it so GenStmt's Try/Throw cases don't need to re-look-it-up by name
  // every time.
  llvm::GlobalVariable *exceptionCurrentHandler = nullptr;
  llvm::StructType *exceptionFrameType = nullptr; // see GetExceptionFrameType
  llvm::StructType *GetExceptionFrameType();
  llvm::StructType *arrayHeaderType = nullptr; // also used for strings: same { i64 length, ptr data } shape
  // `{ i32 tag, ptr payload }` - every `any` value's own box (see
  // TypeTag::Any's own doc comment). Named (like arrayHeaderType/
  // exceptionFrameType, unlike GetHandlerStructType's anonymous one)
  // since GenBoxAny/the Unary "typeof" case both need to CreateStructGEP
  // into it from otherwise-unrelated call sites, all needing the exact
  // same layout.
  llvm::StructType *anyBoxType = nullptr;
  llvm::StructType *GetAnyBoxType();
  // Builds a fresh `any` box (see GetAnyBoxType) wrapping `rawValue`,
  // already-evaluated as `sourceType` (one of the tags Sema::IsAnyBoxable
  // accepts) - the ONE place that decides both the runtime AnyTag and how
  // to actually store the value (a heap cell for Number/Boolean/Enum/
  // Handler, which have no spare pointer-shaped slot of their own to
  // reuse; `rawValue` itself for String/Struct/Array, already `ptr`-
  // shaped). Called from GenExpr's own thin wrapper below whenever
  // Sema set Expr::needsAnyBox - see its own doc comment in ast.h.
  llvm::Value *GenBoxAny(llvm::Value *rawValue, const ResolvedType &sourceType);
  std::unordered_map<std::string, llvm::StructType *> structTypes;
  std::unordered_map<std::string, llvm::Function *> llvmFunctions;
  std::unordered_map<std::string, VarBinding> globalVars;
  // Lazily generated, cached per plain-function name - see
  // GetOrCreatePlainThunk's own doc comment.
  std::unordered_map<std::string, llvm::Function *> plainHandlerThunks;

  std::vector<std::unordered_map<std::string, VarBinding>> scopes;
  llvm::Function *currentFunction = nullptr;

  // One entry per loop/switch/try GenStmt is currently generating,
  // innermost last (a real LIFO stack, matching lexical nesting order) -
  // what `break`/`continue`/`return` actually have to unwind through on
  // their way out. Two unrelated things share this one stack (rather
  // than two separate ones) specifically because they have to be walked
  // TOGETHER, in real nesting order: a `break` three `try`s deep inside
  // a loop has to pop all three of those try-handler frames (see
  // GenExceptionHandlerCleanup) on its way to the loop's own break
  // target, and the only way to know exactly which trys sit between
  // "here" and "there" is one shared, correctly-ordered stack.
  struct ScopeExit {
    enum class Kind { Loop, Try } kind;

    // Loop (also used by Switch, which is break-able like a loop but not
    // continue-able - see LoopTargets' own old doc comment, preserved
    // here): breakTarget is always set; continueTarget is null for a
    // switch frame, since `continue` skips right past any enclosing
    // switch to the nearest enclosing LOOP (matching real JS) - see
    // GenStmt's own Continue case, which walks this stack for the first
    // Loop-kind frame that actually has one, rather than always using
    // the top frame the way Break does. Sema (loopDepth/switchDepth)
    // already guarantees a Break/Continue node here always finds a
    // valid frame - this stack exists purely to know which LLVM
    // BasicBlock/handler state to unwind to, not to re-validate what
    // Sema already proved.
    llvm::BasicBlock *breakTarget = nullptr;
    llvm::BasicBlock *continueTarget = nullptr;

    // Try: a pointer to this try's own handler-frame struct (see
    // GetExceptionFrameType) - the same frame StmtKind::Throw's own
    // codegen reads a "prev" field out of, at a completely different,
    // unrelated call site, to restore `@art.exception.currentHandler`
    // when it fires (throw has no compile-time visibility into any
    // particular try's own SSA values - only the runtime linked list
    // rooted at that global - so it always goes through memory; early
    // exit reuses that exact same "read this frame's own prev field"
    // mechanism rather than a second, parallel one). A break/continue/
    // return skipping past N nested try frames needs only the OUTERMOST
    // (earliest-pushed) of those N frames' own prev field, loaded once -
    // see GenExceptionHandlerCleanup.
    llvm::Value *framePtr = nullptr;

    // Try, only when this frame's own try/catch/finally statement has a
    // `finally` clause - the AST Stmt whose `statements` are that clause's
    // body (see Stmt::finallyBody). Null for a Try frame with no finally,
    // and for every Loop frame. Read by GenExceptionHandlerCleanup, which
    // (re-)generates this exact Stmt's own code (via an ordinary GenStmt
    // call - safe to do more than once, since Sema guarantees a finally
    // body can never itself jump anywhere but straight through - see
    // Stmt::finallyBody's own doc comment) at every early-exit call site
    // that crosses this frame, in addition to the try's own "normal
    // completion" and "an exception was caught here" call sites (see
    // GenStmt's own Try case) - one of potentially several places the
    // SAME source-level finally block's code gets emitted, each a
    // genuinely different control-flow edge at runtime, never more than
    // one of which actually executes on any given dynamic pass through
    // this try statement.
    Stmt *finallyBody = nullptr;
  };
  std::vector<ScopeExit> scopeStack;

  // Restores `@art.exception.currentHandler` to what it was before every
  // Try-kind ScopeExit frame at or above `stopBelow` on `scopeStack` was
  // pushed - i.e. "pop every try handler frame between here and (but not
  // including) `stopBelow`" - AND, before doing so, runs each crossed
  // frame's own `finally` body (if it has one - see ScopeExit::
  // finallyBody), innermost first (real nesting order - the closest
  // cleanup runs first, matching real TS/JS). Called before any jump that
  // leaves one or more try bodies early - Break/Continue (stopBelow: the
  // target loop/switch's own stack index) and Return (stopBelow: 0, the
  // whole current function). A no-op (no store emitted, no finally run)
  // if no Try frame is actually being skipped - skipping straight to a
  // target with no try in between costs nothing. The handler-chain
  // restoration itself is the one piece of bookkeeping that makes
  // try/catch safe to jump out of early at all: without it, a LATER,
  // completely unrelated `throw` elsewhere could `longjmp` into a stack
  // frame that's already been reused by something else - real memory
  // corruption, not a cosmetic bug.
  void GenExceptionHandlerCleanup(size_t stopBelow);
  // Shared tail of both StmtKind::Throw's own codegen and every "this
  // try/catch has no handler of its own for what just arrived - keep
  // propagating it" path a `finally` needs (see GenTryFinallyOnly/
  // GenTryWithFinallyAndCatch): loads `@art.exception.currentHandler`
  // (already correctly pointing at the next-OUTER handler by the time any
  // of these call this - see StmtKind::Throw's own "pop BEFORE jumping"
  // comment), and either longjmps `errorVal` there or, if there is none,
  // aborts (an uncaught exception). `errorVal` is always an Error* (ptr).
  void GenRethrow(llvm::Value *errorVal);
  // The exact try/catch mechanism from before `finally` existed -
  // requires a catch clause (stmt->declaredType/elseBranch), no finally
  // awareness of its own. Used directly for a `try`/`catch` with no
  // `finally`, and reused UNMODIFIED as the inner, protected construct
  // inside GenTryWithFinallyAndCatch when both are present.
  void GenTryCatchOnly(Stmt *stmt);
  // `try { ... } finally { ... }` with NO catch clause - one handler
  // frame is enough (unlike GenTryWithFinallyAndCatch's two): there's no
  // separate catch body of its own that could itself throw and need
  // further protection, so this frame's own "something was thrown" path
  // just runs the finally body directly, then re-propagates via
  // GenRethrow, rather than declaring a catch variable and running a
  // catch body the way GenTryCatchOnly's own does.
  void GenTryFinallyOnly(Stmt *stmt);
  // `try { ... } catch (...) { ... } finally { ... }` - both clauses
  // present. Needs TWO real, separate handler frames: an OUTER one whose
  // only job is guaranteeing the finally body runs (and whatever's in
  // flight keeps propagating) even if the CATCH body itself throws -
  // something a single frame can't do, since GenTryCatchOnly's own catch
  // path already pops ITS OWN frame before the catch body ever runs (a
  // catch block's own errors must never be caught by the same catch -
  // see StmtKind::Throw's own "pop BEFORE jumping" comment) - wrapping an
  // INNER frame that's the exact unmodified GenTryCatchOnly construct,
  // nested one level inside the outer's own protected region.
  void GenTryWithFinallyAndCatch(Stmt *stmt);

  void SetupTarget();
  llvm::Type *MapType(const ResolvedType &t);
  llvm::Type *ArrayElemStorageType(const ResolvedType &elem);
  llvm::StructType *GetArrayHeaderType();
  llvm::StructType *GetOrCreateStructType(const std::string &ifaceName);
  // `fieldName`'s own index into `iface->fields` - see
  // StructFieldGepIndex for the (possibly different) index its actual
  // compiled struct layout has, which is what a real CreateStructGEP
  // call needs.
  int FieldIndex(InterfaceDecl *iface, const std::string &fieldName);
  int StructFieldGepIndex(InterfaceDecl *iface, int fieldIndex);

  // One cached LLVM global array per class with hasVirtualDispatch - the
  // class's own vtable, one function pointer per virtual method slot
  // (see FunctionDecl::isVirtual/vtableSlot). Built once per class,
  // lazily, the first time anything needs it (a method call through a
  // polymorphic receiver, or constructing an instance of that exact
  // class) - see GetOrCreateVtable's own doc comment for exactly how a
  // slot's current contents are resolved.
  std::unordered_map<std::string, llvm::GlobalVariable *> vtables;
  llvm::GlobalVariable *GetOrCreateVtable(InterfaceDecl *iface);
  // For a class with hasVirtualDispatch, the ordered list of whichever
  // FunctionDecl currently implements each vtable slot for THIS class
  // specifically (its own override if it has one, otherwise whichever
  // ancestor's version is nearest) - built fresh from the already-
  // resolved AST (FunctionDecl::isVirtual/vtableSlot, InterfaceDecl::
  // baseClass), not cached, since it's only ever needed once per class
  // (from GetOrCreateVtable) to build that class's own vtable constant.
  std::vector<FunctionDecl *> BuildVtableLayout(InterfaceDecl *iface);

  // Emits a global constructor (via llvm::appendToGlobalCtors) that calls
  // Boehm GC's GC_init() at module-load time, before any other code in
  // this module (including a `main` wrapper, or, when this module is
  // linked into the larger artisan runtime instead of standing alone, the
  // host binary's own `main`) can possibly run - so GC_malloc is always
  // safe to call the moment ART code starts executing, in either build
  // shape, with no per-entry-point call site of its own to remember.
  // GC_init is safe to call more than once (a no-op after the first), so
  // this needs no coordination with anything else that might also
  // initialize the collector.
  void GenGCInit();
  void DeclareFunctionSignatures(const std::vector<FunctionDecl *> &decls, bool allowMainRename);
  void GenGlobalDecl(Stmt *stmt); // a top-level `let`/`const` - see Program::globals
  // Companion to GenGlobalDecl: every global whose initializer isn't a
  // bare number/boolean/string literal (GenGlobalDecl already handles
  // those as real compile-time llvm::Constants) needs its initializer
  // expression actually *run* to get a value - a call, an object/array
  // literal, another global, a bare function reference, ... none of
  // which are compile-time constants. Emits one more module constructor
  // (see GenGCInit's own doc comment for the mechanism), at a lower
  // priority so it always runs after GC_init (an initializer that
  // allocates needs GC_malloc already safe to call), evaluating each
  // such global's initializer via the ordinary GenExpr machinery and
  // storing the result - in declaration order, so an initializer may
  // read an *earlier* global's already-stored value but not a *later*
  // one's (see Sema::CheckGlobalDecl's own doc comment). Must run after
  // DeclareFunctionSignatures (an initializer calling a function only
  // needs its signature declared, not yet defined) but doesn't itself
  // need any function's body generated first.
  void GenGlobalInit(std::vector<std::unique_ptr<Stmt>> &globals);
  // Emits a real, externally-visible function literally named
  // "setupApp" (`void setupApp(void)`, exactly the symbol main.cpp's
  // generated_setup_app_stub.cpp already calls once per page load) whose
  // body runs `stmts` in order - see Program::topLevelStmts' own doc
  // comment for why this exists as a second, separate mechanism from
  // GenGlobalInit above rather than one unified thing: a global's
  // initializer runs once, ever, at process start; this runs every time
  // setupApp itself is called. Only ever invoked when nothing already
  // declared this exact symbol (see its own call site in Generate()) -
  // an explicit `function setupApp()` the project wrote itself takes
  // priority and this is skipped entirely (Sema::Check guarantees
  // `stmts` is empty whenever that's the case); with neither, `stmts` is
  // simply empty and this still emits a real, empty `setupApp` - the
  // trampoline calls the symbol unconditionally, so it always has to
  // exist.
  void GenSetupAppBody(std::vector<std::unique_ptr<Stmt>> &stmts);
  void GenBuiltinNumberToString(); // defines the LLVM function backing Sema::SeedBuiltins' "numberToString"
  void GenBuiltinStringToNumber(); // defines the LLVM function backing Sema::SeedBuiltins' "stringToNumber"
  // Defines one instantiation's LLVM function backing Sema::SeedBuiltins'
  // "makeArray" generic template - called once per distinct T actually
  // used (see Generate()'s own split of sema.Instantiations() into
  // "makeArray$..." vs everything else), since the per-element storage
  // size/shape (see ArrayElemStorageType) depends on T. Mirrors
  // ExprKind::ArrayLiteral's own codegen almost exactly - same header
  // shape, same bool-packed-as-i8 element storage - just with a real
  // loop instead of one unrolled store per element, since `size` here is
  // a runtime value, not a compile-time-known element count.
  void GenBuiltinMakeArray(FunctionDecl *inst);
  // Defines one instantiation's LLVM function backing Sema::SeedBuiltins'
  // "notNull" generic template - called once per distinct T actually
  // used, same "split out of `instantiations`, hand-generated directly"
  // deal GenBuiltinMakeArray already has. Boxes its own argument into a
  // fresh GC cell sized to MapType(T) and returns that pointer.
  void GenBuiltinNotNull(FunctionDecl *inst);
  void GenFunction(FunctionDecl *decl);
  // Binds one parameter of `fn` to `argVal` - shared by GenFunction and
  // GenClosureFunction. Boxes into a fresh GC cell (see VarBinding's own
  // doc comment) when `p.isCapturedByClosure`, otherwise an ordinary
  // stack alloca, exactly the same split GenStmt's VarDecl case makes
  // for a captured vs. plain local.
  void DeclareParamBinding(llvm::Function *fn, const Param &p, llvm::Value *argVal);

  // Every closure literal Sema found (sema.Closures()) gets its own LLVM
  // function declared here, ahead of any function body generation (a
  // closure can be referenced before its own textual position - e.g.
  // stored in a global) - mirrors DeclareFunctionSignatures, but every
  // closure's signature is the uniform thunk shape "(ptr env,
  // ...params) -> void" (see GenClosureFunction's own doc comment), not
  // derived from MapType the normal way.
  void DeclareClosureThunkSignatures(const std::vector<FunctionDecl *> &closureFns);
  // Generates one closure's own thunk body - see FunctionDecl::captures'
  // doc comment and DeclareClosureThunkSignatures above for the
  // signature this fills in. Mirrors GenFunction almost exactly, except
  // there's an extra env-unpack prologue before the ordinary param loop,
  // and the tail is always a plain "ret void" (a closure is always
  // void - see Sema::CheckExpr's FunctionExpr case).
  void GenClosureFunction(FunctionDecl *fn);
  // Returns the Handler struct type - see MapType's own Handler case,
  // the single choke point that makes this apply everywhere a Handler-
  // typed value flows (locals, params, returns, array/struct fields).
  llvm::StructType *GetHandlerStructType();
  // Returns the "(ptr env, ...) -> void"-shaped thunk for `fnName` - a
  // plain top-level function used as a first-class Handler *value* (not
  // directly called by name) still needs one, purely so every Handler
  // value - closure or plain-function-reference alike - is callable
  // through the exact same uniform calling convention at every indirect-
  // call/declare-function-call site. Lazily generated and cached in
  // `plainHandlerThunks`, since most functions are only ever called
  // directly and never need one at all.
  llvm::Function *GetOrCreatePlainThunk(const std::string &fnName);
  // Builds the {thunk, null} Handler aggregate for a bare reference to
  // `fnName` used as a value - see GetOrCreatePlainThunk above.
  llvm::Value *BuildPlainFunctionHandlerValue(const std::string &fnName);
  // Finds the already-checked FunctionDecl `name` names, if any -
  // sema.Functions() for an ordinary function/method, sema.Instantiations()
  // for a generic one. Used only to read `isExtern` at a call site (see
  // AppendCallArg below) - returns null for anything that isn't looked
  // up by name at all (a closure, an indirect call through a Handler
  // value), which never needs this.
  FunctionDecl *LookupCalleeDecl(const std::string &name);
  // Appends one already-evaluated call argument `val` to `args` - plain
  // append, UNLESS `unpackHandler` is set and `ty` is Handler, in which
  // case the 2-word {fn, env} aggregate is unpacked into two separate
  // native arguments instead of one. `unpackHandler` is true only for a
  // call into an `isExtern` (declare function) native symbol: the real
  // C function on the other side (see include/art_bridge.h) takes `fn`/
  // `env` as two separate parameters, not an LLVM struct value - see
  // DeclareFunctionSignatures' matching signature-side rule below, and
  // FunctionDecl::isExtern's own doc comment for why this can't just be
  // `body == nullptr`.
  void AppendCallArg(std::vector<llvm::Value *> &args, llvm::Value *val, const ResolvedType &ty,
                      bool unpackHandler);

  void GenStmt(Stmt *stmt);
  // Thin wrapper around GenExprInner: every OTHER call site in this file
  // calls this, never GenExprInner directly, so `any`-boxing (see
  // Expr::needsAnyBox's own doc comment) is applied in exactly ONE place
  // regardless of which of GenExprInner's many per-ExprKind cases
  // actually produced the value - the same "one choke point, not
  // threaded through every case" shape isNarrowedNonNull's own unboxing
  // already has, just on the opposite (boxing, not unboxing) side.
  llvm::Value *GenExpr(Expr *expr);
  // The actual, unchanged-in-shape big per-ExprKind switch GenExpr used
  // to be before `any` existed - never call this directly (see GenExpr's
  // own doc comment above for why).
  llvm::Value *GenExprInner(Expr *expr);
  LValue GenLValue(Expr *expr);

  llvm::Constant *BuildStringConstant(const std::string &value); // the header global's address
  llvm::Constant *BuildCStringConstant(const std::string &value); // a raw null-terminated `ptr`, not an ART string
  llvm::Value *GenStringLiteral(const std::string &value);
  llvm::Value *GenStringConcat(llvm::Value *lhsPtr, llvm::Value *rhsPtr);
  llvm::Value *GenStringEquals(llvm::Value *lhsPtr, llvm::Value *rhsPtr); // returns i1 "bytes equal"
  // The exact "==" comparison logic ExprKind::Binary's own "==" case
  // uses (String -> GenStringEquals; Handler -> field-wise fn/env
  // identity; Number -> FCmpOEQ; everything else, e.g. Struct/Array
  // pointers -> ICmpEQ) - factored out here so StmtKind::Switch's own
  // case-value comparisons (see GenStmt) reuse it exactly rather than
  // duplicating (and risking drifting from) Binary's own rules.
  llvm::Value *GenEqualityCheck(llvm::Value *lhsVal, llvm::Value *rhsVal, TypeTag tag);
  llvm::Value *GenStringIndex(llvm::Value *strPtr, llvm::Value *idxVal);
  llvm::Value *GenHeapAlloc(uint64_t bytes);
  llvm::Value *GenHeapAlloc(llvm::Value *bytes);

  llvm::AllocaInst *CreateEntryAlloca(llvm::Function *fn, llvm::Type *type, const std::string &name);
  void PushScope();
  void PopScope();
  VarBinding *Lookup(const std::string &name);
  // Same lookup as Lookup(), but returns null instead of throwing when
  // `name` isn't a variable at all - used to disambiguate a Handler-typed
  // Identifier (see GenExpr's own case), which is either a real local/
  // global variable holding a Handler value, or a bare reference to a
  // top-level function used as a value (a plain code address, found in
  // llvmFunctions instead - see Sema::CheckExpr's Identifier case for
  // why both produce the same TypeTag::Handler resolvedType).
  VarBinding *TryLookup(const std::string &name);
  void Declare(const std::string &name, llvm::Value *alloca, llvm::Type *type, bool isBoxed = false);
  // Reads a variable's current value - a plain load for an ordinary
  // binding, or one extra indirection (load the cell pointer out of
  // `b->alloca`, then load the value out of the cell) for a boxed one -
  // see VarBinding::isBoxed's own doc comment. Used by both GenExpr's
  // Identifier case and (indirectly, via LValue's boxed-aware address)
  // GenLValue's own Identifier branch.
  llvm::Value *LoadVar(VarBinding *b, const std::string &name);
};

} // namespace ART

#endif
