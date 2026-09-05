#include "codegen.h"

#include <algorithm>
#include <stdexcept>

#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"

namespace ART {

Codegen::Codegen(std::string moduleName, std::string targetTriple, const Sema &sema)
    : moduleName(std::move(moduleName)), targetTriple(std::move(targetTriple)), sema(sema), builder(context) {}

void Codegen::SetupTarget() {
  std::string error;
  const llvm::Target *target = llvm::TargetRegistry::lookupTarget(targetTriple, error);
  if (!target) {
    throw std::runtime_error("unknown target triple '" + targetTriple + "': " + error);
  }
  llvm::TargetOptions options;
  targetMachine.reset(
      target->createTargetMachine(targetTriple, "generic", "", options, llvm::Reloc::PIC_));
  if (!targetMachine) {
    throw std::runtime_error("failed to create a target machine for '" + targetTriple + "'");
  }
  module->setDataLayout(targetMachine->createDataLayout());
  module->setTargetTriple(targetTriple);
}

// ---------------------------------------------------------------------
// Type mapping
// ---------------------------------------------------------------------

llvm::Type *Codegen::MapType(const ResolvedType &t) {
  switch (t.tag) {
  case TypeTag::Number:
    return llvm::Type::getDoubleTy(context);
  case TypeTag::Boolean:
    return llvm::Type::getInt1Ty(context);
  case TypeTag::String:
    return llvm::PointerType::get(context, 0);
  case TypeTag::Handler:
    return GetHandlerStructType();
  case TypeTag::Void:
    return llvm::Type::getVoidTy(context);
  case TypeTag::Array:
    return llvm::PointerType::get(context, 0);
  case TypeTag::Struct:
    return llvm::PointerType::get(context, 0);
  case TypeTag::Enum:
    // A real, distinct type from Number at the Sema level, but the exact
    // same runtime representation - see EnumDecl's own doc comment.
    return llvm::Type::getDoubleTy(context);
  case TypeTag::Nullable:
    // Always a boxed `ptr`, regardless of what the wrapped T's own
    // unwrapped representation would be - see TypeTag::Nullable's own
    // doc comment for why a uniform box (not "reuse T's own null-able
    // bit pattern, where it has one") is the right call here.
    return llvm::PointerType::get(context, 0);
  case TypeTag::Any:
    // Always a boxed `ptr` too - see TypeTag::Any's own doc comment and
    // GetAnyBoxType.
    return llvm::PointerType::get(context, 0);
  case TypeTag::Unknown:
    break;
  }
  throw std::runtime_error("codegen: unresolved type reached MapType - Sema should have rejected this program");
}

// A Handler value is a literal, anonymous 2-word {ptr fn, ptr env}
// struct, passed by value - not a pointer to a heap struct. `fn` always
// has the uniform "(ptr env, ...params) -> void" thunk signature (see
// GetOrCreatePlainThunk/GenClosureFunction), so it's callable the same
// way whether it's a plain function reference (env == null) or a real
// closure. Anonymous/literal (not `StructType::create`d) - LLVM
// auto-uniques structurally identical anonymous struct types itself, so
// there's nothing to cache here the way GetArrayHeaderType's NAMED
// struct needs.
llvm::StructType *Codegen::GetHandlerStructType() {
  llvm::Type *ptrTy = llvm::PointerType::get(context, 0);
  return llvm::StructType::get(context, {ptrTy, ptrTy});
}

llvm::Type *Codegen::ArrayElemStorageType(const ResolvedType &elem) {
  if (elem.tag == TypeTag::Boolean) return llvm::Type::getInt8Ty(context);
  return MapType(elem);
}

llvm::StructType *Codegen::GetAnyBoxType() {
  if (!anyBoxType) {
    anyBoxType = llvm::StructType::create(
        context, {llvm::Type::getInt32Ty(context), llvm::PointerType::get(context, 0)}, "art.any");
  }
  return anyBoxType;
}

// See this method's own doc comment in codegen.h.
llvm::Value *Codegen::GenBoxAny(llvm::Value *rawValue, const ResolvedType &sourceType) {
  llvm::Value *payload;
  AnyTag tag;
  switch (sourceType.tag) {
  case TypeTag::Number:
  case TypeTag::Enum: {
    // Same runtime representation either way (see MapType's own Enum
    // case) - `rawValue` is already a real `double` for both.
    tag = AnyTag::Number;
    llvm::Value *cell = GenHeapAlloc(8);
    builder.CreateStore(rawValue, cell);
    payload = cell;
    break;
  }
  case TypeTag::Boolean: {
    tag = AnyTag::Boolean;
    llvm::Value *cell = GenHeapAlloc(1);
    builder.CreateStore(builder.CreateZExt(rawValue, llvm::Type::getInt8Ty(context)), cell);
    payload = cell;
    break;
  }
  case TypeTag::String:
    tag = AnyTag::String;
    payload = rawValue; // already `ptr`-shaped - no extra box needed
    break;
  case TypeTag::Struct:
  case TypeTag::Array:
    // Indistinguishable via `typeof` (both report "object", exactly like
    // real JS - see AnyTag::Object's own doc comment), so one shared tag
    // is enough - neither is ever narrowed back out anyway.
    tag = AnyTag::Object;
    payload = rawValue; // already `ptr`-shaped - no extra box needed
    break;
  case TypeTag::Handler: {
    // Unlike everything else here, `rawValue` is a literal 2-word
    // {ptr fn, ptr env} AGGREGATE VALUE, not itself a pointer (see
    // GetHandlerStructType's own doc comment) - needs a real heap cell
    // to live in before anything can point at it.
    tag = AnyTag::Function;
    llvm::Value *cell = GenHeapAlloc(module->getDataLayout().getTypeAllocSize(GetHandlerStructType()).getFixedValue());
    builder.CreateStore(rawValue, cell);
    payload = cell;
    break;
  }
  default:
    throw std::runtime_error("codegen: unboxable type reached GenBoxAny - Sema::IsAnyBoxable should have rejected this");
  }

  llvm::StructType *boxTy = GetAnyBoxType();
  llvm::Value *box = GenHeapAlloc(module->getDataLayout().getTypeAllocSize(boxTy).getFixedValue());
  builder.CreateStore(llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), static_cast<uint64_t>(tag)),
                       builder.CreateStructGEP(boxTy, box, 0));
  builder.CreateStore(payload, builder.CreateStructGEP(boxTy, box, 1));
  return box;
}

llvm::StructType *Codegen::GetArrayHeaderType() {
  if (!arrayHeaderType) {
    arrayHeaderType = llvm::StructType::create(
        context, {llvm::Type::getInt64Ty(context), llvm::PointerType::get(context, 0)}, "art.array");
  }
  return arrayHeaderType;
}

// { [512 x i8] jmpbuf, ptr prev, ptr thrownValue } - one instance per
// dynamically-active `try`, stack-allocated in the ART function that owns
// it (see GenStmt's own Try case for why that's safe even across
// recursion/loop iterations). 512 bytes is comfortably larger than any
// real platform's own jmp_buf (macOS: 148 bytes; glibc: ~200) - a fixed,
// generously-oversized buffer rather than a per-platform probe, since
// getting this wrong would be silent stack corruption, not a compile
// error. `prev` links to the next-OUTER active frame (or null), forming
// the runtime stack `@art.exception.currentHandler` roots - both
// GenStmt's Throw case (an unrelated call site, possibly in a
// completely different function, with no compile-time visibility into
// any particular try's own SSA values) and GenExceptionHandlerCleanup
// (same function, early exit) read it the same way. `thrownValue` is an
// Error* - see StmtKind::Try's own doc comment for why only one
// throwable type exists right now.
llvm::StructType *Codegen::GetExceptionFrameType() {
  if (!exceptionFrameType) {
    llvm::Type *jmpBufTy = llvm::ArrayType::get(llvm::Type::getInt8Ty(context), 512);
    llvm::Type *ptrTy = llvm::PointerType::get(context, 0);
    exceptionFrameType = llvm::StructType::create(context, {jmpBufTy, ptrTy, ptrTy}, "art.exhandler");
  }
  return exceptionFrameType;
}

// See this method's own doc comment in codegen.h.
void Codegen::GenExceptionHandlerCleanup(size_t stopBelow) {
  // Run every crossed try's own finally body first - innermost to
  // outermost (scanning the stack top-down), real nesting order - BEFORE
  // touching the handler chain at all, so a nested try inside one of
  // these finally bodies still sees an accurate (if temporarily
  // redundant) `@art.exception.currentHandler` while it runs.
  for (size_t i = scopeStack.size(); i-- > stopBelow;) {
    if (scopeStack[i].kind == ScopeExit::Kind::Try && scopeStack[i].finallyBody) {
      GenStmt(scopeStack[i].finallyBody);
    }
  }

  llvm::Value *outermostTryFrame = nullptr;
  for (size_t i = stopBelow; i < scopeStack.size(); i++) {
    if (scopeStack[i].kind == ScopeExit::Kind::Try) {
      outermostTryFrame = scopeStack[i].framePtr;
      break; // the first Try found scanning up FROM stopBelow is the outermost one being skipped
    }
  }
  if (!outermostTryFrame) return; // no try frame in range - nothing to restore
  llvm::StructType *frameTy = GetExceptionFrameType();
  llvm::Value *prevFieldPtr = builder.CreateStructGEP(frameTy, outermostTryFrame, 1);
  llvm::Value *prev = builder.CreateLoad(llvm::PointerType::get(context, 0), prevFieldPtr);
  builder.CreateStore(prev, exceptionCurrentHandler);
}

llvm::StructType *Codegen::GetOrCreateStructType(const std::string &ifaceName) {
  auto it = structTypes.find(ifaceName);
  if (it != structTypes.end()) return it->second;

  InterfaceDecl *iface = sema.Interfaces().at(ifaceName);
  std::vector<llvm::Type *> fieldTypes;
  fieldTypes.reserve(iface->fields.size() + 1);
  // A class actually touched by inheritance gets a vtable pointer as
  // its own real field 0, ahead of every ART-visible field - see
  // FieldIndex's own matching +1 shift, and GenExpr's ObjectLiteral case
  // for where it's actually populated at construction time. A class
  // untouched by inheritance gets none of this: same layout as before
  // this feature existed at all.
  if (iface->hasVirtualDispatch) fieldTypes.push_back(llvm::PointerType::get(context, 0));
  for (auto &field : iface->fields) fieldTypes.push_back(MapType(field.resolvedType));

  auto *structTy = llvm::StructType::create(context, fieldTypes, "struct." + ifaceName);
  structTypes[ifaceName] = structTy;
  return structTy;
}

// The field's own index into `iface->fields` - NOT necessarily its
// struct-GEP index (see StructFieldGepIndex, which adds the vtable-
// pointer shift a hasVirtualDispatch class's own compiled layout has -
// see GetOrCreateStructType). Kept separate (rather than folding the
// shift in here) so `iface->fields[FieldIndex(...)]` - reading the
// field's own resolvedType/isReadonly/etc. - stays correct at every
// call site without also having to separately un-shift it back.
int Codegen::FieldIndex(InterfaceDecl *iface, const std::string &fieldName) {
  for (size_t i = 0; i < iface->fields.size(); i++)
    if (iface->fields[i].name == fieldName) return static_cast<int>(i);
  throw std::runtime_error("codegen: unknown field '" + fieldName + "' on interface '" + iface->name +
                            "' - Sema should have rejected this program");
}

// The field's real positional index in `iface`'s own COMPILED struct
// layout (what CreateStructGEP actually needs) - `FieldIndex`'s own
// result, shifted by one if this class has a vtable pointer occupying
// real field 0 (see GetOrCreateStructType/InterfaceDecl::
// hasVirtualDispatch).
int Codegen::StructFieldGepIndex(InterfaceDecl *iface, int fieldIndex) {
  return fieldIndex + (iface->hasVirtualDispatch ? 1 : 0);
}

std::vector<FunctionDecl *> Codegen::BuildVtableLayout(InterfaceDecl *iface) {
  int slotCount = 0;
  for (InterfaceDecl *cur = iface; cur; cur = cur->baseClass) {
    for (auto &m : cur->methods) {
      if (m->isVirtual) slotCount = std::max(slotCount, m->vtableSlot + 1);
    }
  }
  std::vector<FunctionDecl *> layout(slotCount, nullptr);
  // Root-to-`iface` order, so a derived class's own override (visited
  // LATER in this loop, since it's nearer the front of `chain` and this
  // walks it in reverse) correctly overwrites whatever its ancestor left
  // at the same slot.
  std::vector<InterfaceDecl *> chain;
  for (InterfaceDecl *cur = iface; cur; cur = cur->baseClass) chain.push_back(cur);
  for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
    for (auto &m : (*it)->methods) {
      if (m->isVirtual) layout[m->vtableSlot] = m.get();
    }
  }
  return layout;
}

llvm::GlobalVariable *Codegen::GetOrCreateVtable(InterfaceDecl *iface) {
  auto it = vtables.find(iface->name);
  if (it != vtables.end()) return it->second;

  std::vector<FunctionDecl *> layout = BuildVtableLayout(iface);
  llvm::Type *ptrTy = llvm::PointerType::get(context, 0);
  std::vector<llvm::Constant *> entries;
  entries.reserve(layout.size());
  for (FunctionDecl *m : layout) entries.push_back(llvmFunctions.at(m->name));

  auto *vtableTy = llvm::ArrayType::get(ptrTy, entries.size());
  auto *initializer = llvm::ConstantArray::get(vtableTy, entries);
  auto *global = new llvm::GlobalVariable(*module, vtableTy, /*isConstant=*/true, llvm::GlobalValue::InternalLinkage,
                                           initializer, "art.vtable." + iface->name);
  vtables[iface->name] = global;
  return global;
}

// ---------------------------------------------------------------------
// Heap allocation / strings
// ---------------------------------------------------------------------

llvm::Value *Codegen::GenHeapAlloc(uint64_t bytes) {
  return builder.CreateCall(gcMallocFn, {llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), bytes)});
}

llvm::Value *Codegen::GenHeapAlloc(llvm::Value *bytes) { return builder.CreateCall(gcMallocFn, {bytes}); }

// A string value is a pointer to the same { i64 length, ptr data } header
// arrays use. `data` is always null-terminated one byte past `length` (the
// terminator itself is never counted in `length` or touched by ART's own
// string operations) purely so it can be handed directly to a C function
// expecting a plain `const char*` - see art_bridge.h in the artisan repo.
//
// Returns the header global's own address - always a compile-time
// constant, so this doubles as both a string literal expression's value
// and a top-level `let`/`const` string's initializer (see GenGlobalDecl).
llvm::Constant *Codegen::BuildStringConstant(const std::string &value) {
  llvm::Constant *bytes = llvm::ConstantDataArray::getString(context, value, /*AddNull=*/true);
  auto *dataGlobal = new llvm::GlobalVariable(*module, bytes->getType(), /*isConstant=*/true,
                                               llvm::GlobalValue::PrivateLinkage, bytes, "art.str.data");
  dataGlobal->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Global);

  llvm::Constant *header = llvm::ConstantStruct::get(
      GetArrayHeaderType(),
      {llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), value.size()), dataGlobal});
  auto *headerGlobal = new llvm::GlobalVariable(*module, GetArrayHeaderType(), /*isConstant=*/true,
                                                 llvm::GlobalValue::PrivateLinkage, header, "art.str");
  headerGlobal->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Global);
  return headerGlobal;
}

llvm::Value *Codegen::GenStringLiteral(const std::string &value) { return BuildStringConstant(value); }

// A plain null-terminated `ptr`, not an ART string header - for handing a
// literal C string (e.g. a printf-style format) to an extern C function
// from within Codegen itself, never exposed to ART source.
llvm::Constant *Codegen::BuildCStringConstant(const std::string &value) {
  llvm::Constant *bytes = llvm::ConstantDataArray::getString(context, value, /*AddNull=*/true);
  auto *global = new llvm::GlobalVariable(*module, bytes->getType(), /*isConstant=*/true,
                                           llvm::GlobalValue::PrivateLinkage, bytes, "art.cstr");
  global->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Global);
  return global;
}

