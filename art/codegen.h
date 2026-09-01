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
//   an interface type       -> ptr to a malloc'd, per-interface named struct
// There is no garbage collector yet - heap allocations are never freed.
// Array/struct pointers are opaque (LLVM 18 default); the pointee type is
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

  llvm::FunctionCallee mallocFn;
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

  void DeclareFunctionSignatures(std::vector<std::unique_ptr<FunctionDecl>> &decls, bool allowMainRename);
  void GenGlobalDecl(Stmt *stmt); // a top-level `let`/`const` - see Program::globals
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
  void Declare(const std::string &name, llvm::AllocaInst *alloca, llvm::Type *type);
};

} // namespace ART

#endif
