#include "codegen.h"

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
    return llvm::PointerType::get(context, 0);
  case TypeTag::Void:
    return llvm::Type::getVoidTy(context);
  case TypeTag::Array:
    return llvm::PointerType::get(context, 0);
  case TypeTag::Struct:
    return llvm::PointerType::get(context, 0);
  case TypeTag::Unknown:
    break;
  }
  throw std::runtime_error("codegen: unresolved type reached MapType - Sema should have rejected this program");
}

llvm::Type *Codegen::ArrayElemStorageType(const ResolvedType &elem) {
  if (elem.tag == TypeTag::Boolean) return llvm::Type::getInt8Ty(context);
  return MapType(elem);
}

llvm::StructType *Codegen::GetArrayHeaderType() {
  if (!arrayHeaderType) {
    arrayHeaderType = llvm::StructType::create(
        context, {llvm::Type::getInt64Ty(context), llvm::PointerType::get(context, 0)}, "art.array");
  }
  return arrayHeaderType;
}

llvm::StructType *Codegen::GetOrCreateStructType(const std::string &ifaceName) {
  auto it = structTypes.find(ifaceName);
  if (it != structTypes.end()) return it->second;

  InterfaceDecl *iface = sema.Interfaces().at(ifaceName);
  std::vector<llvm::Type *> fieldTypes;
  fieldTypes.reserve(iface->fields.size());
  for (auto &field : iface->fields) fieldTypes.push_back(MapType(field.resolvedType));

  auto *structTy = llvm::StructType::create(context, fieldTypes, "struct." + ifaceName);
  structTypes[ifaceName] = structTy;
  return structTy;
}

int Codegen::FieldIndex(InterfaceDecl *iface, const std::string &fieldName) {
  for (size_t i = 0; i < iface->fields.size(); i++)
    if (iface->fields[i].name == fieldName) return static_cast<int>(i);
  throw std::runtime_error("codegen: unknown field '" + fieldName + "' on interface '" + iface->name +
                            "' - Sema should have rejected this program");
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

void Codegen::Declare(const std::string &name, llvm::AllocaInst *alloca, llvm::Type *type) {
  scopes.back()[name] = VarBinding{alloca, type};
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
  for (auto &[mangledName, inst] : sema.Instantiations()) {
    if (mangledName.rfind("makeArray$", 0) == 0) {
      makeArrayInstantiations.push_back(inst);
    } else {
      instantiations.push_back(inst);
    }
  }

  DeclareFunctionSignatures(concreteFunctions, /*allowMainRename=*/true);
  DeclareFunctionSignatures(externFunctions, /*allowMainRename=*/false);
  DeclareFunctionSignatures(instantiations, /*allowMainRename=*/false);
  DeclareFunctionSignatures(makeArrayInstantiations, /*allowMainRename=*/false);
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
  for (auto *fn : makeArrayInstantiations) GenBuiltinMakeArray(fn);
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
    paramTypes.reserve(decl->params.size());
    for (auto &p : decl->params) paramTypes.push_back(MapType(p.resolvedType));
    llvm::Type *retTy = MapType(decl->resolvedReturnType);
    auto *fnTy = llvm::FunctionType::get(retTy, paramTypes, false);
    // ART's own "main" (if any) is renamed at the LLVM level so the real C
    // ABI entry point ("main", built after every function is generated -
    // see Generate()) can own that symbol instead. Never applies to a
    // `declare function` - it must bind to the external symbol verbatim.
    std::string llvmName = (allowMainRename && decl->name == "main") ? "__art_main" : decl->name;
    auto *fn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, llvmName, module.get());
    size_t i = 0;
    for (auto &arg : fn->args()) arg.setName(decl->params[i++].name);
    llvmFunctions[decl->name] = fn;
  }
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
  uint64_t elemSize = elemType.tag == TypeTag::Boolean ? 1 : 8; // i8 vs {double,ptr} - same convention ArrayLiteral uses

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