llvm::Value *Codegen::GenStringConcat(llvm::Value *lhsPtr, llvm::Value *rhsPtr) {
  llvm::Type *i64Ty = llvm::Type::getInt64Ty(context);
  llvm::Type *ptrTy = llvm::PointerType::get(context, 0);
  llvm::StructType *hdrTy = GetArrayHeaderType();

  llvm::Value *lhsLen = builder.CreateLoad(i64Ty, builder.CreateStructGEP(hdrTy, lhsPtr, 0));
  llvm::Value *rhsLen = builder.CreateLoad(i64Ty, builder.CreateStructGEP(hdrTy, rhsPtr, 0));
  llvm::Value *lhsData = builder.CreateLoad(ptrTy, builder.CreateStructGEP(hdrTy, lhsPtr, 1));
  llvm::Value *rhsData = builder.CreateLoad(ptrTy, builder.CreateStructGEP(hdrTy, rhsPtr, 1));

  llvm::Value *totalLen = builder.CreateAdd(lhsLen, rhsLen);
  llvm::Value *allocLen = builder.CreateAdd(totalLen, llvm::ConstantInt::get(i64Ty, 1)); // +1 for the null terminator
  llvm::Value *newData = GenHeapAlloc(allocLen);
  builder.CreateMemCpy(newData, llvm::MaybeAlign(1), lhsData, llvm::MaybeAlign(1), lhsLen);
  llvm::Value *newDataTail = builder.CreateGEP(llvm::Type::getInt8Ty(context), newData, {lhsLen});
  builder.CreateMemCpy(newDataTail, llvm::MaybeAlign(1), rhsData, llvm::MaybeAlign(1), rhsLen);
  llvm::Value *termPtr = builder.CreateGEP(llvm::Type::getInt8Ty(context), newData, {totalLen});
  builder.CreateStore(llvm::ConstantInt::get(llvm::Type::getInt8Ty(context), 0), termPtr);

  llvm::Value *newHeader = GenHeapAlloc(16);
  builder.CreateStore(totalLen, builder.CreateStructGEP(hdrTy, newHeader, 0));
  builder.CreateStore(newData, builder.CreateStructGEP(hdrTy, newHeader, 1));
  return newHeader;
}

llvm::Value *Codegen::GenStringEquals(llvm::Value *lhsPtr, llvm::Value *rhsPtr) {
  llvm::Type *i64Ty = llvm::Type::getInt64Ty(context);
  llvm::Type *ptrTy = llvm::PointerType::get(context, 0);
  llvm::StructType *hdrTy = GetArrayHeaderType();

  llvm::Value *lhsLen = builder.CreateLoad(i64Ty, builder.CreateStructGEP(hdrTy, lhsPtr, 0));
  llvm::Value *rhsLen = builder.CreateLoad(i64Ty, builder.CreateStructGEP(hdrTy, rhsPtr, 0));
  llvm::Value *lengthsEqual = builder.CreateICmpEQ(lhsLen, rhsLen);

  auto *checkBytesBB = llvm::BasicBlock::Create(context, "streq.bytes", currentFunction);
  auto *mergeBB = llvm::BasicBlock::Create(context, "streq.end", currentFunction);
  llvm::BasicBlock *entryBB = builder.GetInsertBlock();
  builder.CreateCondBr(lengthsEqual, checkBytesBB, mergeBB);

  builder.SetInsertPoint(checkBytesBB);
  llvm::Value *lhsData = builder.CreateLoad(ptrTy, builder.CreateStructGEP(hdrTy, lhsPtr, 1));
  llvm::Value *rhsData = builder.CreateLoad(ptrTy, builder.CreateStructGEP(hdrTy, rhsPtr, 1));
  llvm::Value *cmp = builder.CreateCall(memcmpFn, {lhsData, rhsData, lhsLen});
  llvm::Value *bytesEqual = builder.CreateICmpEQ(cmp, llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0));
  builder.CreateBr(mergeBB);

  builder.SetInsertPoint(mergeBB);
  llvm::PHINode *phi = builder.CreatePHI(llvm::Type::getInt1Ty(context), 2);
  phi->addIncoming(llvm::ConstantInt::getFalse(context), entryBB);
  phi->addIncoming(bytesEqual, checkBytesBB);
  return phi;
}

llvm::Value *Codegen::GenEqualityCheck(llvm::Value *lhsVal, llvm::Value *rhsVal, TypeTag tag) {
  if (tag == TypeTag::String) return GenStringEquals(lhsVal, rhsVal);
  if (tag == TypeTag::Handler) {
    // A Handler value is a 2-word {fn, env} aggregate, not a bare
    // pointer - a plain ICmpEQ doesn't accept an aggregate operand, and
    // even if it did, comparing only raw bytes would be structurally
    // right anyway only by accident. Equal means both fields match: same
    // underlying function AND same captured environment - two closures
    // compiled from the same source literal (e.g. two different loop
    // iterations) share one generated thunk function pointer but have
    // different envs, and must compare unequal, matching real JS
    // reference semantics for closures.
    llvm::Value *lhsFn = builder.CreateExtractValue(lhsVal, 0);
    llvm::Value *rhsFn = builder.CreateExtractValue(rhsVal, 0);
    llvm::Value *lhsEnv = builder.CreateExtractValue(lhsVal, 1);
    llvm::Value *rhsEnv = builder.CreateExtractValue(rhsVal, 1);
    return builder.CreateAnd(builder.CreateICmpEQ(lhsFn, rhsFn), builder.CreateICmpEQ(lhsEnv, rhsEnv));
  }
  // Number/Enum both compare as float (an Enum's runtime representation
  // is exactly the same `double` a Number's already is - see
  // EnumDecl's own doc comment); every other reachable tag here
  // (Boolean, and - via Sema's generic "same tag" rule for "=="/"!=" -
  // Struct/Array, both represented as `ptr`) compares as a plain
  // integer/pointer identity check.
  return (tag == TypeTag::Number || tag == TypeTag::Enum) ? builder.CreateFCmpOEQ(lhsVal, rhsVal)
                                                           : builder.CreateICmpEQ(lhsVal, rhsVal);
}

llvm::Value *Codegen::GenStringIndex(llvm::Value *strPtr, llvm::Value *idxVal) {
  llvm::StructType *hdrTy = GetArrayHeaderType();
  llvm::Value *idxInt = builder.CreateFPToSI(idxVal, llvm::Type::getInt64Ty(context));
  llvm::Value *dataPtr = builder.CreateLoad(llvm::PointerType::get(context, 0), builder.CreateStructGEP(hdrTy, strPtr, 1));
  llvm::Value *charPtr = builder.CreateGEP(llvm::Type::getInt8Ty(context), dataPtr, {idxInt});
  llvm::Value *charByte = builder.CreateLoad(llvm::Type::getInt8Ty(context), charPtr);

  llvm::Value *newData = GenHeapAlloc(2); // char + null terminator
  builder.CreateStore(charByte, newData);
  llvm::Value *termPtr = builder.CreateGEP(llvm::Type::getInt8Ty(context), newData,
                                            {llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 1)});
  builder.CreateStore(llvm::ConstantInt::get(llvm::Type::getInt8Ty(context), 0), termPtr);
  llvm::Value *newHeader = GenHeapAlloc(16);
  builder.CreateStore(llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 1), builder.CreateStructGEP(hdrTy, newHeader, 0));
  builder.CreateStore(newData, builder.CreateStructGEP(hdrTy, newHeader, 1));
  return newHeader;
}

// ---------------------------------------------------------------------
// Scopes
// ---------------------------------------------------------------------

void Codegen::PushScope() { scopes.emplace_back(); }
void Codegen::PopScope() { scopes.pop_back(); }

Codegen::VarBinding *Codegen::TryLookup(const std::string &name) {
  for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
    auto found = it->find(name);
    if (found != it->end()) return &found->second;
  }
  auto globalIt = globalVars.find(name);
  if (globalIt != globalVars.end()) return &globalIt->second;
  return nullptr;
}

Codegen::VarBinding *Codegen::Lookup(const std::string &name) {
  VarBinding *b = TryLookup(name);
  if (!b) throw std::runtime_error("codegen: undefined variable '" + name + "' - Sema should have rejected this program");
  return b;
}

void Codegen::Declare(const std::string &name, llvm::Value *alloca, llvm::Type *type, bool isBoxed) {
  scopes.back()[name] = VarBinding{alloca, type, isBoxed};
}

llvm::Value *Codegen::LoadVar(VarBinding *b, const std::string &name) {
  if (!b->isBoxed) return builder.CreateLoad(b->type, b->alloca, name);
  llvm::Value *cellPtr = builder.CreateLoad(llvm::PointerType::get(context, 0), b->alloca, name + ".cell");
  return builder.CreateLoad(b->type, cellPtr, name);
}

llvm::AllocaInst *Codegen::CreateEntryAlloca(llvm::Function *fn, llvm::Type *type, const std::string &name) {
  llvm::IRBuilder<> tmp(&fn->getEntryBlock(), fn->getEntryBlock().begin());
  return tmp.CreateAlloca(type, nullptr, name);
}

// ---------------------------------------------------------------------
// Top level
// ---------------------------------------------------------------------

// See this method's own doc comment in codegen.h.
void Codegen::GenGCInit() {
  llvm::FunctionType *gcInitTy = llvm::FunctionType::get(llvm::Type::getVoidTy(context), {}, false);
  llvm::FunctionCallee gcInitFn = module->getOrInsertFunction("GC_init", gcInitTy);

  auto *ctorFn = llvm::Function::Create(gcInitTy, llvm::Function::InternalLinkage, "art.gc_ctor", module.get());
  auto *entry = llvm::BasicBlock::Create(context, "entry", ctorFn);
  llvm::IRBuilder<> ctorBuilder(entry);
  ctorBuilder.CreateCall(gcInitFn, {});
  ctorBuilder.CreateRetVoid();

  llvm::appendToGlobalCtors(*module, ctorFn, /*Priority=*/0);
}

std::unique_ptr<llvm::Module> Codegen::Generate(Program &program) {
  module = std::make_unique<llvm::Module>(moduleName, context);
  SetupTarget();

  gcMallocFn = module->getOrInsertFunction(
      "GC_malloc", llvm::FunctionType::get(llvm::PointerType::get(context, 0), {llvm::Type::getInt64Ty(context)}, false));
  GenGCInit();
  memcmpFn = module->getOrInsertFunction(
      "memcmp", llvm::FunctionType::get(llvm::Type::getInt32Ty(context),
                                         {llvm::PointerType::get(context, 0), llvm::PointerType::get(context, 0),
                                          llvm::Type::getInt64Ty(context)},
                                         false));
  // libc's own setjmp/longjmp - see GetExceptionFrameType's own doc
  // comment for the frame shape these operate on, and codegen.h's own
  // note on why calling them directly (never through a wrapper function)
  // matters. `setjmp` takes/returns the platform's real ABI shape (an
  // opaque jmp_buf, decayed to a pointer to its first byte, and a plain
  // i32 - true on every platform this targets, glibc and macOS libc
  // alike).
  setjmpFn = module->getOrInsertFunction(
      "setjmp", llvm::FunctionType::get(llvm::Type::getInt32Ty(context), {llvm::PointerType::get(context, 0)}, false));
  // Without this, LLVM's optimizer is free to assume a call only ever
  // returns once - and can then miscompile code around it (e.g. treating
  // a value computed before the call as still valid after the "second
  // return" a later longjmp produces, when a jump elsewhere may have run
  // in between). This is the one attribute that tells it setjmp is
  // special.
  llvm::cast<llvm::Function>(setjmpFn.getCallee())->addFnAttr(llvm::Attribute::ReturnsTwice);
  longjmpFn = module->getOrInsertFunction(
      "longjmp", llvm::FunctionType::get(llvm::Type::getVoidTy(context),
                                          {llvm::PointerType::get(context, 0), llvm::Type::getInt32Ty(context)}, false));
  // Never returns to its own caller (control resumes at the matching
  // setjmp call site instead) - a real, useful fact for the optimizer,
  // not just documentation.
  llvm::cast<llvm::Function>(longjmpFn.getCallee())->addFnAttr(llvm::Attribute::NoReturn);
  abortFn = module->getOrInsertFunction("abort", llvm::FunctionType::get(llvm::Type::getVoidTy(context), {}, false));
  llvm::cast<llvm::Function>(abortFn.getCallee())->addFnAttr(llvm::Attribute::NoReturn);
  exceptionCurrentHandler = new llvm::GlobalVariable(
      *module, llvm::PointerType::get(context, 0), /*isConstant=*/false, llvm::GlobalValue::InternalLinkage,
      llvm::ConstantPointerNull::get(llvm::PointerType::get(context, 0)), "art.exception.currentHandler");

  GenBuiltinNumberToString();
  GenBuiltinStringToNumber();

  for (auto &g : program.globals) GenGlobalDecl(g.get());

  // A generic function/declare-function's own template FunctionDecl is
  // skipped entirely here - it was never given resolved param/return
  // types (see Sema::RegisterFunctionSignature) since there's no concrete
  // type to resolve them against. Only its instantiations (each an
  // ordinary, fully-concrete FunctionDecl Sema cloned and checked per
  // distinct `name::<Type,...>` call site actually used - see
  // Sema::CheckGenericCall) are real, generated below.
  std::vector<FunctionDecl *> concreteFunctions;
  for (auto &fn : program.functions)
    if (fn->typeParams.empty()) concreteFunctions.push_back(fn.get());
  // A class's methods (see InterfaceDecl::methods) are ordinary, already-
  // qualified FunctionDecls owned by their InterfaceDecl rather than
  // Program::functions - Sema registered/checked them exactly like any
  // other function (see Sema::Check), so they're generated the same way
  // too, just collected from a different place. Collected from
  // sema.Interfaces() rather than program.interfaces specifically so a
  // generic class's own instantiations (see Sema::InstantiateInterface)
  // are included too - those live only in Sema's own interfaces map
  // (interfaceInstantiationStorage), never in Program::interfaces itself
  // (the template there is skipped entirely - never checked/qualified in
  // template form, same "no per-instantiation template-skipping needed
  // here since the template was never in this set to begin with" deal
  // sema.Instantiations() already has for generic functions below).
  // sema.Interfaces() also already contains every plain, non-generic
  // class exactly once, so this fully replaces (not just supplements)
  // walking program.interfaces directly.
  for (auto &[name, iface] : sema.Interfaces())
    for (auto &method : iface->methods) concreteFunctions.push_back(method.get());
  std::vector<FunctionDecl *> externFunctions;
  for (auto &fn : program.externFunctions)
    if (fn->typeParams.empty()) externFunctions.push_back(fn.get());
  // "makeArray$..." instantiations (see Sema::SeedBuiltins/
  // GenBuiltinMakeArray) are split out here rather than joining
  // `instantiations` below - unlike a generic function/`declare
  // function`'s own instantiations (a real ART body to compile, or a
  // bare extern declaration resolved at link time), each of these needs
  // Codegen to hand-generate a real definition directly, so it's neither
  // of those two cases. The "makeArray$" prefix alone is enough to tell
  // them apart safely: Sema's own duplicate-declaration checking already
  // guarantees no user function can ever be named "makeArray" too.
  std::vector<FunctionDecl *> instantiations;
  std::vector<FunctionDecl *> makeArrayInstantiations;
  std::vector<FunctionDecl *> notNullInstantiations;
  for (auto &[mangledName, inst] : sema.Instantiations()) {
    if (mangledName.rfind("makeArray$", 0) == 0) {
      makeArrayInstantiations.push_back(inst);
    } else if (mangledName.rfind("notNull$", 0) == 0) {
      notNullInstantiations.push_back(inst);
    } else {
      instantiations.push_back(inst);
    }
  }

  DeclareFunctionSignatures(concreteFunctions, /*allowMainRename=*/true);
  DeclareFunctionSignatures(externFunctions, /*allowMainRename=*/false);
  DeclareFunctionSignatures(instantiations, /*allowMainRename=*/false);
  DeclareFunctionSignatures(makeArrayInstantiations, /*allowMainRename=*/false);
  DeclareFunctionSignatures(notNullInstantiations, /*allowMainRename=*/false);
  // Every closure literal Sema found, declared the same "signature
  // first, body later" way as everything else above - a closure can be
  // referenced before its own textual position (e.g. stored in a
  // global's initializer).
  DeclareClosureThunkSignatures(sema.Closures());
  // Needs every function signature already declared above (a global
  // initializer may call one), but not yet defined - see its own doc
  // comment in codegen.h.
  GenGlobalInit(program.globals);
  // Always ensures a "setupApp" symbol exists - main.cpp's trampoline
  // calls it unconditionally, so a project with neither an explicit
  // `function setupApp()` nor any top-level statements still needs one,
  // just an empty one (GenSetupAppBody handles an empty `stmts` the same
  // way any other function with an empty body would). An explicit
  // `function setupApp()`, if the project has one, already declared
  // this exact symbol via DeclareFunctionSignatures above - Sema::Check
  // guarantees topLevelStmts is empty whenever that's the case, so
  // there's nothing to actually generate here either way, just skip
  // re-declaring the same symbol twice.
  if (!llvmFunctions.count("setupApp")) GenSetupAppBody(program.topLevelStmts);
  for (auto *fn : concreteFunctions) GenFunction(fn);
  for (auto *fn : instantiations)
    if (fn->body) GenFunction(fn);
  for (auto *fn : sema.Closures()) GenClosureFunction(fn);
  for (auto *fn : makeArrayInstantiations) GenBuiltinMakeArray(fn);
  for (auto *fn : notNullInstantiations) GenBuiltinNotNull(fn);
  // program.externFunctions and a generic `declare function`'s own
  // instantiations (body == nullptr) stay as bare external declarations,
  // resolved at link time against whatever object/library actually
  // defines them.

  // A zero-arg ART `main` returning number/void becomes the process entry
  // point, but its LLVM function returns `double` (or `void`), not the `i32`
  // the C runtime's `main` ABI requires - emit a thin wrapper that adapts.
  auto mainIt = sema.Functions().find("main");
  if (mainIt != sema.Functions().end() && mainIt->second->params.empty() &&
      (mainIt->second->resolvedReturnType.tag == TypeTag::Number ||
       mainIt->second->resolvedReturnType.tag == TypeTag::Void)) {
    llvm::Function *artMain = llvmFunctions.at("main");
    auto *wrapperTy = llvm::FunctionType::get(llvm::Type::getInt32Ty(context), {}, false);
    auto *wrapper = llvm::Function::Create(wrapperTy, llvm::Function::ExternalLinkage, "main", module.get());
    auto *entryBB = llvm::BasicBlock::Create(context, "entry", wrapper);
    builder.SetInsertPoint(entryBB);
    llvm::Value *result = builder.CreateCall(artMain, {});
    if (mainIt->second->resolvedReturnType.tag == TypeTag::Number) {
      builder.CreateRet(builder.CreateFPToSI(result, llvm::Type::getInt32Ty(context)));
    } else {
      builder.CreateRet(llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0));
    }
  }

  return std::move(module);
}

