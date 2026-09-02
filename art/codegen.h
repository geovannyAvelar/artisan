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
    llvm::Type *type;
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
  llvm::StructType *arrayHeaderType = nullptr; // also used for strings: same { i64 length, ptr data } shape
  std::unordered_map<std::string, llvm::StructType *> structTypes;
  std::unordered_map<std::string, llvm::Function *> llvmFunctions;
  std::unordered_map<std::string, VarBinding> globalVars;

  std::vector<std::unordered_map<std::string, VarBinding>> scopes;
  llvm::Function *currentFunction = nullptr;

  void SetupTarget();
  llvm::Type *MapType(const ResolvedType &t);
  llvm::Type *ArrayElemStorageType(const ResolvedType &elem);
  llvm::StructType *GetArrayHeaderType();
  llvm::StructType *GetOrCreateStructType(const std::string &ifaceName);
  int FieldIndex(InterfaceDecl *iface, const std::string &fieldName);

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
  void GenBuiltinNumberToString(); // defines the LLVM function backing Sema::SeedBuiltins' "numberToString"
  void GenFunction(FunctionDecl *decl);

  void GenStmt(Stmt *stmt);
  llvm::Value *GenExpr(Expr *expr);
  LValue GenLValue(Expr *expr);

  llvm::Constant *BuildStringConstant(const std::string &value); // the header global's address
  llvm::Constant *BuildCStringConstant(const std::string &value); // a raw null-terminated `ptr`, not an ART string
  llvm::Value *GenStringLiteral(const std::string &value);
  llvm::Value *GenStringConcat(llvm::Value *lhsPtr, llvm::Value *rhsPtr);
  llvm::Value *GenStringEquals(llvm::Value *lhsPtr, llvm::Value *rhsPtr); // returns i1 "bytes equal"
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
  void Declare(const std::string &name, llvm::AllocaInst *alloca, llvm::Type *type);
};

} // namespace ART

#endif