void Codegen::GenFunction(FunctionDecl *decl) {
  llvm::Function *fn = llvmFunctions.at(decl->name);
  currentFunction = fn;

  auto *entry = llvm::BasicBlock::Create(context, "entry", fn);
  builder.SetInsertPoint(entry);
  PushScope();

  size_t i = 0;
  for (auto &arg : fn->args()) {
    const Param &p = decl->params[i++];
    llvm::Type *ty = MapType(p.resolvedType);
    llvm::AllocaInst *alloca = CreateEntryAlloca(fn, ty, p.name);
    builder.CreateStore(&arg, alloca);
    Declare(p.name, alloca, ty);
  }

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

// ---------------------------------------------------------------------
// Statements
// ---------------------------------------------------------------------

void Codegen::GenStmt(Stmt *stmt) {
  switch (stmt->kind) {
  case StmtKind::VarDecl: {
    llvm::Type *ty = MapType(stmt->resolvedVarType);
    llvm::AllocaInst *alloca = CreateEntryAlloca(currentFunction, ty, stmt->varName);
    llvm::Value *val = GenExpr(stmt->expr.get());
    builder.CreateStore(val, alloca);
    Declare(stmt->varName, alloca, ty);
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
    GenStmt(stmt->body.get());
    if (!builder.GetInsertBlock()->getTerminator()) builder.CreateBr(condBB);

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
    GenStmt(stmt->body.get());
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

    llvm::AllocaInst *idxAlloca = CreateEntryAlloca(currentFunction, i64Ty, "forof.idx");
    builder.CreateStore(llvm::ConstantInt::get(i64Ty, 0), idxAlloca);
    llvm::AllocaInst *varAlloca = CreateEntryAlloca(currentFunction, varTy, stmt->varName);

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
    builder.CreateStore(elemVal, varAlloca);
    Declare(stmt->varName, varAlloca, varTy);
    GenStmt(stmt->body.get());
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
    if (stmt->expr) {
      builder.CreateRet(GenExpr(stmt->expr.get()));
    } else {
      builder.CreateRetVoid();
    }
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
  llvm::Value *fieldPtr = builder.CreateStructGEP(structTy, objPtr, idx);
  return {fieldPtr, MapType(iface->fields[idx].resolvedType), false};
}

// ---------------------------------------------------------------------
// Expressions
// ---------------------------------------------------------------------

llvm::Value *Codegen::GenExpr(Expr *expr) {
  switch (expr->kind) {
  case ExprKind::NumberLiteral:
    return llvm::ConstantFP::get(llvm::Type::getDoubleTy(context), expr->numberValue);

  case ExprKind::BoolLiteral:
    return llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), expr->boolValue ? 1 : 0);

  case ExprKind::StringLiteral:
    return GenStringLiteral(expr->name);

  case ExprKind::Identifier: {
    if (expr->resolvedType.tag == TypeTag::Handler) {
      // A Handler-typed Identifier is either a real local/global variable
      // holding a Handler value (e.g. a parameter forwarded into another
      // call - `function wrap(h: () => void): void { other(h); }`) or a
      // bare reference to a top-level function used as a value, a plain
      // code address with no variable/alloca of its own at all (see
      // Sema::CheckExpr's Identifier case - both produce the same
      // TypeTag::Handler resolvedType, so only an actual variable lookup
      // tells them apart, not the type tag alone).
      VarBinding *b = TryLookup(expr->name);
      if (b) return builder.CreateLoad(b->type, b->alloca, expr->name);
      return llvmFunctions.at(expr->name);
    }
    VarBinding *b = Lookup(expr->name);
    return builder.CreateLoad(b->type, b->alloca, expr->name);
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
      if (op == "==") return GenStringEquals(lhsVal, rhsVal);
      if (op == "!=") return builder.CreateNot(GenStringEquals(lhsVal, rhsVal));
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
    // Number compares as float; every other reachable tag here (Boolean,
    // and - via Sema's generic "same tag" rule for "=="/"!=" - Struct/
    // Array/Handler, all represented as `ptr`) compares as a plain
    // integer/pointer identity check. String has its own branch above.
    if (op == "==") {
      return operandTag == TypeTag::Number ? builder.CreateFCmpOEQ(lhsVal, rhsVal)
                                            : builder.CreateICmpEQ(lhsVal, rhsVal);
    }
    if (op == "!=") {
      return operandTag == TypeTag::Number ? builder.CreateFCmpONE(lhsVal, rhsVal)
                                            : builder.CreateICmpNE(lhsVal, rhsVal);
    }
    throw std::runtime_error("codegen: unknown binary operator '" + op + "'");
  }

  case ExprKind::Unary: {
    llvm::Value *operandVal = GenExpr(expr->operand.get());
    if (expr->op == "-") return builder.CreateFNeg(operandVal);
    if (expr->op == "!") return builder.CreateNot(operandVal);
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
      // symbol to look up, just a computed function-pointer value (see
      // Sema::CheckExpr's Call case). The function type only needs
      // param types (every Handler is void-returning), taken from lhs's
      // own resolved type rather than re-deriving it from the argument
      // expressions, so an indirect call with zero arguments still
      // builds the right (empty) signature.
      llvm::Value *calleeVal = GenExpr(expr->lhs.get());
      std::vector<llvm::Type *> paramTypes;
      paramTypes.reserve(expr->lhs->resolvedType.handlerParamTypes->size());
      for (auto &p : *expr->lhs->resolvedType.handlerParamTypes) paramTypes.push_back(MapType(p));
      auto *fnTy = llvm::FunctionType::get(llvm::Type::getVoidTy(context), paramTypes, false);
      std::vector<llvm::Value *> args;
      args.reserve(expr->elements.size());
      for (auto &argExpr : expr->elements) args.push_back(GenExpr(argExpr.get()));
      return builder.CreateCall(fnTy, calleeVal, args);
    }
    // resolvedCalleeName is the plain name for an ordinary call, or the
    // mangled per-instantiation name for a generic one (see
    // Sema::CheckGenericCall) - either way, exactly what
    // DeclareFunctionSignatures registered into llvmFunctions under.
    llvm::Function *callee = llvmFunctions.at(expr->resolvedCalleeName);
    std::vector<llvm::Value *> args;
    args.reserve(expr->elements.size());
    for (auto &argExpr : expr->elements) args.push_back(GenExpr(argExpr.get()));
    return builder.CreateCall(callee, args);
  }

  case ExprKind::ArrayLiteral: {
    const ResolvedType &elemType = *expr->resolvedType.elementType;
    llvm::Type *storageTy = ArrayElemStorageType(elemType);
    uint64_t elemSize = elemType.tag == TypeTag::Boolean ? 1 : 8; // i8 vs {double,ptr}
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

    for (size_t idx = 0; idx < iface->fields.size(); idx++) {
      const std::string &fieldName = iface->fields[idx].name;
      Expr *valueExpr = nullptr;
      for (auto &f : expr->fields)
        if (f.first == fieldName) { valueExpr = f.second.get(); break; }
      llvm::Value *val = GenExpr(valueExpr);
      llvm::Value *fieldPtr = builder.CreateStructGEP(structTy, instancePtr, static_cast<unsigned>(idx));
      builder.CreateStore(val, fieldPtr);
    }
    return instancePtr;
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
  }

  throw std::runtime_error("codegen: unhandled expression kind");
}

} // namespace ART