void Codegen::DeclareFunctionSignatures(const std::vector<FunctionDecl *> &decls, bool allowMainRename) {
  for (auto *decl : decls) {
    std::vector<llvm::Type *> paramTypes;
    std::vector<std::string> argNames; // parallel to the actual LLVM args - longer than decl->params whenever a
                                        // Handler param below expands into two
    paramTypes.reserve(decl->params.size());
    argNames.reserve(decl->params.size());
    for (auto &p : decl->params) {
      if (decl->isExtern && p.resolvedType.tag == TypeTag::Handler) {
        // A Handler-typed parameter of a real native-ABI function
        // (`declare function`) is two separate native words on the C
        // side (see include/art_bridge.h's ArtHandler/ArtEventHandler
        // typedefs), not one LLVM struct value - matches the unpacking
        // AppendCallArg does at every call site passing one of these.
        llvm::Type *ptrTy = llvm::PointerType::get(context, 0);
        paramTypes.push_back(ptrTy);
        paramTypes.push_back(ptrTy);
        argNames.push_back(p.name + ".fn");
        argNames.push_back(p.name + ".env");
      } else {
        paramTypes.push_back(MapType(p.resolvedType));
        argNames.push_back(p.name);
      }
    }
    llvm::Type *retTy = MapType(decl->resolvedReturnType);
    auto *fnTy = llvm::FunctionType::get(retTy, paramTypes, false);
    // ART's own "main" (if any) is renamed at the LLVM level so the real C
    // ABI entry point ("main", built after every function is generated -
    // see Generate()) can own that symbol instead. Never applies to a
    // `declare function` - it must bind to the external symbol verbatim.
    std::string llvmName = (allowMainRename && decl->name == "main") ? "__art_main" : decl->name;
    auto *fn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, llvmName, module.get());
    size_t i = 0;
    for (auto &arg : fn->args()) arg.setName(argNames[i++]);
    llvmFunctions[decl->name] = fn;
  }
}

// See this method's own doc comment in codegen.h.
FunctionDecl *Codegen::LookupCalleeDecl(const std::string &name) {
  auto it = sema.Functions().find(name);
  if (it != sema.Functions().end()) return it->second;
  auto it2 = sema.Instantiations().find(name);
  if (it2 != sema.Instantiations().end()) return it2->second;
  return nullptr;
}

// See this method's own doc comment in codegen.h.
void Codegen::AppendCallArg(std::vector<llvm::Value *> &args, llvm::Value *val, const ResolvedType &ty,
                             bool unpackHandler) {
  if (unpackHandler && ty.tag == TypeTag::Handler) {
    args.push_back(builder.CreateExtractValue(val, 0)); // fn
    args.push_back(builder.CreateExtractValue(val, 1)); // env
  } else {
    args.push_back(val);
  }
}

// See this method's own doc comment in codegen.h.
llvm::Function *Codegen::GetOrCreatePlainThunk(const std::string &fnName) {
  auto it = plainHandlerThunks.find(fnName);
  if (it != plainHandlerThunks.end()) return it->second;
  llvm::Function *real = llvmFunctions.at(fnName);
  llvm::Type *ptrTy = llvm::PointerType::get(context, 0);
  std::vector<llvm::Type *> paramTypes = {ptrTy};
  paramTypes.reserve(1 + real->getFunctionType()->params().size());
  for (auto *t : real->getFunctionType()->params()) paramTypes.push_back(t);
  auto *thunkTy = llvm::FunctionType::get(llvm::Type::getVoidTy(context), paramTypes, false);
  auto *thunk =
      llvm::Function::Create(thunkTy, llvm::Function::InternalLinkage, "art.handler_thunk." + fnName, module.get());
  auto *entry = llvm::BasicBlock::Create(context, "entry", thunk);
  // A LOCAL builder, deliberately not `this->builder` - this can be
  // triggered mid-generation of some unrelated function (the first time
  // that function's own body references `fnName` as a value) and must
  // not disturb its insert point, the same reasoning CreateEntryAlloca's
  // own temporary builder already has.
  llvm::IRBuilder<> tb(entry);
  std::vector<llvm::Value *> callArgs;
  auto argIt = thunk->arg_begin();
  ++argIt; // skip env - a plain function reference never captures anything
  for (; argIt != thunk->arg_end(); ++argIt) callArgs.push_back(&*argIt);
  tb.CreateCall(real, callArgs);
  tb.CreateRetVoid();
  plainHandlerThunks[fnName] = thunk;
  return thunk;
}

// See this method's own doc comment in codegen.h.
llvm::Value *Codegen::BuildPlainFunctionHandlerValue(const std::string &fnName) {
  llvm::Function *thunk = GetOrCreatePlainThunk(fnName);
  llvm::Value *agg = llvm::UndefValue::get(GetHandlerStructType());
  agg = builder.CreateInsertValue(agg, thunk, {0});
  agg = builder.CreateInsertValue(agg, llvm::ConstantPointerNull::get(llvm::PointerType::get(context, 0)), {1});
  return agg;
}

void Codegen::GenGlobalDecl(Stmt *stmt) {
  llvm::Type *ty = MapType(stmt->resolvedVarType);
  ExprKind k = stmt->expr->kind;

  // A bare number/boolean/string literal is a real, compile-time
  // llvm::Constant - no IRBuilder/basic-block machinery needed to build
  // it, and (unlike the general case below) genuinely never changes
  // after this, so it's safe to mark isConstant for an ART `const` too.
  if (k == ExprKind::NumberLiteral || k == ExprKind::BoolLiteral || k == ExprKind::StringLiteral) {
    llvm::Constant *init;
    switch (k) {
    case ExprKind::NumberLiteral:
      init = llvm::ConstantFP::get(llvm::Type::getDoubleTy(context), stmt->expr->numberValue);
      break;
    case ExprKind::BoolLiteral:
      init = llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), stmt->expr->boolValue ? 1 : 0);
      break;
    default:
      init = BuildStringConstant(stmt->expr->name);
      break;
    }
    auto *global = new llvm::GlobalVariable(*module, ty, /*isConstant=*/stmt->isConst,
                                             llvm::GlobalValue::InternalLinkage, init, "global." + stmt->varName);
    globalVars[stmt->varName] = VarBinding{global, ty};
    return;
  }

  // Anything else (a call, an object/array literal, another global, a
  // bare function reference, ...) isn't a compile-time constant - declare
  // the global now, zero-valued, and let GenGlobalInit's module
  // constructor compute/store its real value once, in declaration order,
  // as real code (see its own doc comment). Never isConstant, even for
  // an ART `const`: GenGlobalInit still needs to store into it exactly
  // once - ART-level immutability past that point is already fully
  // enforced by Sema (CheckLValueTarget rejects reassigning a `const`),
  // not by an LLVM-level guarantee.
  llvm::Constant *zero = llvm::Constant::getNullValue(ty);
  auto *global = new llvm::GlobalVariable(*module, ty, /*isConstant=*/false, llvm::GlobalValue::InternalLinkage,
                                           zero, "global." + stmt->varName);
  globalVars[stmt->varName] = VarBinding{global, ty};
}

// See this method's own doc comment in codegen.h.
void Codegen::GenGlobalInit(std::vector<std::unique_ptr<Stmt>> &globals) {
  bool anyNonLiteral = false;
  for (auto &g : globals) {
    ExprKind k = g->expr->kind;
    if (k != ExprKind::NumberLiteral && k != ExprKind::BoolLiteral && k != ExprKind::StringLiteral) {
      anyNonLiteral = true;
      break;
    }
  }
  if (!anyNonLiteral) return;

  llvm::FunctionType *ctorTy = llvm::FunctionType::get(llvm::Type::getVoidTy(context), {}, false);
  auto *ctorFn = llvm::Function::Create(ctorTy, llvm::Function::InternalLinkage, "art.globals_ctor", module.get());
  auto *entry = llvm::BasicBlock::Create(context, "entry", ctorFn);
  llvm::Function *savedCurrentFunction = currentFunction;
  currentFunction = ctorFn;
  builder.SetInsertPoint(entry);

  for (auto &g : globals) {
    ExprKind k = g->expr->kind;
    if (k == ExprKind::NumberLiteral || k == ExprKind::BoolLiteral || k == ExprKind::StringLiteral) continue;
    llvm::Value *val = GenExpr(g->expr.get());
    VarBinding &binding = globalVars.at(g->varName);
    builder.CreateStore(val, binding.alloca);
  }

  builder.CreateRetVoid();
  currentFunction = savedCurrentFunction;

  // A higher priority number than GenGCInit's 0 - LLVM runs lower-
  // priority constructors first, so GC_init is always safe to have
  // already run by the time any initializer here might allocate.
  llvm::appendToGlobalCtors(*module, ctorFn, /*Priority=*/1);
}

// See this method's own doc comment in codegen.h.
void Codegen::GenSetupAppBody(std::vector<std::unique_ptr<Stmt>> &stmts) {
  auto *fnTy = llvm::FunctionType::get(llvm::Type::getVoidTy(context), {}, false);
  auto *fn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, "setupApp", module.get());
  llvmFunctions["setupApp"] = fn;
  currentFunction = fn;

  auto *entry = llvm::BasicBlock::Create(context, "entry", fn);
  builder.SetInsertPoint(entry);
  PushScope();

  for (auto &s : stmts) GenStmt(s.get());

  if (!builder.GetInsertBlock()->getTerminator()) {
    builder.CreateRetVoid();
  }

  PopScope();
  currentFunction = nullptr;
}

// Backs Sema::SeedBuiltins' "numberToString" - real double-to-string
// formatting (correct rounding, shortest-round-trip digit counts, ...) is
// its own small research problem, so this defers to libc's `snprintf`
// rather than reimplementing it: `%.15g` matches double's ~15 significant
// decimal digits of precision, using %f-style output for an ordinary-
// magnitude number (e.g. 42 -> "42", not "42.000000" - %g trims
// insignificant trailing zeros and the decimal point itself) and falling
// back to %e-style scientific notation only once the magnitude actually
// needs it. Not a guaranteed match for real JS's own Number-to-string
// algorithm at the edges (NaN/Infinity print as "nan"/"inf", not
// "NaN"/"Infinity"; -0 prints as "-0", not "0"; extreme magnitudes may
// round or switch notation slightly differently) - close enough for the
// ordinary counters/measurements an ART program actually computes.
void Codegen::GenBuiltinNumberToString() {
  llvm::Type *doubleTy = llvm::Type::getDoubleTy(context);
  llvm::Type *ptrTy = llvm::PointerType::get(context, 0);
  llvm::Type *i64Ty = llvm::Type::getInt64Ty(context);
  llvm::Type *i32Ty = llvm::Type::getInt32Ty(context);

  auto *fnTy = llvm::FunctionType::get(ptrTy, {doubleTy}, false);
  auto *fn = llvm::Function::Create(fnTy, llvm::Function::InternalLinkage, "art.numberToString", module.get());
  llvmFunctions["numberToString"] = fn;
  currentFunction = fn;

  auto *entry = llvm::BasicBlock::Create(context, "entry", fn);
  builder.SetInsertPoint(entry);
  llvm::Argument *nArg = &*fn->arg_begin();
  nArg->setName("n");

  // Generous fixed-size scratch buffer: worst case is a sign, ~15 digits,
  // a decimal point, and a 4-5 character exponent - nowhere near 32
  // bytes. Reused directly as the resulting ART string's data buffer (no
  // separate copy) since snprintf already null-terminates it in place,
  // exactly the shape ART's own string codegen builds.
  llvm::Value *buf = GenHeapAlloc(32);
  llvm::Constant *fmt = BuildCStringConstant("%.15g");
  llvm::FunctionCallee snprintfFn = module->getOrInsertFunction(
      "snprintf", llvm::FunctionType::get(i32Ty, {ptrTy, i64Ty, ptrTy}, /*isVarArg=*/true));
  builder.CreateCall(snprintfFn, {buf, llvm::ConstantInt::get(i64Ty, 32), fmt, nArg});

  llvm::FunctionCallee strlenFn = module->getOrInsertFunction("strlen", llvm::FunctionType::get(i64Ty, {ptrTy}, false));
  llvm::Value *len = builder.CreateCall(strlenFn, {buf});

  llvm::Value *header = GenHeapAlloc(16);
  builder.CreateStore(len, builder.CreateStructGEP(GetArrayHeaderType(), header, 0));
  builder.CreateStore(buf, builder.CreateStructGEP(GetArrayHeaderType(), header, 1));
  builder.CreateRet(header);
  currentFunction = nullptr;
}

// Backs Sema::SeedBuiltins' "stringToNumber" - defers to libc's `strtod`
// for the actual parsing, same "trust libc over reimplementing a
// well-known hard problem" call GenBuiltinNumberToString already makes
// for the opposite direction. `s`'s data buffer is already
// null-terminated (see ArtString's own doc comment in art_bridge.h, and
// every other string codegen path here that relies on it), so it can be
// handed to strtod directly with no copy. Parses a leading numeric
// prefix - optional whitespace, sign, digits, decimal point, exponent -
// the same lenient behavior real JS `parseFloat` has (`parseFloat(
// "42px")` -> 42), not real JS `Number()`, which requires the whole
// string to be numeric. strtod's own "no conversion could be performed"
// case (nothing numeric at the start at all) isn't distinguished from a
// genuine "0" - both return 0.0, the same "no exceptions, simple
// default on failure" contract every other ART builtin already has (see
// ArtChildAt's out-of-bounds note, or makeArray's aliasing one) rather
// than needing a NaN/isNaN pair just for this one case.
void Codegen::GenBuiltinStringToNumber() {
  llvm::Type *doubleTy = llvm::Type::getDoubleTy(context);
  llvm::Type *ptrTy = llvm::PointerType::get(context, 0);

  auto *fnTy = llvm::FunctionType::get(doubleTy, {ptrTy}, false);
  auto *fn = llvm::Function::Create(fnTy, llvm::Function::InternalLinkage, "art.stringToNumber", module.get());
  llvmFunctions["stringToNumber"] = fn;
  currentFunction = fn;

  auto *entry = llvm::BasicBlock::Create(context, "entry", fn);
  builder.SetInsertPoint(entry);
  llvm::Argument *sArg = &*fn->arg_begin();
  sArg->setName("s");

  llvm::StructType *hdrTy = GetArrayHeaderType();
  llvm::Value *dataPtr = builder.CreateLoad(ptrTy, builder.CreateStructGEP(hdrTy, sArg, 1));

  llvm::FunctionCallee strtodFn =
      module->getOrInsertFunction("strtod", llvm::FunctionType::get(doubleTy, {ptrTy, ptrTy}, false));
  llvm::Value *result = builder.CreateCall(strtodFn, {dataPtr, llvm::Constant::getNullValue(ptrTy)});
  builder.CreateRet(result);
  currentFunction = nullptr;
}

// See this method's own doc comment in codegen.h.
void Codegen::GenBuiltinMakeArray(FunctionDecl *inst) {
  const ResolvedType &elemType = *inst->resolvedReturnType.elementType;
  llvm::Type *storageTy = ArrayElemStorageType(elemType);
  // Computed from the actual LLVM storage type rather than hardcoded -
  // Handler is a 16-byte {ptr,ptr} struct, not an 8-byte pointer, so a
  // fixed "boolean ? 1 : 8" would under-allocate a Handler[]'s backing
  // buffer by half (real heap corruption, not just a wrong size).
  uint64_t elemSize = module->getDataLayout().getTypeAllocSize(storageTy).getFixedValue();

  llvm::Function *fn = llvmFunctions.at(inst->name);
  currentFunction = fn;
  auto *entry = llvm::BasicBlock::Create(context, "entry", fn);
  builder.SetInsertPoint(entry);

  llvm::Value *sizeArg = fn->getArg(0);
  llvm::Value *fillArg = fn->getArg(1);
  sizeArg->setName("size");
  fillArg->setName("fill");

  llvm::Type *i64Ty = llvm::Type::getInt64Ty(context);
  // `size` must be >= 0 - same "undefined for nonsensical input" deal
  // ArtChildAt's own out-of-bounds contract already has, not something
  // checked at runtime.
  llvm::Value *count = builder.CreateFPToSI(sizeArg, i64Ty, "count");
  llvm::Value *totalBytes = builder.CreateMul(count, llvm::ConstantInt::get(i64Ty, elemSize));
  llvm::Value *dataPtr = GenHeapAlloc(totalBytes);

  llvm::AllocaInst *iAlloca = CreateEntryAlloca(fn, i64Ty, "i");
  builder.CreateStore(llvm::ConstantInt::get(i64Ty, 0), iAlloca);

  auto *condBB = llvm::BasicBlock::Create(context, "makearray.cond", fn);
  auto *bodyBB = llvm::BasicBlock::Create(context, "makearray.body", fn);
  auto *endBB = llvm::BasicBlock::Create(context, "makearray.end", fn);

  builder.CreateBr(condBB);
  builder.SetInsertPoint(condBB);
  llvm::Value *iVal = builder.CreateLoad(i64Ty, iAlloca);
  llvm::Value *cond = builder.CreateICmpSLT(iVal, count);
  builder.CreateCondBr(cond, bodyBB, endBB);

  builder.SetInsertPoint(bodyBB);
  llvm::Value *elemPtr = builder.CreateGEP(storageTy, dataPtr, {iVal});
  llvm::Value *toStore = fillArg;
  if (elemType.tag == TypeTag::Boolean) toStore = builder.CreateZExt(fillArg, llvm::Type::getInt8Ty(context));
  builder.CreateStore(toStore, elemPtr);
  llvm::Value *iNext = builder.CreateAdd(iVal, llvm::ConstantInt::get(i64Ty, 1));
  builder.CreateStore(iNext, iAlloca);
  builder.CreateBr(condBB);

  builder.SetInsertPoint(endBB);
  llvm::Value *headerPtr = GenHeapAlloc(16);
  builder.CreateStore(count, builder.CreateStructGEP(GetArrayHeaderType(), headerPtr, 0));
  builder.CreateStore(dataPtr, builder.CreateStructGEP(GetArrayHeaderType(), headerPtr, 1));
  builder.CreateRet(headerPtr);

  currentFunction = nullptr;
}

void Codegen::GenBuiltinNotNull(FunctionDecl *inst) {
  // `inst->resolvedReturnType` is Nullable(T) - `*elementType` is T
  // itself, whatever it actually is for this instantiation.
  const ResolvedType &innerType = *inst->resolvedReturnType.elementType;
  llvm::Type *innerTy = MapType(innerType);
  uint64_t bytes = module->getDataLayout().getTypeAllocSize(innerTy).getFixedValue();

  llvm::Function *fn = llvmFunctions.at(inst->name);
  currentFunction = fn;
  auto *entry = llvm::BasicBlock::Create(context, "entry", fn);
  builder.SetInsertPoint(entry);

  llvm::Value *vArg = fn->getArg(0);
  vArg->setName("v");

  llvm::Value *cellPtr = GenHeapAlloc(bytes);
  builder.CreateStore(vArg, cellPtr);
  builder.CreateRet(cellPtr);

  currentFunction = nullptr;
}

void Codegen::GenFunction(FunctionDecl *decl) {
  llvm::Function *fn = llvmFunctions.at(decl->name);
  currentFunction = fn;

  auto *entry = llvm::BasicBlock::Create(context, "entry", fn);
  builder.SetInsertPoint(entry);
  PushScope();

  size_t i = 0;
  for (auto &arg : fn->args()) DeclareParamBinding(fn, decl->params[i++], &arg);

  GenStmt(decl->body.get());

  if (!builder.GetInsertBlock()->getTerminator()) {
    if (decl->resolvedReturnType.tag == TypeTag::Void) {
      builder.CreateRetVoid();
    } else {
      // Sema's definite-return check guarantees every non-void path already
      // returned; this is unreachable but keeps the block well-formed.
      builder.CreateUnreachable();
    }
  }

  PopScope();
  currentFunction = nullptr;
}

// See this method's own doc comment in codegen.h. `fn` is the function
// the parameter is being bound INTO (needed for CreateEntryAlloca's own
// entry-block-of-this-function requirement) - not necessarily
// `currentFunction`-by-another-name, just always equal to it in
// practice, since this is only ever called while generating `fn`'s own
// body.
void Codegen::DeclareParamBinding(llvm::Function *fn, const Param &p, llvm::Value *argVal) {
  llvm::Type *ty = MapType(p.resolvedType);
  if (p.isCapturedByClosure) {
    llvm::Type *ptrTy = llvm::PointerType::get(context, 0);
    llvm::AllocaInst *slot = CreateEntryAlloca(fn, ptrTy, p.name);
    uint64_t bytes = module->getDataLayout().getTypeAllocSize(ty).getFixedValue();
    llvm::Value *cellPtr = GenHeapAlloc(bytes); // once per call - a param is bound exactly once per invocation
    builder.CreateStore(argVal, cellPtr);
    builder.CreateStore(cellPtr, slot);
    Declare(p.name, slot, ty, /*isBoxed=*/true);
  } else {
    llvm::AllocaInst *alloca = CreateEntryAlloca(fn, ty, p.name);
    builder.CreateStore(argVal, alloca);
    Declare(p.name, alloca, ty);
  }
}

// See this method's own doc comment in codegen.h.
void Codegen::DeclareClosureThunkSignatures(const std::vector<FunctionDecl *> &closureFns) {
  llvm::Type *ptrTy = llvm::PointerType::get(context, 0);
  for (auto *fn : closureFns) {
    std::vector<llvm::Type *> paramTypes = {ptrTy};
    paramTypes.reserve(1 + fn->params.size());
    for (auto &p : fn->params) paramTypes.push_back(MapType(p.resolvedType));
    auto *fnTy = llvm::FunctionType::get(llvm::Type::getVoidTy(context), paramTypes, false);
    auto *thunk = llvm::Function::Create(fnTy, llvm::Function::InternalLinkage, fn->name, module.get());
    thunk->getArg(0)->setName("env");
    size_t i = 1;
    for (auto &p : fn->params) thunk->getArg(static_cast<unsigned>(i++))->setName(p.name);
    llvmFunctions[fn->name] = thunk;
  }
}

// See this method's own doc comment in codegen.h.
void Codegen::GenClosureFunction(FunctionDecl *fn) {
  llvm::Function *thunkFn = llvmFunctions.at(fn->name);
  currentFunction = thunkFn;
  auto *entry = llvm::BasicBlock::Create(context, "entry", thunkFn);
  builder.SetInsertPoint(entry);
  PushScope();

  llvm::Type *ptrTy = llvm::PointerType::get(context, 0);
  auto argIt = thunkFn->arg_begin();
  llvm::Argument *envArg = &*argIt++;

  if (!fn->captures.empty()) {
    // Unpacks each captured cell's address out of the env struct into an
    // ordinary boxed local binding (one more alloca+store) - purely so
    // every other piece of codegen (LoadVar, GenLValue's Identifier
    // branch) treats "captured, unpacked from env" identically to
    // "declared locally and boxed", with no special-casing anywhere
    // else. This is also what makes N-level nesting work: a nested
    // closure's own env-build code (see the FunctionExpr case below)
    // looks these up the exact same way it would look up a directly-
    // declared boxed local.
    std::vector<llvm::Type *> fieldTypes(fn->captures.size(), ptrTy);
    auto *envTy = llvm::StructType::get(context, fieldTypes);
    for (size_t i = 0; i < fn->captures.size(); i++) {
      llvm::Value *fieldPtr = builder.CreateStructGEP(envTy, envArg, static_cast<unsigned>(i));
      llvm::Value *cellPtr = builder.CreateLoad(ptrTy, fieldPtr);
      llvm::AllocaInst *slot = CreateEntryAlloca(thunkFn, ptrTy, fn->captures[i].name);
      builder.CreateStore(cellPtr, slot);
      Declare(fn->captures[i].name, slot, MapType(fn->captures[i].type), /*isBoxed=*/true);
    }
  }

  for (auto &p : fn->params) DeclareParamBinding(thunkFn, p, &*argIt++);

  GenStmt(fn->body.get());
  // A closure is always void (Sema enforces this in its FunctionExpr
  // case) - no Unreachable-vs-RetVoid split to make the way GenFunction's
  // own tail needs for a possibly-non-void function.
  if (!builder.GetInsertBlock()->getTerminator()) builder.CreateRetVoid();

  PopScope();
  currentFunction = nullptr;
}

// ---------------------------------------------------------------------
// Statements
// ---------------------------------------------------------------------

// See this method's own doc comment in codegen.h.
void Codegen::GenRethrow(llvm::Value *errorVal) {
  llvm::Type *ptrTy = llvm::PointerType::get(context, 0);
  llvm::Value *handler = builder.CreateLoad(ptrTy, exceptionCurrentHandler);
  llvm::Value *hasHandler = builder.CreateICmpNE(handler, llvm::ConstantPointerNull::get(llvm::PointerType::get(context, 0)));

  auto *handledBB = llvm::BasicBlock::Create(context, "rethrow.handled", currentFunction);
  auto *uncaughtBB = llvm::BasicBlock::Create(context, "rethrow.uncaught", currentFunction);
  builder.CreateCondBr(hasHandler, handledBB, uncaughtBB);

  // No active handler anywhere - matches real JS's "an uncaught
  // exception terminates the program" (no message printed yet - a
  // real follow-up, not attempted here to keep this v1's actual
  // control-flow mechanism the sole focus).
  builder.SetInsertPoint(uncaughtBB);
  builder.CreateCall(abortFn, {});
  builder.CreateUnreachable();

  builder.SetInsertPoint(handledBB);
  llvm::StructType *frameTy = GetExceptionFrameType();
  // Pop BEFORE jumping - once the target handler is entered, it is no
  // longer "inside" its own try (matches GenTryCatchOnly's own Try case,
  // which does NOT re-pop on the catch path, since this already did).
  llvm::Value *prevFieldPtr = builder.CreateStructGEP(frameTy, handler, 1);
  llvm::Value *prev = builder.CreateLoad(ptrTy, prevFieldPtr);
  builder.CreateStore(prev, exceptionCurrentHandler);
  llvm::Value *thrownFieldPtr = builder.CreateStructGEP(frameTy, handler, 2);
  builder.CreateStore(errorVal, thrownFieldPtr);
  llvm::Value *jmpBufPtr = builder.CreateStructGEP(frameTy, handler, 0);
  builder.CreateCall(longjmpFn, {jmpBufPtr, llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 1)});
  builder.CreateUnreachable();
}

// See this method's own doc comment in codegen.h.
void Codegen::GenTryCatchOnly(Stmt *stmt) {
  llvm::StructType *frameTy = GetExceptionFrameType();
  llvm::Type *ptrTy = llvm::PointerType::get(context, 0);
  // Entry-block alloca, same convention every other local already
  // uses - correct even under recursion or a try inside a loop, since
  // `alloca` allocates fresh stack space on every actual call/iteration
  // of the function that contains it; there's exactly one active copy
  // of this frame per dynamically-active try, never shared.
  llvm::AllocaInst *frame = CreateEntryAlloca(currentFunction, frameTy, "try.frame");

  // Push: save the currently-active handler into this frame's own
  // "prev" field, then make this frame the new active one.
  llvm::Value *prevHandler = builder.CreateLoad(ptrTy, exceptionCurrentHandler);
  llvm::Value *prevFieldPtr = builder.CreateStructGEP(frameTy, frame, 1);
  builder.CreateStore(prevHandler, prevFieldPtr);
  builder.CreateStore(frame, exceptionCurrentHandler);

  llvm::Value *jmpBufPtr = builder.CreateStructGEP(frameTy, frame, 0);
  llvm::Value *rc = builder.CreateCall(setjmpFn, {jmpBufPtr});
  llvm::Value *isFirstEntry = builder.CreateICmpEQ(rc, llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0));

  auto *tryBB = llvm::BasicBlock::Create(context, "try.body", currentFunction);
  auto *catchBB = llvm::BasicBlock::Create(context, "try.catch", currentFunction);
  auto *endBB = llvm::BasicBlock::Create(context, "try.end", currentFunction);
  builder.CreateCondBr(isFirstEntry, tryBB, catchBB);

  builder.SetInsertPoint(tryBB);
  scopeStack.push_back({.kind = ScopeExit::Kind::Try, .framePtr = frame});
  GenStmt(stmt->body.get());
  scopeStack.pop_back();
  if (!builder.GetInsertBlock()->getTerminator()) {
    // Normal completion (no exception, no early exit - both of those
    // are handled elsewhere: GenExceptionHandlerCleanup for an early
    // exit, GenStmt's own Throw case for an exception). Restore the
    // handler that was active before this try, so a later, unrelated
    // try/throw doesn't see this now-inactive frame.
    builder.CreateStore(prevHandler, exceptionCurrentHandler);
    builder.CreateBr(endBB);
  }

  builder.SetInsertPoint(catchBB);
  // The handler was already popped by GenRethrow before the longjmp that
  // resumed execution here fired - nothing to restore on this path.
  llvm::Value *thrownFieldPtr = builder.CreateStructGEP(frameTy, frame, 2);
  llvm::Value *thrownVal = builder.CreateLoad(ptrTy, thrownFieldPtr);
  llvm::Type *catchVarTy = MapType(stmt->resolvedVarType);
  // Same boxed-vs-plain split every other local declaration already
  // has (see GenStmt's VarDecl case) - a closure capturing the caught
  // exception variable needs a real heap cell, not a stack slot that's
  // about to go out of scope.
  if (stmt->isCapturedByClosure) {
    llvm::AllocaInst *slot = CreateEntryAlloca(currentFunction, ptrTy, stmt->varName);
    uint64_t bytes = module->getDataLayout().getTypeAllocSize(catchVarTy).getFixedValue();
    llvm::Value *cellPtr = GenHeapAlloc(bytes);
    builder.CreateStore(thrownVal, cellPtr);
    builder.CreateStore(cellPtr, slot);
    Declare(stmt->varName, slot, catchVarTy, /*isBoxed=*/true);
  } else {
    llvm::AllocaInst *alloca = CreateEntryAlloca(currentFunction, catchVarTy, stmt->varName);
    builder.CreateStore(thrownVal, alloca);
    Declare(stmt->varName, alloca, catchVarTy);
  }
  GenStmt(stmt->elseBranch.get());
  if (!builder.GetInsertBlock()->getTerminator()) builder.CreateBr(endBB);

  builder.SetInsertPoint(endBB);
}

// See this method's own doc comment in codegen.h.
void Codegen::GenTryFinallyOnly(Stmt *stmt) {
  llvm::StructType *frameTy = GetExceptionFrameType();
  llvm::Type *ptrTy = llvm::PointerType::get(context, 0);
  llvm::AllocaInst *frame = CreateEntryAlloca(currentFunction, frameTy, "try.frame");

  llvm::Value *prevHandler = builder.CreateLoad(ptrTy, exceptionCurrentHandler);
  builder.CreateStore(prevHandler, builder.CreateStructGEP(frameTy, frame, 1));
  builder.CreateStore(frame, exceptionCurrentHandler);

  llvm::Value *rc = builder.CreateCall(setjmpFn, {builder.CreateStructGEP(frameTy, frame, 0)});
  llvm::Value *isFirstEntry = builder.CreateICmpEQ(rc, llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0));

  auto *tryBB = llvm::BasicBlock::Create(context, "try.body", currentFunction);
  auto *catchBB = llvm::BasicBlock::Create(context, "try.catch", currentFunction);
  auto *endBB = llvm::BasicBlock::Create(context, "try.end", currentFunction);
  builder.CreateCondBr(isFirstEntry, tryBB, catchBB);

  builder.SetInsertPoint(tryBB);
  scopeStack.push_back({.kind = ScopeExit::Kind::Try, .framePtr = frame, .finallyBody = stmt->finallyBody.get()});
  GenStmt(stmt->body.get());
  scopeStack.pop_back();
  if (!builder.GetInsertBlock()->getTerminator()) {
    builder.CreateStore(prevHandler, exceptionCurrentHandler);
    GenStmt(stmt->finallyBody.get());
    if (!builder.GetInsertBlock()->getTerminator()) builder.CreateBr(endBB);
  }

  builder.SetInsertPoint(catchBB);
  // No catch clause of its own - just run the finally, then keep
  // propagating whatever was thrown (GenRethrow already loads the
  // now-correctly-restored `@art.exception.currentHandler` - see its own
  // doc comment - so this needs no explicit pop of its own, same as
  // GenTryCatchOnly's own catch path).
  llvm::Value *thrownVal = builder.CreateLoad(ptrTy, builder.CreateStructGEP(frameTy, frame, 2));
  GenStmt(stmt->finallyBody.get());
  if (!builder.GetInsertBlock()->getTerminator()) GenRethrow(thrownVal);

  builder.SetInsertPoint(endBB);
}

// See this method's own doc comment in codegen.h.
void Codegen::GenTryWithFinallyAndCatch(Stmt *stmt) {
  llvm::StructType *frameTy = GetExceptionFrameType();
  llvm::Type *ptrTy = llvm::PointerType::get(context, 0);
  llvm::AllocaInst *outerFrame = CreateEntryAlloca(currentFunction, frameTy, "try.outer.frame");

  llvm::Value *prevHandler = builder.CreateLoad(ptrTy, exceptionCurrentHandler);
  builder.CreateStore(prevHandler, builder.CreateStructGEP(frameTy, outerFrame, 1));
  builder.CreateStore(outerFrame, exceptionCurrentHandler);

  llvm::Value *rc = builder.CreateCall(setjmpFn, {builder.CreateStructGEP(frameTy, outerFrame, 0)});
  llvm::Value *isFirstEntry = builder.CreateICmpEQ(rc, llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0));

  auto *outerTryBB = llvm::BasicBlock::Create(context, "try.outer.body", currentFunction);
  auto *outerCatchBB = llvm::BasicBlock::Create(context, "try.outer.catch", currentFunction);
  auto *outerEndBB = llvm::BasicBlock::Create(context, "try.outer.end", currentFunction);
  builder.CreateCondBr(isFirstEntry, outerTryBB, outerCatchBB);

  builder.SetInsertPoint(outerTryBB);
  // Covers BOTH the try body and the catch body (GenTryCatchOnly's own
  // inner ScopeExit only ever covers the try body itself, matching the
  // existing "the handler is already popped before the catch body even
  // runs" semantics) - an early return/break/continue from EITHER one
  // needs this outer frame's finally to run exactly once on its way out.
  scopeStack.push_back({.kind = ScopeExit::Kind::Try, .framePtr = outerFrame, .finallyBody = stmt->finallyBody.get()});
  GenTryCatchOnly(stmt); // unmodified - its own separate inner frame
  scopeStack.pop_back();
  if (!builder.GetInsertBlock()->getTerminator()) {
    builder.CreateStore(prevHandler, exceptionCurrentHandler);
    GenStmt(stmt->finallyBody.get());
    if (!builder.GetInsertBlock()->getTerminator()) builder.CreateBr(outerEndBB);
  }

  builder.SetInsertPoint(outerCatchBB);
  // Reached only if the INNER catch body itself threw - the inner try
  // body's own throw is already fully handled inside GenTryCatchOnly,
  // without ever escaping to this outer frame at all.
  llvm::Value *rethrowVal = builder.CreateLoad(ptrTy, builder.CreateStructGEP(frameTy, outerFrame, 2));
  GenStmt(stmt->finallyBody.get());
  if (!builder.GetInsertBlock()->getTerminator()) GenRethrow(rethrowVal);

  builder.SetInsertPoint(outerEndBB);
}

void Codegen::GenStmt(Stmt *stmt) {
  switch (stmt->kind) {
  case StmtKind::VarDecl: {
    llvm::Type *ty = MapType(stmt->resolvedVarType);
    llvm::Value *val = GenExpr(stmt->expr.get());
    if (stmt->isCapturedByClosure) {
      // Boxed: `slot` (an ordinary, entry-block, mem2reg-friendly ptr
      // alloca) always holds the CURRENT cell's address, but the cell
      // itself - and the store into it - is allocated right here, at
      // this statement's own position in the CFG, not hoisted. So a
      // `let` inside a loop *body* naturally gets a fresh cell every
      // dynamic execution (every iteration re-runs this GenStmt call) -
      // a closure created in one iteration copies that iteration's cell
      // address into its own env and never reads `slot` again, so a
      // later iteration overwriting `slot` doesn't affect it. See
      // VarBinding::isBoxed's own doc comment.
      llvm::Type *ptrTy = llvm::PointerType::get(context, 0);
      llvm::AllocaInst *slot = CreateEntryAlloca(currentFunction, ptrTy, stmt->varName);
      uint64_t bytes = module->getDataLayout().getTypeAllocSize(ty).getFixedValue();
      llvm::Value *cellPtr = GenHeapAlloc(bytes);
      builder.CreateStore(val, cellPtr);
      builder.CreateStore(cellPtr, slot);
      Declare(stmt->varName, slot, ty, /*isBoxed=*/true);
    } else {
      llvm::AllocaInst *alloca = CreateEntryAlloca(currentFunction, ty, stmt->varName);
      builder.CreateStore(val, alloca);
      Declare(stmt->varName, alloca, ty);
    }
    break;
  }

  case StmtKind::If: {
    llvm::Value *condVal = GenExpr(stmt->cond.get());
    auto *thenBB = llvm::BasicBlock::Create(context, "if.then", currentFunction);
    auto *elseBB = stmt->elseBranch ? llvm::BasicBlock::Create(context, "if.else", currentFunction) : nullptr;
    auto *mergeBB = llvm::BasicBlock::Create(context, "if.end", currentFunction);

    builder.CreateCondBr(condVal, thenBB, elseBB ? elseBB : mergeBB);

    builder.SetInsertPoint(thenBB);
    GenStmt(stmt->body.get());
    if (!builder.GetInsertBlock()->getTerminator()) builder.CreateBr(mergeBB);

    if (elseBB) {
      builder.SetInsertPoint(elseBB);
      GenStmt(stmt->elseBranch.get());
      if (!builder.GetInsertBlock()->getTerminator()) builder.CreateBr(mergeBB);
    }

    builder.SetInsertPoint(mergeBB);
    break;
  }

  case StmtKind::While: {
    auto *condBB = llvm::BasicBlock::Create(context, "while.cond", currentFunction);
    auto *bodyBB = llvm::BasicBlock::Create(context, "while.body", currentFunction);
    auto *endBB = llvm::BasicBlock::Create(context, "while.end", currentFunction);

    builder.CreateBr(condBB);
    builder.SetInsertPoint(condBB);
    llvm::Value *condVal = GenExpr(stmt->cond.get());
    builder.CreateCondBr(condVal, bodyBB, endBB);

    builder.SetInsertPoint(bodyBB);
    scopeStack.push_back({.kind = ScopeExit::Kind::Loop, .breakTarget = endBB, .continueTarget = condBB});
    GenStmt(stmt->body.get());
    scopeStack.pop_back();
    if (!builder.GetInsertBlock()->getTerminator()) builder.CreateBr(condBB);

    builder.SetInsertPoint(endBB);
    break;
  }

  case StmtKind::DoWhile: {
    auto *bodyBB = llvm::BasicBlock::Create(context, "dowhile.body", currentFunction);
    auto *condBB = llvm::BasicBlock::Create(context, "dowhile.cond", currentFunction);
    auto *endBB = llvm::BasicBlock::Create(context, "dowhile.end", currentFunction);

    // Unlike While, the body runs unconditionally the first time - the
    // condition isn't checked until after it (see ast.h's own doc
    // comment on StmtKind::DoWhile).
    builder.CreateBr(bodyBB);
    builder.SetInsertPoint(bodyBB);
    // `continue` re-checks the condition, same as While - it doesn't
    // re-run the body unconditionally the way the very first entry does.
    scopeStack.push_back({.kind = ScopeExit::Kind::Loop, .breakTarget = endBB, .continueTarget = condBB});
    GenStmt(stmt->body.get());
    scopeStack.pop_back();
    if (!builder.GetInsertBlock()->getTerminator()) builder.CreateBr(condBB);

    builder.SetInsertPoint(condBB);
    llvm::Value *condVal = GenExpr(stmt->cond.get());
    builder.CreateCondBr(condVal, bodyBB, endBB);

    builder.SetInsertPoint(endBB);
    break;
  }

  case StmtKind::For: {
    PushScope();
    if (stmt->initStmt) GenStmt(stmt->initStmt.get());

    auto *condBB = llvm::BasicBlock::Create(context, "for.cond", currentFunction);
    auto *bodyBB = llvm::BasicBlock::Create(context, "for.body", currentFunction);
    auto *updateBB = llvm::BasicBlock::Create(context, "for.update", currentFunction);
    auto *endBB = llvm::BasicBlock::Create(context, "for.end", currentFunction);

    builder.CreateBr(condBB);
    builder.SetInsertPoint(condBB);
    llvm::Value *condVal =
        stmt->cond ? GenExpr(stmt->cond.get()) : llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), 1);
    builder.CreateCondBr(condVal, bodyBB, endBB);

    builder.SetInsertPoint(bodyBB);
    // `continue` goes to updateBB (not condBB) - a real for-loop's
    // update still has to run before the condition is re-checked, unlike
    // While/DoWhile where there's no separate update step at all.
    scopeStack.push_back({.kind = ScopeExit::Kind::Loop, .breakTarget = endBB, .continueTarget = updateBB});
    GenStmt(stmt->body.get());
    scopeStack.pop_back();
    if (!builder.GetInsertBlock()->getTerminator()) builder.CreateBr(updateBB);

    builder.SetInsertPoint(updateBB);
    if (stmt->update) GenExpr(stmt->update.get());
    builder.CreateBr(condBB);

    builder.SetInsertPoint(endBB);
    PopScope();
    break;
  }

  case StmtKind::ForOf: {
    PushScope();
    llvm::Value *arrPtr = GenExpr(stmt->expr.get());
    llvm::StructType *hdrTy = GetArrayHeaderType();
    llvm::Type *i64Ty = llvm::Type::getInt64Ty(context);
    llvm::Value *length = builder.CreateLoad(i64Ty, builder.CreateStructGEP(hdrTy, arrPtr, 0));
    llvm::Value *dataPtr = builder.CreateLoad(llvm::PointerType::get(context, 0), builder.CreateStructGEP(hdrTy, arrPtr, 1));

    const ResolvedType &elemType = stmt->resolvedVarType;
    llvm::Type *storageTy = ArrayElemStorageType(elemType);
    llvm::Type *varTy = MapType(elemType);
    llvm::Type *ptrTy = llvm::PointerType::get(context, 0);

    llvm::AllocaInst *idxAlloca = CreateEntryAlloca(currentFunction, i64Ty, "forof.idx");
    builder.CreateStore(llvm::ConstantInt::get(i64Ty, 0), idxAlloca);
    // Boxed: this slot always holds the CURRENT iteration's cell address
    // - a fresh cell is allocated inside the loop body below, every
    // dynamic pass through it (i.e. every iteration, since a real
    // for...of iteration variable is already conceptually a fresh
    // per-iteration binding) - same pattern/reasoning as GenStmt's own
    // VarDecl case.
    llvm::AllocaInst *varAlloca =
        CreateEntryAlloca(currentFunction, stmt->isCapturedByClosure ? ptrTy : varTy, stmt->varName);

    auto *condBB = llvm::BasicBlock::Create(context, "forof.cond", currentFunction);
    auto *bodyBB = llvm::BasicBlock::Create(context, "forof.body", currentFunction);
    auto *updateBB = llvm::BasicBlock::Create(context, "forof.update", currentFunction);
    auto *endBB = llvm::BasicBlock::Create(context, "forof.end", currentFunction);

    builder.CreateBr(condBB);
    builder.SetInsertPoint(condBB);
    llvm::Value *idx = builder.CreateLoad(i64Ty, idxAlloca);
    builder.CreateCondBr(builder.CreateICmpSLT(idx, length), bodyBB, endBB);

    builder.SetInsertPoint(bodyBB);
    llvm::Value *elemPtr = builder.CreateGEP(storageTy, dataPtr, {idx});
    llvm::Value *rawVal = builder.CreateLoad(storageTy, elemPtr);
    llvm::Value *elemVal = rawVal;
    if (elemType.tag == TypeTag::Boolean) {
      elemVal = builder.CreateICmpNE(rawVal, llvm::ConstantInt::get(llvm::Type::getInt8Ty(context), 0));
    }
    if (stmt->isCapturedByClosure) {
      uint64_t bytes = module->getDataLayout().getTypeAllocSize(varTy).getFixedValue();
      llvm::Value *cellPtr = GenHeapAlloc(bytes);
      builder.CreateStore(elemVal, cellPtr);
      builder.CreateStore(cellPtr, varAlloca);
      Declare(stmt->varName, varAlloca, varTy, /*isBoxed=*/true);
    } else {
      builder.CreateStore(elemVal, varAlloca);
      Declare(stmt->varName, varAlloca, varTy);
    }
    scopeStack.push_back({.kind = ScopeExit::Kind::Loop, .breakTarget = endBB, .continueTarget = updateBB});
    GenStmt(stmt->body.get());
    scopeStack.pop_back();
    if (!builder.GetInsertBlock()->getTerminator()) builder.CreateBr(updateBB);

    builder.SetInsertPoint(updateBB);
    llvm::Value *nextIdx = builder.CreateAdd(builder.CreateLoad(i64Ty, idxAlloca), llvm::ConstantInt::get(i64Ty, 1));
    builder.CreateStore(nextIdx, idxAlloca);
    builder.CreateBr(condBB);

    builder.SetInsertPoint(endBB);
    PopScope();
    break;
  }

  case StmtKind::Return: {
    // The return VALUE (if any) is evaluated first, while every
    // currently-active try handler is still genuinely active - if
    // evaluating it throws (a call to something that itself throws), that
    // exception has to be caught by whatever's active at THIS point, and
    // this return never actually completes at all (control already
    // jumped away via longjmp - the cleanup/CreateRet below is simply
    // never reached on that path). Only once it's evaluated
    // successfully does this function's own try frames actually need
    // popping (see GenExceptionHandlerCleanup) - stopBelow 0 pops every
    // one still active, the whole current function being exited.
    if (stmt->expr) {
      llvm::Value *val = GenExpr(stmt->expr.get());
      GenExceptionHandlerCleanup(0);
      builder.CreateRet(val);
    } else {
      GenExceptionHandlerCleanup(0);
      builder.CreateRetVoid();
    }
    break;
  }

  case StmtKind::Break: {
    // Sema (loopDepth/switchDepth) already proved this is inside a real
    // loop/switch, so a Loop-kind frame is guaranteed to exist somewhere
    // on scopeStack - break always means "the nearest enclosing one".
    // Every try frame ABOVE it (nested inside it, between it and this
    // break) needs popping on the way out - see GenExceptionHandlerCleanup.
    size_t targetIdx = scopeStack.size();
    for (size_t i = scopeStack.size(); i-- > 0;) {
      if (scopeStack[i].kind == ScopeExit::Kind::Loop) {
        targetIdx = i;
        break;
      }
    }
    GenExceptionHandlerCleanup(targetIdx + 1);
    builder.CreateBr(scopeStack[targetIdx].breakTarget);
    break;
  }

  case StmtKind::Continue: {
    // Unlike Break, this searches for a Loop-kind frame that actually has
    // a continueTarget instead of always using the nearest Loop-kind one
    // - a switch frame's own continueTarget is null (see ScopeExit's own
    // doc comment), so `continue` inside a switch skips right past it to
    // the nearest enclosing LOOP, matching real JS. Sema (loopDepth > 0)
    // already proved one exists somewhere on this stack. Every try frame
    // ABOVE the target also needs popping, same as Break.
    size_t targetIdx = scopeStack.size();
    for (size_t i = scopeStack.size(); i-- > 0;) {
      if (scopeStack[i].kind == ScopeExit::Kind::Loop && scopeStack[i].continueTarget) {
        targetIdx = i;
        break;
      }
    }
    GenExceptionHandlerCleanup(targetIdx + 1);
    builder.CreateBr(scopeStack[targetIdx].continueTarget);
    break;
  }

  case StmtKind::Switch: {
    llvm::Value *discriminant = GenExpr(stmt->expr.get());
    TypeTag discTag = stmt->expr->resolvedType.tag;

    auto *endBB = llvm::BasicBlock::Create(context, "switch.end", currentFunction);
    size_t n = stmt->statements.size();
    std::vector<llvm::BasicBlock *> bodyBBs(n);
    llvm::BasicBlock *defaultBodyBB = nullptr;
    for (size_t i = 0; i < n; i++) {
      bodyBBs[i] = llvm::BasicBlock::Create(context, "switch.arm" + std::to_string(i), currentFunction);
      if (!stmt->statements[i]->expr) defaultBodyBB = bodyBBs[i]; // `default:` arm
    }
    // No case at all (only `default:`, or an entirely empty switch) is
    // legal - the fallback below still correctly targets endBB in that
    // case, same as a switch whose discriminant matches nothing.
    llvm::BasicBlock *noMatchTarget = defaultBodyBB ? defaultBodyBB : endBB;

    // A chain of test blocks, one per `case` arm (in source order,
    // `default:` skipped - it has no value of its own to test, it's only
    // ever reached as the final fallback above, or by falling through
    // from an earlier arm below). Each false edge falls through to the
    // NEXT case's own test, so the whole chain reads top to bottom the
    // same way a real if/else-if chain would.
    std::vector<llvm::BasicBlock *> testBBs;
    for (size_t i = 0; i < n; i++) {
      if (!stmt->statements[i]->expr) continue;
      testBBs.push_back(llvm::BasicBlock::Create(context, "switch.test" + std::to_string(i), currentFunction));
    }

    builder.CreateBr(testBBs.empty() ? noMatchTarget : testBBs[0]);
    size_t testIdx = 0;
    for (size_t i = 0; i < n; i++) {
      if (!stmt->statements[i]->expr) continue;
      builder.SetInsertPoint(testBBs[testIdx]);
      llvm::Value *caseVal = GenExpr(stmt->statements[i]->expr.get());
      llvm::Value *matches = GenEqualityCheck(discriminant, caseVal, discTag);
      llvm::BasicBlock *onFalse = (testIdx + 1 < testBBs.size()) ? testBBs[testIdx + 1] : noMatchTarget;
      builder.CreateCondBr(matches, bodyBBs[i], onFalse);
      testIdx++;
    }

    // Arm bodies, in source order - each one falls through into the
    // NEXT arm (real switch fallthrough, matching JS) unless it already
    // ends in a terminator (a `break`, `return`, or another already-
    // exhaustive control-flow statement).
    scopeStack.push_back({.kind = ScopeExit::Kind::Loop, .breakTarget = endBB}); // continueTarget left null - see ScopeExit's own doc comment
    for (size_t i = 0; i < n; i++) {
      builder.SetInsertPoint(bodyBBs[i]);
      for (auto &s : stmt->statements[i]->statements) {
        GenStmt(s.get());
        if (builder.GetInsertBlock()->getTerminator()) break; // rest is unreachable
      }
      if (!builder.GetInsertBlock()->getTerminator()) {
        builder.CreateBr(i + 1 < n ? bodyBBs[i + 1] : endBB);
      }
    }
    scopeStack.pop_back();

    builder.SetInsertPoint(endBB);
    break;
  }

  case StmtKind::Case:
    // Never generated directly - StmtKind::Switch's own case above
    // always reaches an arm's statements through `stmt->statements[i]`
    // itself (a Case node), not through a recursive GenStmt call on the
    // Case node. Unreachable in practice; see Sema's own identical
    // fallback (CheckStmt's StmtKind::Case case) for why this is still
    // handled rather than omitted.
    for (auto &s : stmt->statements) GenStmt(s.get());
    break;

  case StmtKind::Destructure: {
    // The source struct is evaluated exactly once, regardless of how
    // many bindings read from it - same field-GEP pattern GenLValue's
    // own Member case already uses for an ordinary `obj.field` read.
    llvm::Value *objPtr = GenExpr(stmt->expr.get());
    const std::string &ifaceName = stmt->expr->resolvedType.structName;
    llvm::StructType *structTy = GetOrCreateStructType(ifaceName);
    InterfaceDecl *iface = sema.Interfaces().at(ifaceName);
    for (auto &b : stmt->destructureBindings) {
      int idx = FieldIndex(iface, b.fieldName);
      llvm::Type *fieldTy = MapType(iface->fields[idx].resolvedType);
      llvm::Value *fieldPtr = builder.CreateStructGEP(structTy, objPtr, StructFieldGepIndex(iface, idx));
      llvm::Value *val = builder.CreateLoad(fieldTy, fieldPtr);
      // isCapturedByClosure lives on the whole Destructure Stmt, not
      // per-binding (see its own doc comment on Stmt) - if ANY binding
      // from this pattern is captured, every one of them is boxed, even
      // ones that individually aren't. Safe (boxing is never wrong, just
      // occasionally unnecessary), and far simpler than threading
      // per-binding capture tracking through Sema's Declare/Lookup,
      // which are built around one binding per declaration site.
      if (stmt->isCapturedByClosure) {
        llvm::Type *ptrTy = llvm::PointerType::get(context, 0);
        llvm::AllocaInst *slot = CreateEntryAlloca(currentFunction, ptrTy, b.localName);
        uint64_t bytes = module->getDataLayout().getTypeAllocSize(fieldTy).getFixedValue();
        llvm::Value *cellPtr = GenHeapAlloc(bytes);
        builder.CreateStore(val, cellPtr);
        builder.CreateStore(cellPtr, slot);
        Declare(b.localName, slot, fieldTy, /*isBoxed=*/true);
      } else {
        llvm::AllocaInst *alloca = CreateEntryAlloca(currentFunction, fieldTy, b.localName);
        builder.CreateStore(val, alloca);
        Declare(b.localName, alloca, fieldTy);
      }
    }
    break;
  }

  case StmtKind::Try: {
    // See each helper's own doc comment in codegen.h for exactly why the
    // finally-and-catch combination needs two real handler frames while
    // the other two shapes need at most one.
    bool hasCatch = stmt->declaredType != nullptr;
    bool hasFinally = stmt->finallyBody != nullptr;
    if (hasFinally && hasCatch) {
      GenTryWithFinallyAndCatch(stmt);
    } else if (hasFinally) {
      GenTryFinallyOnly(stmt);
    } else {
      GenTryCatchOnly(stmt);
    }
    break;
  }

  case StmtKind::Throw: {
    llvm::Value *errorVal = GenExpr(stmt->expr.get()); // an Error* (ptr)
    GenRethrow(errorVal);
    break;
  }

  case StmtKind::ExprStmt:
    GenExpr(stmt->expr.get());
    break;

  case StmtKind::Block:
    PushScope();
    for (auto &s : stmt->statements) {
      GenStmt(s.get());
      if (builder.GetInsertBlock()->getTerminator()) break; // rest is unreachable
    }
    PopScope();
    break;
  }
}

// ---------------------------------------------------------------------
// Lvalues (assignment targets, and read access to Index/Member re-uses this)
// ---------------------------------------------------------------------

Codegen::LValue Codegen::GenLValue(Expr *expr) {
  if (expr->kind == ExprKind::Identifier) {
    VarBinding *b = Lookup(expr->name);
    if (b->isBoxed) {
      // The variable's own storage slot holds a `ptr` to its cell (see
      // VarBinding::isBoxed's own doc comment) - the lvalue's real
      // address is the cell itself, one indirection past the slot.
      llvm::Value *cellPtr = builder.CreateLoad(llvm::PointerType::get(context, 0), b->alloca);
      return {cellPtr, b->type, false};
    }
    return {b->alloca, b->type, false};
  }

  if (expr->kind == ExprKind::Index) {
    llvm::Value *objPtr = GenExpr(expr->lhs.get());
    llvm::Value *idxVal = GenExpr(expr->operand.get());
    llvm::Value *idxInt = builder.CreateFPToSI(idxVal, llvm::Type::getInt64Ty(context));

    llvm::Value *dataFieldPtr = builder.CreateStructGEP(GetArrayHeaderType(), objPtr, 1);
    llvm::Value *dataPtr = builder.CreateLoad(llvm::PointerType::get(context, 0), dataFieldPtr);

    const ResolvedType &elemType = *expr->lhs->resolvedType.elementType;
    llvm::Type *storageTy = ArrayElemStorageType(elemType);
    llvm::Value *elemPtr = builder.CreateGEP(storageTy, dataPtr, {idxInt});
    return {elemPtr, storageTy, elemType.tag == TypeTag::Boolean};
  }

  // Member (struct field only - `.length` is read-only and never reaches GenLValue).
  llvm::Value *objPtr = GenExpr(expr->lhs.get());
  const std::string &ifaceName = expr->lhs->resolvedType.structName;
  llvm::StructType *structTy = GetOrCreateStructType(ifaceName);
  InterfaceDecl *iface = sema.Interfaces().at(ifaceName);
  int idx = FieldIndex(iface, expr->name);
  llvm::Value *fieldPtr = builder.CreateStructGEP(structTy, objPtr, StructFieldGepIndex(iface, idx));
  return {fieldPtr, MapType(iface->fields[idx].resolvedType), false};
}

// ---------------------------------------------------------------------
// Expressions
// ---------------------------------------------------------------------

llvm::Value *Codegen::GenExprInner(Expr *expr) {
  switch (expr->kind) {
  case ExprKind::NumberLiteral:
    return llvm::ConstantFP::get(llvm::Type::getDoubleTy(context), expr->numberValue);

  case ExprKind::BoolLiteral:
    return llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), expr->boolValue ? 1 : 0);

  case ExprKind::StringLiteral:
    return GenStringLiteral(expr->name);

  case ExprKind::NullLiteral:
    if (expr->resolvedType.tag == TypeTag::Any) {
      // `any`'s own `null` needs a REAL box (tag AnyTag::Null) - unlike
      // Nullable(T)'s bare null pointer below, `typeof` still has to read
      // an actual tag byte out of this, and a bare null pointer has
      // nothing to read (see Sema::CheckExpr's own NullLiteral case).
      llvm::StructType *boxTy = GetAnyBoxType();
      llvm::Value *box = GenHeapAlloc(module->getDataLayout().getTypeAllocSize(boxTy).getFixedValue());
      builder.CreateStore(
          llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), static_cast<uint64_t>(AnyTag::Null)),
          builder.CreateStructGEP(boxTy, box, 0));
      builder.CreateStore(llvm::ConstantPointerNull::get(llvm::PointerType::get(context, 0)),
                           builder.CreateStructGEP(boxTy, box, 1));
      return box;
    }
    // The boxed representation's own "absent" value (see TypeTag::
    // Nullable's own doc comment) - a plain null pointer, regardless of
    // what the wrapped T actually is.
    return llvm::ConstantPointerNull::get(llvm::PointerType::get(context, 0));

  case ExprKind::Identifier: {
    if (expr->isNarrowedNonNull) {
      // Narrowed (see Expr::isNarrowedNonNull's own doc comment): the
      // variable's own storage is still the boxed `ptr` a Nullable(T)
      // always is (see TypeTag::Nullable's own doc comment) regardless
      // of what T itself is - one more load, through that pointer, is
      // what actually turns "the box" into "the T inside it".
      // Deliberately checked and handled BEFORE the ordinary
      // Handler-value branch below: if T itself happens to be Handler
      // (a nullable handler variable), that branch's own assumption -
      // the variable's OWN declared type already IS Handler, not
      // Nullable(Handler) - would be wrong here.
      llvm::Value *boxPtr = LoadVar(Lookup(expr->name), expr->name);
      return builder.CreateLoad(MapType(expr->resolvedType), boxPtr);
    }
    if (expr->isNarrowedAny) {
      // Narrowed via `typeof` (see Expr::isNarrowedAny's own doc
      // comment): the variable's own storage is still an `any` box - load
      // its `payload` field, then unwrap according to whichever of the
      // three narrowable concrete types `expr->resolvedType` actually is
      // now (see GenBoxAny for the inverse - how each of these three got
      // stored in the first place). String needs no further unwrapping:
      // its payload IS the string's own pointer already (see GenBoxAny's
      // own String case).
      llvm::Value *boxPtr = LoadVar(Lookup(expr->name), expr->name);
      llvm::Value *payload =
          builder.CreateLoad(llvm::PointerType::get(context, 0), builder.CreateStructGEP(GetAnyBoxType(), boxPtr, 1));
      switch (expr->resolvedType.tag) {
      case TypeTag::Number:
        return builder.CreateLoad(llvm::Type::getDoubleTy(context), payload);
      case TypeTag::Boolean: {
        llvm::Value *byte = builder.CreateLoad(llvm::Type::getInt8Ty(context), payload);
        return builder.CreateICmpNE(byte, llvm::ConstantInt::get(llvm::Type::getInt8Ty(context), 0));
      }
      case TypeTag::String:
      default:
        return payload;
      }
    }
    if (expr->resolvedType.tag == TypeTag::Handler) {
      // A Handler-typed Identifier is either a real local/global variable
      // holding a Handler value (e.g. a parameter forwarded into another
      // call - `function wrap(h: () => void): void { other(h); }`) or a
      // bare reference to a top-level function used as a value, with no
      // variable/alloca of its own at all (see Sema::CheckExpr's
      // Identifier case - both produce the same TypeTag::Handler
      // resolvedType, so only an actual variable lookup tells them apart,
      // not the type tag alone). The latter still needs a real {fn, env}
      // aggregate value, not just the bare llvm::Function* - see
      // BuildPlainFunctionHandlerValue.
      VarBinding *b = TryLookup(expr->name);
      if (b) return LoadVar(b, expr->name);
      return BuildPlainFunctionHandlerValue(expr->name);
    }
    return LoadVar(Lookup(expr->name), expr->name);
  }

  case ExprKind::Binary: {
    const std::string &op = expr->op;

    if (op == "&&" || op == "||") {
      bool isAnd = op == "&&";
      auto *rhsBB = llvm::BasicBlock::Create(context, isAnd ? "and.rhs" : "or.rhs", currentFunction);
      auto *mergeBB = llvm::BasicBlock::Create(context, isAnd ? "and.end" : "or.end", currentFunction);

      llvm::Value *lhsVal = GenExpr(expr->lhs.get());
      llvm::BasicBlock *lhsEndBB = builder.GetInsertBlock();
      if (isAnd) {
        builder.CreateCondBr(lhsVal, rhsBB, mergeBB);
      } else {
        builder.CreateCondBr(lhsVal, mergeBB, rhsBB);
      }

      builder.SetInsertPoint(rhsBB);
      llvm::Value *rhsVal = GenExpr(expr->rhs.get());
      llvm::BasicBlock *rhsEndBB = builder.GetInsertBlock();
      builder.CreateBr(mergeBB);

      builder.SetInsertPoint(mergeBB);
      llvm::PHINode *phi = builder.CreatePHI(llvm::Type::getInt1Ty(context), 2);
      phi->addIncoming(llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), isAnd ? 0 : 1), lhsEndBB);
      phi->addIncoming(rhsVal, rhsEndBB);
      return phi;
    }

    llvm::Value *lhsVal = GenExpr(expr->lhs.get());
    llvm::Value *rhsVal = GenExpr(expr->rhs.get());
    TypeTag operandTag = expr->lhs->resolvedType.tag;

    if (operandTag == TypeTag::String) {
      if (op == "+") return GenStringConcat(lhsVal, rhsVal);
      if (op == "==") return GenEqualityCheck(lhsVal, rhsVal, operandTag);
      if (op == "!=") return builder.CreateNot(GenEqualityCheck(lhsVal, rhsVal, operandTag));
      throw std::runtime_error("codegen: unsupported string operator '" + op + "'");
    }

    if (op == "+") return builder.CreateFAdd(lhsVal, rhsVal);
    if (op == "-") return builder.CreateFSub(lhsVal, rhsVal);
    if (op == "*") return builder.CreateFMul(lhsVal, rhsVal);
    if (op == "/") return builder.CreateFDiv(lhsVal, rhsVal);
    if (op == "%") return builder.CreateFRem(lhsVal, rhsVal);
    if (op == "<") return builder.CreateFCmpOLT(lhsVal, rhsVal);
    if (op == "<=") return builder.CreateFCmpOLE(lhsVal, rhsVal);
    if (op == ">") return builder.CreateFCmpOGT(lhsVal, rhsVal);
    if (op == ">=") return builder.CreateFCmpOGE(lhsVal, rhsVal);

    // '&'/'|'/'^'/'<<'/'>>'/'>>>' - real JS bitwise/shift semantics: each
    // operand truncates to a 32-bit *signed* int (JS's ToInt32 - take the
    // low 32 bits of the double's integer value, mod 2^32, interpreted as
    // signed), the operator runs on those bits, and the i32 result widens
    // back to a double (sign-extended, so a negative result round-trips
    // correctly - e.g. ~0 is -1, not 4294967295). A shift amount is
    // additionally masked to 0-31 (`& 31`), matching JS - `1 << 32` is
    // `1`, not `0`, since only the low 5 bits of the shift count matter.
    // '>>>' is unsigned: the same bits, but zero-extended back to double
    // instead of sign-extended, so a negative operand comes back as a
    // large positive number instead of staying negative.
    if (op == "&" || op == "|" || op == "^" || op == "<<" || op == ">>" || op == ">>>") {
      llvm::Type *i32Ty = llvm::Type::getInt32Ty(context);
      llvm::Type *dblTy = llvm::Type::getDoubleTy(context);
      llvm::Value *lhsI32 = builder.CreateFPToSI(lhsVal, i32Ty);
      llvm::Value *rhsI32 = builder.CreateFPToSI(rhsVal, i32Ty);
      llvm::Value *result;
      if (op == "&") {
        result = builder.CreateAnd(lhsI32, rhsI32);
      } else if (op == "|") {
        result = builder.CreateOr(lhsI32, rhsI32);
      } else if (op == "^") {
        result = builder.CreateXor(lhsI32, rhsI32);
      } else {
        llvm::Value *shiftAmt =
            builder.CreateAnd(rhsI32, llvm::ConstantInt::get(i32Ty, 31));
        if (op == "<<") {
          result = builder.CreateShl(lhsI32, shiftAmt);
        } else if (op == ">>") {
          result = builder.CreateAShr(lhsI32, shiftAmt);
        } else { // ">>>"
          llvm::Value *lhsU32 = builder.CreateBitCast(lhsI32, i32Ty); // same bits, treated unsigned below
          result = builder.CreateLShr(lhsU32, shiftAmt);
          return builder.CreateUIToFP(result, dblTy);
        }
      }
      return builder.CreateSIToFP(result, dblTy);
    }

    // Every other valid op for every other reachable operand type
    // (Number, Boolean, Handler, Struct/Array pointers - String already
    // handled above) has already returned by this point - Sema only
    // ever allows "=="/"!=" to reach here, per its own Binary case.
    if (op == "==") return GenEqualityCheck(lhsVal, rhsVal, operandTag);
    if (op == "!=") return builder.CreateNot(GenEqualityCheck(lhsVal, rhsVal, operandTag));
    throw std::runtime_error("codegen: unknown binary operator '" + op + "'");
  }

  case ExprKind::Unary: {
    llvm::Value *operandVal = GenExpr(expr->operand.get());
    if (expr->op == "-") return builder.CreateFNeg(operandVal);
    if (expr->op == "!") return builder.CreateNot(operandVal);
    if (expr->op == "~") {
      // Same ToInt32-truncate-then-widen-back deal as the binary bitwise
      // operators - see their own codegen comment (ExprKind::Binary).
      llvm::Type *i32Ty = llvm::Type::getInt32Ty(context);
      llvm::Value *asI32 = builder.CreateFPToSI(operandVal, i32Ty);
      llvm::Value *notted = builder.CreateNot(asI32);
      return builder.CreateSIToFP(notted, llvm::Type::getDoubleTy(context));
    }
    if (expr->op == "typeof") {
      // `operandVal` is an `any` box (Sema guarantees this - see its own
      // Unary "typeof" case) - read its own tag field and pick the
      // matching string. Every candidate here is a compile-time constant
      // (GenStringLiteral has no side effects of its own - see
      // BuildStringConstant), so a plain select-chain works fine, no real
      // branching needed. AnyTag::Object and AnyTag::Null share the
      // "object" fallback (see AnyTag::Object's own doc comment).
      llvm::Value *tagVal = builder.CreateLoad(llvm::Type::getInt32Ty(context),
                                                builder.CreateStructGEP(GetAnyBoxType(), operandVal, 0));
      auto tagConst = [&](AnyTag t) {
        return llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), static_cast<uint64_t>(t));
      };
      llvm::Value *result = GenStringLiteral("object");
      result = builder.CreateSelect(builder.CreateICmpEQ(tagVal, tagConst(AnyTag::Function)),
                                     GenStringLiteral("function"), result);
      result = builder.CreateSelect(builder.CreateICmpEQ(tagVal, tagConst(AnyTag::String)),
                                     GenStringLiteral("string"), result);
      result = builder.CreateSelect(builder.CreateICmpEQ(tagVal, tagConst(AnyTag::Boolean)),
                                     GenStringLiteral("boolean"), result);
      result = builder.CreateSelect(builder.CreateICmpEQ(tagVal, tagConst(AnyTag::Number)),
                                     GenStringLiteral("number"), result);
      return result;
    }
    throw std::runtime_error("codegen: unknown unary operator '" + expr->op + "'");
  }

  case ExprKind::IncDec: {
    LValue lv = GenLValue(expr->operand.get());
    llvm::Value *cur = builder.CreateLoad(lv.storeType, lv.addr);
    llvm::Value *one = llvm::ConstantFP::get(llvm::Type::getDoubleTy(context), 1.0);
    llvm::Value *updated = expr->op == "++" ? builder.CreateFAdd(cur, one) : builder.CreateFSub(cur, one);
    builder.CreateStore(updated, lv.addr);
    // Prefix (`++x`) evaluates to the new, just-stored value; postfix
    // (`x++`) evaluates to whatever was there before the update.
    return expr->isPostfix ? cur : updated;
  }

  case ExprKind::Call: {
    if (expr->isIndirectCall) {
      // `lhs` is a Handler-*valued* expression (a variable, array
      // element, ...), not a named function or class method - there's no
      // symbol to look up, just a computed {fn, env} value (see
      // Sema::CheckExpr's Call case). `fn` always has the uniform
      // "(ptr env, ...params) -> void" thunk signature (see
      // GetOrCreatePlainThunk/GenClosureFunction) whether this is really
      // a closure or a plain function reference - `env` (null for the
      // latter) is simply the first actual argument either way. Param
      // types (beyond env) come from lhs's own resolved type rather than
      // re-deriving them from the argument expressions, so a zero-
      // argument indirect call still builds the right signature.
      llvm::Value *calleeVal = GenExpr(expr->lhs.get());
      llvm::Value *fnPtr = builder.CreateExtractValue(calleeVal, 0);
      llvm::Value *envPtr = builder.CreateExtractValue(calleeVal, 1);
      llvm::Type *ptrTy = llvm::PointerType::get(context, 0);
      std::vector<llvm::Type *> paramTypes = {ptrTy};
      paramTypes.reserve(1 + expr->lhs->resolvedType.handlerParamTypes->size());
      for (auto &p : *expr->lhs->resolvedType.handlerParamTypes) paramTypes.push_back(MapType(p));
      auto *fnTy = llvm::FunctionType::get(llvm::Type::getVoidTy(context), paramTypes, false);
      std::vector<llvm::Value *> args = {envPtr};
      args.reserve(1 + expr->elements.size());
      for (auto &argExpr : expr->elements) args.push_back(GenExpr(argExpr.get()));
      return builder.CreateCall(fnTy, fnPtr, args);
    }
    if (expr->virtualSlot >= 0) {
      // Real dynamic dispatch (see Expr::virtualSlot's own doc comment):
      // load the vtable pointer out of the receiver's own field 0 (every
      // class with hasVirtualDispatch uses that same slot - see
      // GetOrCreateStructType), index into it at this call's own
      // virtualSlot, and call through that function pointer - reaching
      // whichever override actually applies to the real, dynamic
      // instance, regardless of what was statically visible at this
      // call site. `expr->elements[0]` is the receiver (spliced in by
      // Sema's own CheckExpr Call case, same as an ordinary method call
      // already has) - generated once, here, and reused directly as the
      // call's own first argument rather than re-evaluating it.
      llvm::Value *receiverVal = GenExpr(expr->elements[0].get());
      llvm::StructType *receiverStructTy = GetOrCreateStructType(expr->elements[0]->resolvedType.structName);
      llvm::Type *ptrTy = llvm::PointerType::get(context, 0);
      llvm::Value *vtableFieldPtr = builder.CreateStructGEP(receiverStructTy, receiverVal, 0);
      llvm::Value *vtablePtr = builder.CreateLoad(ptrTy, vtableFieldPtr);
      llvm::Value *slotPtr =
          builder.CreateGEP(ptrTy, vtablePtr, {llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), expr->virtualSlot)});
      llvm::Value *fnPtr = builder.CreateLoad(ptrTy, slotPtr);

      // The statically-resolved implementation's own LLVM function is
      // used only for its signature shape - Sema already guarantees
      // every override in the family shares an ABI-compatible one (see
      // the vtable-computation pass's own exact-signature-match check).
      llvm::Function *staticImpl = llvmFunctions.at(expr->resolvedCalleeName);
      std::vector<llvm::Type *> paramTypes(staticImpl->getFunctionType()->params().begin(),
                                            staticImpl->getFunctionType()->params().end());
      auto *fnTy = llvm::FunctionType::get(staticImpl->getFunctionType()->getReturnType(), paramTypes, false);

      std::vector<llvm::Value *> args;
      args.reserve(expr->elements.size());
      args.push_back(receiverVal);
      for (size_t i = 1; i < expr->elements.size(); i++) {
        llvm::Value *val = GenExpr(expr->elements[i].get());
        AppendCallArg(args, val, expr->elements[i]->resolvedType, /*unpackHandler=*/false);
      }
      return builder.CreateCall(fnTy, fnPtr, args);
    }
    // resolvedCalleeName is the plain name for an ordinary call, or the
    // mangled per-instantiation name for a generic one (see
    // Sema::CheckGenericCall) - either way, exactly what
    // DeclareFunctionSignatures registered into llvmFunctions under. A
    // Handler-typed argument to an `isExtern` (declare function) callee
    // needs unpacking into two separate native arguments (fn, env) -
    // see AppendCallArg's own doc comment and DeclareFunctionSignatures'
    // matching signature-side rule; an ordinary ART-to-ART call (isExtern
    // false, including a call to a generic builtin like makeArray<T> -
    // see FunctionDecl::isExtern's own doc comment) passes the whole
    // 2-word struct through unchanged, matching the callee's own MapType-
    // derived parameter type.
    llvm::Function *callee = llvmFunctions.at(expr->resolvedCalleeName);
    FunctionDecl *calleeDecl = LookupCalleeDecl(expr->resolvedCalleeName);
    bool isExternCall = calleeDecl && calleeDecl->isExtern;
    bool hasRestParam = calleeDecl && !calleeDecl->params.empty() && calleeDecl->params.back().isRest;
    std::vector<llvm::Value *> args;
    args.reserve(calleeDecl ? calleeDecl->params.size() : expr->elements.size());
    if (hasRestParam) {
      // Every leading, fixed parameter is passed through as normal; every
      // TRAILING call-site argument (from the rest parameter's own
      // position onward, however many there are) is collected into one
      // real, freshly allocated array instead - same construction
      // ExprKind::ArrayLiteral's own codegen already uses (GC-malloc a
      // data buffer sized to fit exactly this many elements, store each,
      // GC-malloc the { length, data } header, store both) - and that
      // single array becomes the actual last argument. See
      // Sema::CheckExpr's own Call case for the matching per-argument
      // type-checking this mirrors.
      size_t fixedCount = calleeDecl->params.size() - 1;
      for (size_t i = 0; i < fixedCount; i++) {
        llvm::Value *val = GenExpr(expr->elements[i].get());
        AppendCallArg(args, val, expr->elements[i]->resolvedType, isExternCall);
      }
      const ResolvedType &elemType = *calleeDecl->params.back().resolvedType.elementType;
      llvm::Type *storageTy = ArrayElemStorageType(elemType);
      uint64_t elemSize = module->getDataLayout().getTypeAllocSize(storageTy).getFixedValue();
      uint64_t restCount = expr->elements.size() - fixedCount;

      llvm::Value *dataPtr = builder.CreateCall(
          gcMallocFn, {llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), restCount * elemSize)});
      for (uint64_t i = 0; i < restCount; i++) {
        llvm::Value *val = GenExpr(expr->elements[fixedCount + i].get());
        if (elemType.tag == TypeTag::Boolean) val = builder.CreateZExt(val, llvm::Type::getInt8Ty(context));
        llvm::Value *elemPtr = builder.CreateGEP(storageTy, dataPtr,
                                                  {llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), i)});
        builder.CreateStore(val, elemPtr);
      }
      llvm::Value *headerPtr =
          builder.CreateCall(gcMallocFn, {llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 16)});
      llvm::Value *lengthFieldPtr = builder.CreateStructGEP(GetArrayHeaderType(), headerPtr, 0);
      builder.CreateStore(llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), restCount), lengthFieldPtr);
      llvm::Value *dataFieldPtr = builder.CreateStructGEP(GetArrayHeaderType(), headerPtr, 1);
      builder.CreateStore(dataPtr, dataFieldPtr);
      AppendCallArg(args, headerPtr, calleeDecl->params.back().resolvedType, isExternCall);
    } else {
      for (auto &argExpr : expr->elements) {
        llvm::Value *val = GenExpr(argExpr.get());
        AppendCallArg(args, val, argExpr->resolvedType, isExternCall);
      }
    }
    return builder.CreateCall(callee, args);
  }

  case ExprKind::ArrayLiteral: {
    const ResolvedType &elemType = *expr->resolvedType.elementType;
    llvm::Type *storageTy = ArrayElemStorageType(elemType);
    // See GenBuiltinMakeArray's identical computation for why this can't
    // be a hardcoded "boolean ? 1 : 8" - Handler is a 16-byte struct now.
    uint64_t elemSize = module->getDataLayout().getTypeAllocSize(storageTy).getFixedValue();
    uint64_t count = expr->elements.size();

    llvm::Value *dataPtr = builder.CreateCall(
        gcMallocFn, {llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), count * elemSize)});

    for (uint64_t i = 0; i < count; i++) {
      llvm::Value *val = GenExpr(expr->elements[i].get());
      if (elemType.tag == TypeTag::Boolean) val = builder.CreateZExt(val, llvm::Type::getInt8Ty(context));
      llvm::Value *elemPtr = builder.CreateGEP(storageTy, dataPtr,
                                                {llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), i)});
      builder.CreateStore(val, elemPtr);
    }

    llvm::Value *headerPtr = builder.CreateCall(gcMallocFn, {llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 16)});
    llvm::Value *lengthFieldPtr = builder.CreateStructGEP(GetArrayHeaderType(), headerPtr, 0);
    builder.CreateStore(llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), count), lengthFieldPtr);
    llvm::Value *dataFieldPtr = builder.CreateStructGEP(GetArrayHeaderType(), headerPtr, 1);
    builder.CreateStore(dataPtr, dataFieldPtr);
    return headerPtr;
  }

  case ExprKind::ObjectLiteral: {
    const std::string &ifaceName = expr->resolvedType.structName;
    llvm::StructType *structTy = GetOrCreateStructType(ifaceName);
    InterfaceDecl *iface = sema.Interfaces().at(ifaceName);

    uint64_t sizeBytes = module->getDataLayout().getTypeAllocSize(structTy).getFixedValue();
    llvm::Value *instancePtr =
        builder.CreateCall(gcMallocFn, {llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), sizeBytes)});

    // A class touched by inheritance gets its own vtable pointer stored
    // into field 0 here, at construction time - this exact class's own
    // vtable (see GetOrCreateVtable), never a base's, regardless of what
    // static type this object literal is eventually assigned into. This
    // is the one place that decision gets made: everywhere else (a
    // method call through some base-typed reference) just reads
    // whichever vtable pointer is already sitting in the instance.
    if (iface->hasVirtualDispatch) {
      llvm::GlobalVariable *vtable = GetOrCreateVtable(iface);
      llvm::Value *vtableFieldPtr = builder.CreateStructGEP(structTy, instancePtr, 0);
      builder.CreateStore(vtable, vtableFieldPtr);
    }

    for (size_t idx = 0; idx < iface->fields.size(); idx++) {
      const std::string &fieldName = iface->fields[idx].name;
      Expr *valueExpr = nullptr;
      for (auto &f : expr->fields)
        if (f.first == fieldName) { valueExpr = f.second.get(); break; }
      llvm::Value *val = GenExpr(valueExpr);
      llvm::Value *fieldPtr =
          builder.CreateStructGEP(structTy, instancePtr, StructFieldGepIndex(iface, static_cast<int>(idx)));
      builder.CreateStore(val, fieldPtr);
    }
    return instancePtr;
  }

  case ExprKind::JsxElement: {
    if (expr->name.empty()) {
      // A fragment, `<>...</>` (see ExprKind::JsxElement's own doc
      // comment) - builds a real Node[] (same header shape ArrayLiteral
      // above does: {i64 length, ptr data}) out of its children instead
      // of a wrapping element, so its length is only known at runtime
      // whenever any child is itself a `Node[]` (spread) - unlike
      // ArrayLiteral, which always knows its element count at compile
      // time. Two passes: first total up the final length (a compile-
      // time constant per plain child, plus a runtime `.length` per
      // spread one), allocate exactly that much, then fill it in source
      // order. Every child is evaluated exactly once, before either pass
      // - never twice just to size the array and then fill it - in case
      // it has a side effect (e.g. a function call).
      llvm::Function *createTextNodeFn = llvmFunctions.at("ArtCreateTextNode");
      llvm::Function *numberToStringFn = llvmFunctions.at("numberToString");
      llvm::StructType *hdrTy = GetArrayHeaderType();
      llvm::Type *i64Ty = llvm::Type::getInt64Ty(context);
      llvm::Type *ptrTy = llvm::PointerType::get(context, 0);

      struct FragChild {
        llvm::Value *val;
        Expr *expr;
      };
      std::vector<FragChild> children;
      children.reserve(expr->elements.size());
      for (auto &child : expr->elements) children.push_back({GenExpr(child.get()), child.get()});

      llvm::Value *totalCount = llvm::ConstantInt::get(i64Ty, 0);
      for (auto &c : children) {
        if (c.expr->resolvedType.tag == TypeTag::Array) {
          llvm::Value *len = builder.CreateLoad(i64Ty, builder.CreateStructGEP(hdrTy, c.val, 0));
          totalCount = builder.CreateAdd(totalCount, len);
        } else {
          totalCount = builder.CreateAdd(totalCount, llvm::ConstantInt::get(i64Ty, 1));
        }
      }

      llvm::Value *dataPtr = GenHeapAlloc(builder.CreateMul(totalCount, llvm::ConstantInt::get(i64Ty, 8)));
      llvm::AllocaInst *writeIdxAlloca = CreateEntryAlloca(currentFunction, i64Ty, "fragment.widx");
      builder.CreateStore(llvm::ConstantInt::get(i64Ty, 0), writeIdxAlloca);

      for (auto &c : children) {
        if (c.expr->resolvedType.tag == TypeTag::Array) {
          // Spread: copy every element of this Node[] child in, via a
          // real runtime loop - same shape the element path's own
          // Node[]-child spread has (see below), just writing into this
          // fragment's own output array instead of ArtAppendChild-ing
          // into a parent.
          llvm::Value *len = builder.CreateLoad(i64Ty, builder.CreateStructGEP(hdrTy, c.val, 0));
          llvm::Value *srcData = builder.CreateLoad(ptrTy, builder.CreateStructGEP(hdrTy, c.val, 1));

          llvm::AllocaInst *srcIdxAlloca = CreateEntryAlloca(currentFunction, i64Ty, "fragment.spread.idx");
          builder.CreateStore(llvm::ConstantInt::get(i64Ty, 0), srcIdxAlloca);

          auto *condBB = llvm::BasicBlock::Create(context, "fragment.spread.cond", currentFunction);
          auto *bodyBB = llvm::BasicBlock::Create(context, "fragment.spread.body", currentFunction);
          auto *endBB = llvm::BasicBlock::Create(context, "fragment.spread.end", currentFunction);

          builder.CreateBr(condBB);
          builder.SetInsertPoint(condBB);
          llvm::Value *srcIdx = builder.CreateLoad(i64Ty, srcIdxAlloca);
          builder.CreateCondBr(builder.CreateICmpSLT(srcIdx, len), bodyBB, endBB);

          builder.SetInsertPoint(bodyBB);
          llvm::Value *srcElemPtr = builder.CreateGEP(ptrTy, srcData, {srcIdx});
          llvm::Value *elemVal = builder.CreateLoad(ptrTy, srcElemPtr);
          llvm::Value *writeIdx = builder.CreateLoad(i64Ty, writeIdxAlloca);
          builder.CreateStore(elemVal, builder.CreateGEP(ptrTy, dataPtr, {writeIdx}));
          builder.CreateStore(builder.CreateAdd(writeIdx, llvm::ConstantInt::get(i64Ty, 1)), writeIdxAlloca);
          builder.CreateStore(builder.CreateAdd(srcIdx, llvm::ConstantInt::get(i64Ty, 1)), srcIdxAlloca);
          builder.CreateBr(condBB);

          builder.SetInsertPoint(endBB);
        } else {
          llvm::Value *nodeVal;
          if (c.expr->resolvedType.tag == TypeTag::String) {
            nodeVal = builder.CreateCall(createTextNodeFn, {c.val});
          } else if (c.expr->resolvedType.tag == TypeTag::Number) {
            llvm::Value *strVal = builder.CreateCall(numberToStringFn, {c.val});
            nodeVal = builder.CreateCall(createTextNodeFn, {strVal});
          } else {
            nodeVal = c.val; // already a Node
          }
          llvm::Value *writeIdx = builder.CreateLoad(i64Ty, writeIdxAlloca);
          builder.CreateStore(nodeVal, builder.CreateGEP(ptrTy, dataPtr, {writeIdx}));
          builder.CreateStore(builder.CreateAdd(writeIdx, llvm::ConstantInt::get(i64Ty, 1)), writeIdxAlloca);
        }
      }

      llvm::Value *headerPtr = GenHeapAlloc(16);
      builder.CreateStore(totalCount, builder.CreateStructGEP(hdrTy, headerPtr, 0));
      builder.CreateStore(dataPtr, builder.CreateStructGEP(hdrTy, headerPtr, 1));
      return headerPtr;
    }

    // Desugars straight to ART's standard library's own raw DOM bridge
    // functions (art/stdlib/art.ts) - Sema already confirmed all of these
    // exist in the merged program (see its own JsxElement case). Multiple
    // calls building up one value before yielding it, same shape
    // ObjectLiteral above already has (allocate/build, then return one
    // pointer) - this is a real expression, not restricted to statement
    // position, even though it takes several IR instructions to produce.
    llvm::Function *createElementFn = llvmFunctions.at("ArtCreateElement");
    llvm::Function *setAttributeFn = llvmFunctions.at("ArtSetAttribute");
    llvm::Function *appendChildFn = llvmFunctions.at("ArtAppendChild");
    llvm::Function *addEventListenerFn = llvmFunctions.at("ArtAddEventListener");
    llvm::Function *createTextNodeFn = llvmFunctions.at("ArtCreateTextNode");
    llvm::Function *numberToStringFn = llvmFunctions.at("numberToString");

    llvm::Value *nodeVal = builder.CreateCall(createElementFn, {GenStringLiteral(expr->name)});

    for (auto &attr : expr->fields) {
      bool isEventAttr = attr.first.size() > 2 && attr.first.rfind("on", 0) == 0;
      llvm::Value *valueVal = GenExpr(attr.second.get());
      if (isEventAttr) {
        // "onclick" -> "click" - see Sema's own JsxElement case for the
        // handler-type check this relies on. Always non-capturing (`false`)
        // - the same simplicity real JSX/React's own onClick={...} has;
        // .addEventListener itself is still there for anyone who needs
        // capture=true.
        std::string eventType = attr.first.substr(2);
        llvm::Value *captureVal = llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), 0);
        // ArtAddEventListener is an isExtern declare function - its
        // Handler-typed parameter needs the same {fn,env}-unpacking
        // AppendCallArg gives every other call into it (see the
        // ordinary ExprKind::Call codegen above); this call site is
        // hand-built (not going through that generic path at all) so it
        // needs the same rule applied explicitly.
        std::vector<llvm::Value *> addEventListenerArgs = {nodeVal, GenStringLiteral(eventType)};
        AppendCallArg(addEventListenerArgs, valueVal, attr.second->resolvedType, /*unpackHandler=*/true);
        addEventListenerArgs.push_back(captureVal);
        builder.CreateCall(addEventListenerFn, addEventListenerArgs);
      } else {
        builder.CreateCall(setAttributeFn, {nodeVal, GenStringLiteral(attr.first), valueVal});
      }
    }

    for (auto &child : expr->elements) {
      llvm::Value *childVal = GenExpr(child.get());

      if (child->resolvedType.tag == TypeTag::Array) {
        // A `Node[]` child - spread: append each element in its own
        // right, a real runtime loop (the array's length is only known
        // at runtime) - same shape StmtKind::ForOf's own codegen already
        // has, just calling ArtAppendChild instead of storing into a
        // loop variable.
        llvm::StructType *hdrTy = GetArrayHeaderType();
        llvm::Type *i64Ty = llvm::Type::getInt64Ty(context);
        llvm::Type *ptrTy = llvm::PointerType::get(context, 0);
        llvm::Value *length = builder.CreateLoad(i64Ty, builder.CreateStructGEP(hdrTy, childVal, 0));
        llvm::Value *dataPtr = builder.CreateLoad(ptrTy, builder.CreateStructGEP(hdrTy, childVal, 1));

        llvm::AllocaInst *idxAlloca = CreateEntryAlloca(currentFunction, i64Ty, "jsx.spread.idx");
        builder.CreateStore(llvm::ConstantInt::get(i64Ty, 0), idxAlloca);

        auto *condBB = llvm::BasicBlock::Create(context, "jsx.spread.cond", currentFunction);
        auto *bodyBB = llvm::BasicBlock::Create(context, "jsx.spread.body", currentFunction);
        auto *endBB = llvm::BasicBlock::Create(context, "jsx.spread.end", currentFunction);

        builder.CreateBr(condBB);
        builder.SetInsertPoint(condBB);
        llvm::Value *idx = builder.CreateLoad(i64Ty, idxAlloca);
        builder.CreateCondBr(builder.CreateICmpSLT(idx, length), bodyBB, endBB);

        builder.SetInsertPoint(bodyBB);
        llvm::Value *elemPtr = builder.CreateGEP(ptrTy, dataPtr, {idx});
        llvm::Value *elemVal = builder.CreateLoad(ptrTy, elemPtr);
        builder.CreateCall(appendChildFn, {nodeVal, elemVal});
        llvm::Value *nextIdx = builder.CreateAdd(idx, llvm::ConstantInt::get(i64Ty, 1));
        builder.CreateStore(nextIdx, idxAlloca);
        builder.CreateBr(condBB);

        builder.SetInsertPoint(endBB);
        continue;
      }

      llvm::Value *childNodeVal;
      if (child->resolvedType.tag == TypeTag::String) {
        childNodeVal = builder.CreateCall(createTextNodeFn, {childVal});
      } else if (child->resolvedType.tag == TypeTag::Number) {
        llvm::Value *strVal = builder.CreateCall(numberToStringFn, {childVal});
        childNodeVal = builder.CreateCall(createTextNodeFn, {strVal});
      } else {
        // Already a Node (a nested JsxElement, or any other Node-typed
        // expression) - Sema already rejected anything else.
        childNodeVal = childVal;
      }
      builder.CreateCall(appendChildFn, {nodeVal, childNodeVal});
    }

    return nodeVal;
  }

  case ExprKind::Index: {
    if (expr->lhs->resolvedType.tag == TypeTag::String) {
      llvm::Value *strPtr = GenExpr(expr->lhs.get());
      llvm::Value *idxVal = GenExpr(expr->operand.get());
      return GenStringIndex(strPtr, idxVal);
    }
    LValue lv = GenLValue(expr);
    llvm::Value *raw = builder.CreateLoad(lv.storeType, lv.addr);
    if (lv.isBoolArrayElem) {
      return builder.CreateICmpNE(raw, llvm::ConstantInt::get(llvm::Type::getInt8Ty(context), 0));
    }
    return raw;
  }

  case ExprKind::Member: {
    if (expr->isLengthAccess) {
      llvm::Value *objPtr = GenExpr(expr->lhs.get());
      llvm::Value *lengthFieldPtr = builder.CreateStructGEP(GetArrayHeaderType(), objPtr, 0);
      llvm::Value *lengthI64 = builder.CreateLoad(llvm::Type::getInt64Ty(context), lengthFieldPtr);
      return builder.CreateUIToFP(lengthI64, llvm::Type::getDoubleTy(context));
    }
    LValue lv = GenLValue(expr);
    return builder.CreateLoad(lv.storeType, lv.addr);
  }

  case ExprKind::Assign: {
    // A setter-backed property (see Expr::resolvedCalleeName's own doc
    // comment and Sema::CheckLValueTarget) has no real field/memory
    // address at all - assigning to it calls the setter instead of
    // doing a plain store, still yielding the assigned value itself
    // (rhsVal) as this expression's own value, same as a plain store
    // below does.
    if (expr->lhs->kind == ExprKind::Member && !expr->lhs->resolvedCalleeName.empty()) {
      llvm::Value *rhsVal = GenExpr(expr->rhs.get());
      llvm::Value *receiverVal = GenExpr(expr->lhs->lhs.get());
      llvm::Function *setter = llvmFunctions.at(expr->lhs->resolvedCalleeName);
      builder.CreateCall(setter, {receiverVal, rhsVal});
      return rhsVal;
    }
    llvm::Value *rhsVal = GenExpr(expr->rhs.get());
    LValue lv = GenLValue(expr->lhs.get());
    llvm::Value *toStore = rhsVal;
    if (lv.isBoolArrayElem) toStore = builder.CreateZExt(rhsVal, llvm::Type::getInt8Ty(context));
    builder.CreateStore(toStore, lv.addr);
    return rhsVal;
  }

  case ExprKind::Conditional: {
    // Real branching + a PHI selecting the result, same shape the `&&`/
    // `||` short-circuit codegen above already has - just for an
    // arbitrary shared type (Sema already confirmed both branches agree)
    // instead of always i1.
    llvm::Value *condVal = GenExpr(expr->operand.get());
    auto *thenBB = llvm::BasicBlock::Create(context, "cond.then", currentFunction);
    auto *elseBB = llvm::BasicBlock::Create(context, "cond.else", currentFunction);
    auto *mergeBB = llvm::BasicBlock::Create(context, "cond.end", currentFunction);
    builder.CreateCondBr(condVal, thenBB, elseBB);

    builder.SetInsertPoint(thenBB);
    llvm::Value *thenVal = GenExpr(expr->lhs.get());
    llvm::BasicBlock *thenEndBB = builder.GetInsertBlock();
    builder.CreateBr(mergeBB);

    builder.SetInsertPoint(elseBB);
    llvm::Value *elseVal = GenExpr(expr->rhs.get());
    llvm::BasicBlock *elseEndBB = builder.GetInsertBlock();
    builder.CreateBr(mergeBB);

    builder.SetInsertPoint(mergeBB);
    llvm::PHINode *phi = builder.CreatePHI(MapType(expr->resolvedType), 2);
    phi->addIncoming(thenVal, thenEndBB);
    phi->addIncoming(elseVal, elseEndBB);
    return phi;
  }

  case ExprKind::TemplateLiteral: {
    // A left-to-right fold of GenStringConcat calls - the same '+' on
    // strings already lowers to (see the Binary case above) - stringifying
    // a `number` interpolation via numberToString first (Sema already
    // confirmed every interpolated part is a string or number).
    llvm::Function *numberToStringFn = llvmFunctions.at("numberToString");
    llvm::Value *result = GenExpr(expr->elements[0].get());
    for (size_t i = 1; i < expr->elements.size(); i += 2) {
      Expr *interp = expr->elements[i].get();
      llvm::Value *interpVal = GenExpr(interp);
      llvm::Value *interpStr = interp->resolvedType.tag == TypeTag::Number
                                    ? builder.CreateCall(numberToStringFn, {interpVal})
                                    : interpVal;
      result = GenStringConcat(result, interpStr);
      result = GenStringConcat(result, GenExpr(expr->elements[i + 1].get()));
    }
    return result;
  }

  case ExprKind::FunctionExpr: {
    // The closure's own thunk BODY was already generated as a separate
    // pass (see Generate()'s GenClosureFunction loop, driven by
    // sema.Closures()) - this only builds the {thunk, env} VALUE, right
    // here at the point the closure literal is actually evaluated (e.g.
    // inside a loop body, once per iteration - see GenStmt's VarDecl
    // case for why that gives each iteration's closure its own cell).
    FunctionDecl *fn = expr->fn.get();
    llvm::Function *thunk = llvmFunctions.at(fn->name);
    llvm::PointerType *ptrTy = llvm::PointerType::get(context, 0);
    llvm::Value *envPtr;
    if (fn->captures.empty()) {
      envPtr = llvm::ConstantPointerNull::get(ptrTy);
    } else {
      std::vector<llvm::Type *> fieldTypes(fn->captures.size(), ptrTy);
      auto *envTy = llvm::StructType::get(context, fieldTypes);
      uint64_t bytes = module->getDataLayout().getTypeAllocSize(envTy).getFixedValue();
      llvm::Value *envRaw = GenHeapAlloc(bytes);
      for (size_t i = 0; i < fn->captures.size(); i++) {
        // Looked up in the CURRENT (enclosing) function/closure's own
        // live scope - already an ordinary, boxed VarBinding whether it
        // was declared directly here or itself unpacked from THIS
        // frame's own env as a thread-through capture (see
        // GenClosureFunction's own env-unpack prologue) - no
        // distinction needed, which is exactly what makes N-level
        // nesting work by simple structural induction.
        VarBinding *b = Lookup(fn->captures[i].name);
        llvm::Value *cellPtr = builder.CreateLoad(ptrTy, b->alloca);
        llvm::Value *fieldPtr = builder.CreateStructGEP(envTy, envRaw, static_cast<unsigned>(i));
        builder.CreateStore(cellPtr, fieldPtr);
      }
      envPtr = envRaw;
    }
    llvm::Value *agg = llvm::UndefValue::get(GetHandlerStructType());
    agg = builder.CreateInsertValue(agg, thunk, {0});
    agg = builder.CreateInsertValue(agg, envPtr, {1});
    return agg;
  }
  }

  throw std::runtime_error("codegen: unhandled expression kind");
}

// See this method's own doc comment in codegen.h.
llvm::Value *Codegen::GenExpr(Expr *expr) {
  llvm::Value *raw = GenExprInner(expr);
  if (expr->needsAnyBox) return GenBoxAny(raw, expr->preBoxType);
  return raw;
}

} // namespace ART
