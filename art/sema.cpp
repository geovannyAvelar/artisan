#include "sema.h"

#include <sstream>
#include <unordered_set>

namespace ART {

void Sema::Error(SourceLoc loc, const std::string &message) {
  std::ostringstream oss;
  oss << loc.line << ":" << loc.col << ": " << message;
  diagnostics.push_back(oss.str());
}

ResolvedType Sema::ResolveType(TypeNode *node) {
  switch (node->kind) {
  case TypeSyntaxKind::Number:
    return ResolvedType::Number();
  case TypeSyntaxKind::Boolean:
    return ResolvedType::Boolean();
  case TypeSyntaxKind::String:
    return ResolvedType::String();
  case TypeSyntaxKind::Handler: {
    std::vector<ResolvedType> params;
    params.reserve(node->handlerParamTypes.size());
    for (auto &p : node->handlerParamTypes) params.push_back(ResolveType(p.get()));
    return ResolvedType::Handler(std::move(params));
  }
  case TypeSyntaxKind::Void:
    return ResolvedType::Void();
  case TypeSyntaxKind::Array:
    return ResolvedType::ArrayOf(ResolveType(node->element.get()));
  case TypeSyntaxKind::Named: {
    // A generic instantiation's active substitution wins over everything
    // else: `T` inside a generic function's own signature/body always
    // means whatever concrete type this particular call site substituted
    // for it, never an interface named "T" (that would shadow the type
    // parameter, same as a real generic language's scoping rules). `T`
    // is always used bare (never as `T<...>` - ART has no higher-kinded
    // type parameters), so genericArgs is irrelevant here.
    if (currentSubstitution && node->genericArgs.empty()) {
      auto found = currentSubstitution->find(node->name);
      if (found != currentSubstitution->end()) return found->second;
    }

    if (!node->genericArgs.empty()) {
      auto it = genericInterfaces.find(node->name);
      bool foundAndVisible = it != genericInterfaces.end() && IsVisible(node->name);
      if (!foundAndVisible) {
        bool isPlainInterfaceVisible = interfaces.count(node->name) && IsVisible(node->name);
        if (isPlainInterfaceVisible) {
          Error(node->loc, "interface '" + node->name + "' is not generic - remove the type arguments");
        } else {
          Error(node->loc, "unknown generic type '" + node->name + "'" + VisibilityHint(node->name));
        }
        return ResolvedType{};
      }
      std::vector<ResolvedType> typeArgs;
      typeArgs.reserve(node->genericArgs.size());
      for (auto &a : node->genericArgs) typeArgs.push_back(ResolveType(a.get()));
      return InstantiateInterface(it->second, typeArgs, node->loc);
    }

    if (genericInterfaces.count(node->name) && IsVisible(node->name)) {
      Error(node->loc, "generic type '" + node->name + "' requires type arguments, e.g. '" + node->name + "<Type>'");
      return ResolvedType{};
    }
    if (interfaces.count(node->name) && IsVisible(node->name)) return ResolvedType::Struct(node->name);
    Error(node->loc, "unknown type '" + node->name + "'" + VisibilityHint(node->name));
    return ResolvedType{};
  }
  }
  return ResolvedType{};
}

ResolvedType Sema::InstantiateInterface(InterfaceDecl *tmpl, const std::vector<ResolvedType> &typeArgs,
                                         SourceLoc loc) {
  if (typeArgs.size() != tmpl->typeParams.size()) {
    Error(loc, "type '" + tmpl->name + "' expects " + std::to_string(tmpl->typeParams.size()) +
                   " type argument(s), got " + std::to_string(typeArgs.size()));
    return ResolvedType{};
  }

  std::string mangled = MangleInstantiation(tmpl->name, typeArgs);
  if (interfaces.count(mangled)) return ResolvedType::Struct(mangled);

  std::unordered_map<std::string, ResolvedType> subst;
  for (size_t i = 0; i < tmpl->typeParams.size(); i++) subst[tmpl->typeParams[i]] = typeArgs[i];

  auto clone = std::make_unique<InterfaceDecl>();
  clone->name = mangled;
  clone->loc = tmpl->loc;
  clone->isOpaque = tmpl->isOpaque;
  clone->sourceFile = tmpl->sourceFile;

  // Registered before its fields are resolved (not after) so a self-
  // referential generic interface resolves the recursive reference to
  // this same entry instead of instantiating forever - see the doc
  // comment on the declaration in sema.h.
  InterfaceDecl *inst = clone.get();
  interfaces[mangled] = inst;
  interfaceInstantiationStorage.push_back(std::move(clone));

  const std::unordered_map<std::string, ResolvedType> *savedSubst = currentSubstitution;
  currentSubstitution = &subst;
  // A field's type resolves lexically, against wherever the template
  // itself was declared - not wherever this particular instantiation was
  // asked for - same reasoning CheckGenericCall's own save/restore has.
  std::string savedFile = currentFile;
  currentFile = tmpl->sourceFile;
  for (auto &f : tmpl->fields) {
    InterfaceField cf;
    cf.name = f.name;
    cf.loc = f.loc;
    cf.resolvedType = ResolveType(f.type.get());
    inst->fields.push_back(std::move(cf));
  }
  currentFile = savedFile;
  currentSubstitution = savedSubst;

  return ResolvedType::Struct(mangled);
}

std::string Sema::MangleType(const ResolvedType &t) {
  switch (t.tag) {
  case TypeTag::Number: return "number";
  case TypeTag::Boolean: return "boolean";
  case TypeTag::String: return "string";
  case TypeTag::Void: return "void";
  case TypeTag::Array: return "arr_" + MangleType(*t.elementType);
  case TypeTag::Struct: return t.structName;
  case TypeTag::Handler: {
    std::string out = "fn";
    for (auto &p : *t.handlerParamTypes) out += "_" + MangleType(p);
    return out;
  }
  case TypeTag::Unknown: return "unknown";
  }
  return "?";
}

std::string Sema::MangleInstantiation(const std::string &name, const std::vector<ResolvedType> &typeArgs) {
  std::string out = name;
  for (auto &arg : typeArgs) out += "$" + MangleType(arg);
  return out;
}

void Sema::PushScope() { scopes.emplace_back(); }
void Sema::PopScope() { scopes.pop_back(); }

Sema::VarInfo *Sema::Lookup(const std::string &name) {
  for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
    auto found = it->find(name);
    if (found != it->end()) return &found->second;
  }
  auto globalIt = globals.find(name);
  if (globalIt != globals.end() && IsVisible(name)) return &globalIt->second;
  return nullptr;
}

void Sema::Declare(SourceLoc loc, const std::string &name, ResolvedType type, bool isConst) {
  auto &scope = scopes.back();
  if (scope.count(name)) {
    Error(loc, "'" + name + "' is already declared in this scope");
    return;
  }
  scope[name] = VarInfo{std::move(type), isConst};
}

bool Sema::IsVisible(const std::string &name) const {
  if (!visibility) return true; // no module graph resolved - fully flat/global, the original behavior
  if (builtinNames.count(name)) return true;
  auto it = visibility->find(currentFile);
  return it != visibility->end() && it->second.count(name) != 0;
}

std::string Sema::VisibilityHint(const std::string &name) const {
  if (!visibility) return "";
  bool existsAnywhere = globals.count(name) || functions.count(name) || interfaces.count(name) ||
                        genericFunctions.count(name) || genericInterfaces.count(name);
  return existsAnywhere ? " (it exists, but isn't imported into this file)" : "";
}

// ---------------------------------------------------------------------
// Top level
// ---------------------------------------------------------------------

void Sema::SeedBuiltins() {
  auto numberToString = std::make_unique<FunctionDecl>();
  numberToString->name = "numberToString";
  Param n;
  n.name = "n";
  n.resolvedType = ResolvedType::Number();
  numberToString->params.push_back(std::move(n));
  numberToString->resolvedReturnType = ResolvedType::String();
  // No body - Codegen::GenBuiltinNumberToString generates its LLVM
  // definition directly, the same way it does for every other function
  // signature it finds here, just without a Program::functions entry to
  // walk a Stmt body from.
  functions["numberToString"] = numberToString.get();
  builtinNames.insert("numberToString");
  builtins.push_back(std::move(numberToString));
}

bool Sema::Check(Program &program,
                  const std::unordered_map<std::string, std::unordered_set<std::string>> *visibilityMap) {
  visibility = visibilityMap;
  SeedBuiltins();

  for (auto &iface : program.interfaces) {
    bool alreadyDeclared = interfaces.count(iface->name) || genericInterfaces.count(iface->name);
    if (alreadyDeclared) {
      Error(iface->loc, "interface '" + iface->name + "' is already declared");
    }

    std::unordered_set<std::string> seenFields;
    for (auto &field : iface->fields) {
      if (!seenFields.insert(field.name).second) {
        Error(field.loc, "duplicate field '" + field.name + "' in interface '" + iface->name + "'");
      }
    }

    if (!iface->typeParams.empty()) {
      // A generic template's own field TypeNodes reference its type
      // parameters by name, same as a generic function's params do (see
      // RegisterFunctionSignature) - resolving them here, with no
      // substitution active yet, isn't needed for anything about the
      // template's own shape and would either misresolve a type
      // parameter as unknown or wrongly shadow a same-named interface.
      // InstantiateInterface resolves the (shared, read-only) TypeNodes
      // fresh, with substitution active, per actual instantiation instead.
      std::unordered_set<std::string> seenTypeParams;
      for (auto &t : iface->typeParams) {
        if (!seenTypeParams.insert(t).second) {
          Error(iface->loc, "duplicate type parameter '" + t + "' in interface '" + iface->name + "'");
        }
      }
      if (!alreadyDeclared) genericInterfaces[iface->name] = iface.get();
      continue;
    }

    if (!alreadyDeclared) interfaces[iface->name] = iface.get();
    currentFile = iface->sourceFile;
    for (auto &field : iface->fields) field.resolvedType = ResolveType(field.type.get());
  }
  currentFile.clear();

  // Function signatures are registered before globals (even though a
  // global initializer must ultimately be a literal - see
  // CheckGlobalDecl) so a global's own diagnostics - e.g. a rejected
  // call expression's arguments - can still resolve a forward-declared
  // function instead of spuriously reporting it as undefined.
  for (auto &fn : program.functions) RegisterFunctionSignature(fn.get());
  for (auto &fn : program.externFunctions) RegisterFunctionSignature(fn.get());

  for (auto &g : program.globals) CheckGlobalDecl(g.get());

  // A generic function's body is never checked in template form - only
  // each concrete instantiation a call site actually asks for (see
  // CheckGenericCall), lazily, the same "never fully checked until used"
  // deal C++ templates have.
  for (auto &fn : program.functions)
    if (fn->typeParams.empty()) CheckFunctionBody(fn.get());

  return diagnostics.empty();
}

void Sema::CheckGlobalDecl(Stmt *stmt) {
  currentFile = stmt->sourceFile;
  bool hasDeclared = stmt->declaredType != nullptr;
  ResolvedType declared;
  if (hasDeclared) declared = ResolveType(stmt->declaredType.get());

  if (hasDeclared && declared.tag != TypeTag::Number && declared.tag != TypeTag::Boolean &&
      declared.tag != TypeTag::String && declared.tag != TypeTag::Unknown) {
    Error(stmt->loc, "a top-level variable can only be 'number', 'boolean', or 'string' - found '" +
                          declared.ToString() +
                          "' (there's no way to statically initialize an array, interface, or "
                          "handler global yet)");
  }

  ExprKind k = stmt->expr->kind;
  if (k != ExprKind::NumberLiteral && k != ExprKind::BoolLiteral && k != ExprKind::StringLiteral) {
    Error(stmt->loc, "a top-level '" + std::string(stmt->isConst ? "const" : "let") +
                          "' initializer must be a literal number, boolean, or string - ART has no "
                          "static-initialization-order mechanism to run arbitrary code (a call, "
                          "arithmetic, another global, ...) before setupApp/main runs");
  }

  ResolvedType actual = CheckExpr(stmt->expr.get(), hasDeclared ? &declared : nullptr);
  stmt->resolvedVarType = hasDeclared ? declared : actual;

  if (globals.count(stmt->varName) || functions.count(stmt->varName)) {
    Error(stmt->loc, "'" + stmt->varName + "' is already declared");
  } else {
    globals[stmt->varName] = VarInfo{stmt->resolvedVarType, stmt->isConst};
  }
}

void Sema::RegisterFunctionSignature(FunctionDecl *fn) {
  bool alreadyDeclared = functions.count(fn->name) || genericFunctions.count(fn->name);
  if (alreadyDeclared) {
    Error(fn->loc, "function '" + fn->name + "' is already declared");
  }

  std::unordered_set<std::string> seenParams;
  for (auto &p : fn->params) {
    if (!seenParams.insert(p.name).second) {
      Error(p.loc, "duplicate parameter '" + p.name + "' in function '" + fn->name + "'");
    }
  }

  if (!fn->typeParams.empty()) {
    // A generic template's own param/return TypeNodes reference its type
    // parameters (`T`, ...) by name, same syntax as any other named type -
    // resolving them here (with no substitution active yet) would either
    // wrongly treat a type parameter as an unknown type, or wrongly
    // shadow a same-named interface. Neither is needed anyway: nothing
    // about a template's own shape (arity is enough for a call site's
    // argument-count check) requires resolving these before an actual
    // instantiation substitutes concrete types - see CheckGenericCall,
    // which resolves the (shared, read-only) TypeNodes fresh per
    // instantiation instead.
    std::unordered_set<std::string> seenTypeParams;
    for (auto &t : fn->typeParams) {
      if (!seenTypeParams.insert(t).second) {
        Error(fn->loc, "duplicate type parameter '" + t + "' in function '" + fn->name + "'");
      }
    }
    if (!alreadyDeclared) genericFunctions[fn->name] = fn;
    return;
  }

  if (!alreadyDeclared) functions[fn->name] = fn;
  currentFile = fn->sourceFile;
  for (auto &p : fn->params) p.resolvedType = ResolveType(p.type.get());
  fn->resolvedReturnType = ResolveType(fn->returnType.get());
}

void Sema::CheckFunctionBody(FunctionDecl *decl) {
  currentFunction = decl;
  currentFile = decl->sourceFile;
  PushScope();
  for (auto &p : decl->params) Declare(p.loc, p.name, p.resolvedType, /*isConst=*/false);
  CheckStmt(decl->body.get());
  PopScope();
  if (decl->resolvedReturnType.tag != TypeTag::Void && !AlwaysReturns(decl->body.get())) {
    Error(decl->loc, "function '" + decl->name + "' does not return a value of type " +
                          decl->resolvedReturnType.ToString() + " on all code paths");
  }
  currentFunction = nullptr;
}

ResolvedType Sema::CheckGenericCall(Expr *expr) {
  const std::string &callee = expr->lhs->name;
  auto it = genericFunctions.find(callee);
  if (it == genericFunctions.end() || !IsVisible(callee)) {
    if (it == genericFunctions.end() && functions.count(callee) && IsVisible(callee)) {
      Error(expr->loc, "function '" + callee + "' is not generic - remove the type arguments");
    } else {
      Error(expr->loc, "call to undefined generic function '" + callee + "'" + VisibilityHint(callee));
    }
    for (auto &arg : expr->elements) CheckExpr(arg.get(), nullptr);
    expr->resolvedCalleeName = callee;
    return ResolvedType{};
  }
  FunctionDecl *tmpl = it->second;

  if (expr->typeArgs.size() != tmpl->typeParams.size()) {
    Error(expr->loc, "function '" + callee + "' expects " + std::to_string(tmpl->typeParams.size()) +
                          " type argument(s), got " + std::to_string(expr->typeArgs.size()));
    for (auto &arg : expr->elements) CheckExpr(arg.get(), nullptr);
    expr->resolvedCalleeName = callee;
    return ResolvedType{};
  }

  std::vector<ResolvedType> concreteArgs;
  concreteArgs.reserve(expr->typeArgs.size());
  for (auto &t : expr->typeArgs) concreteArgs.push_back(ResolveType(t.get()));

  std::string mangled = MangleInstantiation(callee, concreteArgs);
  expr->resolvedCalleeName = mangled;

  FunctionDecl *inst;
  auto instIt = instantiations.find(mangled);
  if (instIt != instantiations.end()) {
    inst = instIt->second;
  } else {
    std::unordered_map<std::string, ResolvedType> subst;
    for (size_t i = 0; i < tmpl->typeParams.size(); i++) subst[tmpl->typeParams[i]] = concreteArgs[i];

    auto clone = std::make_unique<FunctionDecl>();
    clone->name = mangled;
    clone->loc = tmpl->loc;
    clone->sourceFile = tmpl->sourceFile;

    const std::unordered_map<std::string, ResolvedType> *savedSubst = currentSubstitution;
    currentSubstitution = &subst;
    // Everything about this instantiation - its param/return types and
    // its body - resolves lexically, against wherever the template
    // itself was written, not wherever this particular call site is.
    std::string savedFile = currentFile;
    currentFile = tmpl->sourceFile;

    clone->params.reserve(tmpl->params.size());
    for (auto &p : tmpl->params) {
      Param cp;
      cp.name = p.name;
      cp.loc = p.loc;
      cp.resolvedType = ResolveType(p.type.get());
      clone->params.push_back(std::move(cp));
    }
    clone->resolvedReturnType = ResolveType(tmpl->returnType.get());

    // Registered before the body is checked (not after) so a direct
    // self-recursive call to this exact instantiation inside its own
    // body - found via CheckGenericCall re-entering with the same
    // mangled name - resolves against this (signature-complete, body
    // still in progress) entry instead of infinitely re-instantiating.
    inst = clone.get();
    instantiations[mangled] = inst;
    instantiationStorage.push_back(std::move(clone));

    if (tmpl->body) {
      inst->body = CloneStmt(*tmpl->body);
      FunctionDecl *savedCurrentFunction = currentFunction;
      currentFunction = inst;
      PushScope();
      for (auto &p : inst->params) Declare(p.loc, p.name, p.resolvedType, /*isConst=*/false);
      CheckStmt(inst->body.get());
      PopScope();
      if (inst->resolvedReturnType.tag != TypeTag::Void && !AlwaysReturns(inst->body.get())) {
        Error(tmpl->loc, "function '" + callee + "' does not return a value of type " +
                              inst->resolvedReturnType.ToString() + " on all code paths (instantiated as '" +
                              mangled + "')");
      }
      currentFunction = savedCurrentFunction;
    }
    // else: a generic `declare function` - no body to check; Codegen
    // treats a null body as extern, same as any non-generic one.

    currentFile = savedFile;
    currentSubstitution = savedSubst;
  }

  if (inst->params.size() != expr->elements.size()) {
    Error(expr->loc, "function '" + callee + "' expects " + std::to_string(inst->params.size()) +
                          " argument(s), got " + std::to_string(expr->elements.size()));
  }
  size_t n = std::min(inst->params.size(), expr->elements.size());
  for (size_t i = 0; i < n; i++) CheckExpr(expr->elements[i].get(), &inst->params[i].resolvedType);
  for (size_t i = n; i < expr->elements.size(); i++) CheckExpr(expr->elements[i].get(), nullptr);

  return inst->resolvedReturnType;
}

bool Sema::AlwaysReturns(Stmt *stmt) {
  switch (stmt->kind) {
  case StmtKind::Return:
    return true;
  case StmtKind::Block:
    for (auto &s : stmt->statements)
      if (AlwaysReturns(s.get())) return true;
    return false;
  case StmtKind::If:
    return stmt->elseBranch && AlwaysReturns(stmt->body.get()) && AlwaysReturns(stmt->elseBranch.get());
  default:
    return false;
  }
}

// ---------------------------------------------------------------------
// Statements
// ---------------------------------------------------------------------

void Sema::CheckStmt(Stmt *stmt) {
  switch (stmt->kind) {
  case StmtKind::VarDecl: {
    bool hasDeclared = stmt->declaredType != nullptr;
    ResolvedType declared;
    if (hasDeclared) declared = ResolveType(stmt->declaredType.get());
    ResolvedType actual = CheckExpr(stmt->expr.get(), hasDeclared ? &declared : nullptr);
    stmt->resolvedVarType = hasDeclared ? declared : actual;
    if (!hasDeclared && actual.tag == TypeTag::Unknown) {
      Error(stmt->loc, "cannot infer a type for '" + stmt->varName + "' - add an explicit type annotation");
    } else if (!hasDeclared && actual.tag == TypeTag::Void) {
      Error(stmt->loc, "cannot declare '" + stmt->varName + "' with type void");
    }
    Declare(stmt->loc, stmt->varName, stmt->resolvedVarType, stmt->isConst);
    break;
  }
  case StmtKind::If: {
    ResolvedType boolT = ResolvedType::Boolean();
    CheckExpr(stmt->cond.get(), &boolT);
    CheckStmt(stmt->body.get());
    if (stmt->elseBranch) CheckStmt(stmt->elseBranch.get());
    break;
  }
  case StmtKind::While: {
    ResolvedType boolT = ResolvedType::Boolean();
    CheckExpr(stmt->cond.get(), &boolT);
    CheckStmt(stmt->body.get());
    break;
  }
  case StmtKind::For: {
    PushScope(); // so a `let` in the initializer is scoped to just the loop
    if (stmt->initStmt) CheckStmt(stmt->initStmt.get());
    if (stmt->cond) {
      ResolvedType boolT = ResolvedType::Boolean();
      CheckExpr(stmt->cond.get(), &boolT);
    }
    if (stmt->update) CheckExpr(stmt->update.get(), nullptr);
    CheckStmt(stmt->body.get());
    PopScope();
    break;
  }
  case StmtKind::ForOf: {
    ResolvedType iterableT = CheckExpr(stmt->expr.get(), nullptr);
    ResolvedType elemT;
    if (iterableT.tag == TypeTag::Array) {
      elemT = *iterableT.elementType;
    } else if (iterableT.tag != TypeTag::Unknown) {
      Error(stmt->loc, "'for...of' requires an array, found '" + iterableT.ToString() + "'");
    }
    stmt->resolvedVarType = elemT;
    PushScope();
    Declare(stmt->loc, stmt->varName, elemT, stmt->isConst);
    CheckStmt(stmt->body.get());
    PopScope();
    break;
  }
  case StmtKind::Return: {
    ResolvedType retT = currentFunction->resolvedReturnType;
    if (retT.tag == TypeTag::Void) {
      if (stmt->expr) {
        Error(stmt->loc, "function '" + currentFunction->name + "' returns void but this 'return' has a value");
        CheckExpr(stmt->expr.get(), nullptr);
      }
    } else if (!stmt->expr) {
      Error(stmt->loc, "function '" + currentFunction->name + "' must return a value of type " + retT.ToString());
    } else {
      CheckExpr(stmt->expr.get(), &retT);
    }
    break;
  }
  case StmtKind::ExprStmt:
    CheckExpr(stmt->expr.get(), nullptr);
    break;
  case StmtKind::Block:
    PushScope();
    for (auto &s : stmt->statements) CheckStmt(s.get());
    PopScope();
    break;
  }
}

// ---------------------------------------------------------------------
// Lvalues (shared by Assign and IncDec)
// ---------------------------------------------------------------------

ResolvedType Sema::CheckLValueTarget(Expr *target, SourceLoc opLoc) {
  if (target->kind == ExprKind::Identifier) {
    VarInfo *v = Lookup(target->name);
    ResolvedType t;
    if (!v) {
      Error(target->loc, "undefined identifier '" + target->name + "'" + VisibilityHint(target->name));
      t = ResolvedType{};
    } else {
      if (v->isConst) Error(opLoc, "cannot assign to '" + target->name + "' - it is declared 'const'");
      t = v->type;
    }
    target->resolvedType = t;
    return t;
  }

  if (target->kind == ExprKind::Index || target->kind == ExprKind::Member) {
    ResolvedType t = CheckExpr(target, nullptr);
    if (target->kind == ExprKind::Member && target->isLengthAccess) {
      Error(opLoc, "cannot assign to '.length' - it is read-only");
    }
    if (target->kind == ExprKind::Index && target->lhs->resolvedType.tag == TypeTag::String) {
      Error(opLoc, "strings are immutable - cannot assign to a character");
    }
    return t;
  }

  Error(opLoc, "invalid assignment target");
  return ResolvedType{};
}

// ---------------------------------------------------------------------
// Expressions
// ---------------------------------------------------------------------

ResolvedType Sema::CheckExpr(Expr *expr, const ResolvedType *expected) {
  ResolvedType actual;

  switch (expr->kind) {
  case ExprKind::NumberLiteral:
    actual = ResolvedType::Number();
    break;

  case ExprKind::BoolLiteral:
    actual = ResolvedType::Boolean();
    break;

  case ExprKind::StringLiteral:
    actual = ResolvedType::String();
    break;

  case ExprKind::Identifier: {
    VarInfo *v = Lookup(expr->name);
    auto funcIt = functions.find(expr->name);
    if (v) {
      actual = v->type;
    } else if (funcIt != functions.end() && IsVisible(expr->name)) {
      // A bare function name (not a call) - the only first-class value ART
      // functions have. Only a void-returning function can be one (ART has
      // no closures, so this is always just a plain code address, never a
      // captured environment) - its own parameter types become the
      // Handler's, checked structurally like any other type against
      // whatever "(params...) => void" the use site actually expects.
      FunctionDecl *fn = funcIt->second;
      if (fn->resolvedReturnType.tag != TypeTag::Void) {
        Error(expr->loc, "function '" + expr->name +
                              "' can't be used as a value - only a void-returning function can "
                              "(as a handler)");
        actual = ResolvedType{};
      } else {
        std::vector<ResolvedType> paramTypes;
        paramTypes.reserve(fn->params.size());
        for (auto &p : fn->params) paramTypes.push_back(p.resolvedType);
        actual = ResolvedType::Handler(std::move(paramTypes));
      }
    } else if (genericFunctions.count(expr->name) && IsVisible(expr->name)) {
      Error(expr->loc, "generic function '" + expr->name +
                            "' can't be used as a value - only a fully-instantiated call is supported "
                            "('" + expr->name + "::<Type>(...)')");
      actual = ResolvedType{};
    } else {
      Error(expr->loc, "undefined identifier '" + expr->name + "'" + VisibilityHint(expr->name));
      actual = ResolvedType{};
    }
    break;
  }

  case ExprKind::Binary: {
    ResolvedType lhsT = CheckExpr(expr->lhs.get(), nullptr);
    ResolvedType rhsT = CheckExpr(expr->rhs.get(), nullptr);
    const std::string &op = expr->op;
    bool lhsUnknown = lhsT.tag == TypeTag::Unknown;
    bool rhsUnknown = rhsT.tag == TypeTag::Unknown;
    if (op == "+" && (lhsT.tag == TypeTag::String || rhsT.tag == TypeTag::String)) {
      if (!lhsUnknown && !rhsUnknown && (lhsT.tag != TypeTag::String || rhsT.tag != TypeTag::String)) {
        Error(expr->loc, "operator '+' requires two strings or two numbers, found '" + lhsT.ToString() + "' and '" +
                              rhsT.ToString() + "'");
      }
      actual = ResolvedType::String();
    } else if (op == "+" || op == "-" || op == "*" || op == "/" || op == "%") {
      if (!lhsUnknown && !rhsUnknown && (lhsT.tag != TypeTag::Number || rhsT.tag != TypeTag::Number)) {
        Error(expr->loc, "operator '" + op + "' requires two numbers, found '" + lhsT.ToString() + "' and '" +
                              rhsT.ToString() + "'");
      }
      actual = ResolvedType::Number();
    } else if (op == "<" || op == "<=" || op == ">" || op == ">=") {
      if (!lhsUnknown && !rhsUnknown && (lhsT.tag != TypeTag::Number || rhsT.tag != TypeTag::Number)) {
        Error(expr->loc, "operator '" + op + "' requires two numbers, found '" + lhsT.ToString() + "' and '" +
                              rhsT.ToString() + "'");
      }
      actual = ResolvedType::Boolean();
    } else if (op == "==" || op == "!=") {
      if (!lhsUnknown && !rhsUnknown && lhsT != rhsT) {
        Error(expr->loc, "operator '" + op + "' requires operands of the same type, found '" + lhsT.ToString() +
                              "' and '" + rhsT.ToString() + "'");
      }
      actual = ResolvedType::Boolean();
    } else if (op == "&&" || op == "||") {
      if (!lhsUnknown && !rhsUnknown && (lhsT.tag != TypeTag::Boolean || rhsT.tag != TypeTag::Boolean)) {
        Error(expr->loc, "operator '" + op + "' requires two booleans, found '" + lhsT.ToString() + "' and '" +
                              rhsT.ToString() + "'");
      }
      actual = ResolvedType::Boolean();
    } else {
      Error(expr->loc, "unknown operator '" + op + "'");
      actual = ResolvedType{};
    }
    break;
  }

  case ExprKind::Unary: {
    ResolvedType operandT = CheckExpr(expr->operand.get(), nullptr);
    bool unknown = operandT.tag == TypeTag::Unknown;
    if (expr->op == "-") {
      if (!unknown && operandT.tag != TypeTag::Number) {
        Error(expr->loc, "unary '-' requires a number, found '" + operandT.ToString() + "'");
      }
      actual = ResolvedType::Number();
    } else if (expr->op == "!") {
      if (!unknown && operandT.tag != TypeTag::Boolean) {
        Error(expr->loc, "unary '!' requires a boolean, found '" + operandT.ToString() + "'");
      }
      actual = ResolvedType::Boolean();
    } else {
      Error(expr->loc, "unknown unary operator '" + expr->op + "'");
      actual = ResolvedType{};
    }
    break;
  }

  case ExprKind::Call: {
    if (expr->lhs->kind != ExprKind::Identifier) {
      Error(expr->loc, "only direct calls to a named function are supported");
      for (auto &arg : expr->elements) CheckExpr(arg.get(), nullptr);
      actual = ResolvedType{};
      break;
    }
    if (!expr->typeArgs.empty()) {
      actual = CheckGenericCall(expr);
      break;
    }
    const std::string &callee = expr->lhs->name;
    expr->resolvedCalleeName = callee;
    auto it = functions.find(callee);
    if (it == functions.end() || !IsVisible(callee)) {
      if (it == functions.end() && genericFunctions.count(callee) && IsVisible(callee)) {
        Error(expr->loc, "generic function '" + callee + "' requires explicit type arguments, e.g. '" + callee +
                              "::<Type>(...)'");
      } else {
        Error(expr->loc, "call to undefined function '" + callee + "'" + VisibilityHint(callee));
      }
      for (auto &arg : expr->elements) CheckExpr(arg.get(), nullptr);
      actual = ResolvedType{};
      break;
    }
    FunctionDecl *fn = it->second;
    if (fn->params.size() != expr->elements.size()) {
      Error(expr->loc, "function '" + callee + "' expects " + std::to_string(fn->params.size()) +
                            " argument(s), got " + std::to_string(expr->elements.size()));
    }
    size_t n = std::min(fn->params.size(), expr->elements.size());
    for (size_t i = 0; i < n; i++) CheckExpr(expr->elements[i].get(), &fn->params[i].resolvedType);
    for (size_t i = n; i < expr->elements.size(); i++) CheckExpr(expr->elements[i].get(), nullptr);
    actual = fn->resolvedReturnType;
    break;
  }

  case ExprKind::ArrayLiteral: {
    if (expected && expected->tag == TypeTag::Array) {
      for (auto &el : expr->elements) CheckExpr(el.get(), expected->elementType.get());
      actual = *expected;
    } else if (expr->elements.empty()) {
      Error(expr->loc, "cannot infer the element type of an empty array literal - add an explicit type annotation");
      actual = ResolvedType{};
    } else {
      ResolvedType elemT = CheckExpr(expr->elements[0].get(), nullptr);
      for (size_t i = 1; i < expr->elements.size(); i++) CheckExpr(expr->elements[i].get(), &elemT);
      actual = ResolvedType::ArrayOf(elemT);
    }
    break;
  }

  case ExprKind::ObjectLiteral: {
    if (!expected || expected->tag != TypeTag::Struct) {
      Error(expr->loc, "object literal needs a target interface type - add an explicit type annotation");
      for (auto &f : expr->fields) CheckExpr(f.second.get(), nullptr);
      actual = ResolvedType{};
      break;
    }
    InterfaceDecl *iface = interfaces.at(expected->structName);
    if (iface->isOpaque) {
      Error(expr->loc, "'" + iface->name +
                            "' is an opaque foreign type (declared with 'declare type') - it has no "
                            "accessible fields and can't be constructed with '{}', only obtained from "
                            "a 'declare function' call");
      for (auto &f : expr->fields) CheckExpr(f.second.get(), nullptr);
      actual = *expected;
      break;
    }
    std::unordered_set<std::string> provided;
    for (auto &f : expr->fields) {
      const InterfaceField *field = nullptr;
      for (auto &candidate : iface->fields)
        if (candidate.name == f.first) { field = &candidate; break; }
      if (!field) {
        Error(expr->loc, "interface '" + iface->name + "' has no field '" + f.first + "'");
        CheckExpr(f.second.get(), nullptr);
        continue;
      }
      if (!provided.insert(f.first).second) {
        Error(expr->loc, "duplicate field '" + f.first + "' in object literal");
      }
      CheckExpr(f.second.get(), &field->resolvedType);
    }
    for (auto &field : iface->fields) {
      if (!provided.count(field.name)) {
        Error(expr->loc, "missing field '" + field.name + "' required by interface '" + iface->name + "'");
      }
    }
    actual = *expected;
    break;
  }

  case ExprKind::Index: {
    ResolvedType objT = CheckExpr(expr->lhs.get(), nullptr);
    ResolvedType idxT = CheckExpr(expr->operand.get(), nullptr);
    if (objT.tag == TypeTag::Unknown) {
      actual = ResolvedType{};
    } else if (objT.tag != TypeTag::Array && objT.tag != TypeTag::String) {
      Error(expr->loc, "cannot index into type '" + objT.ToString() + "' - only arrays and strings support '[]'");
      actual = ResolvedType{};
    } else {
      if (idxT.tag != TypeTag::Unknown && idxT.tag != TypeTag::Number) {
        Error(expr->loc, "index must be a number, found '" + idxT.ToString() + "'");
      }
      // Indexing a string yields a 1-character string, same as real TS.
      actual = objT.tag == TypeTag::String ? ResolvedType::String() : *objT.elementType;
    }
    break;
  }

  case ExprKind::Member: {
    ResolvedType objT = CheckExpr(expr->lhs.get(), nullptr);
    if ((objT.tag == TypeTag::Array || objT.tag == TypeTag::String) && expr->name == "length") {
      expr->isLengthAccess = true;
      actual = ResolvedType::Number();
    } else if (objT.tag == TypeTag::Struct) {
      InterfaceDecl *iface = interfaces.at(objT.structName);
      const InterfaceField *field = nullptr;
      for (auto &candidate : iface->fields)
        if (candidate.name == expr->name) { field = &candidate; break; }
      if (!field) {
        Error(expr->loc, "interface '" + iface->name + "' has no field '" + expr->name + "'");
        actual = ResolvedType{};
      } else {
        actual = field->resolvedType;
      }
    } else if (objT.tag == TypeTag::Unknown) {
      actual = ResolvedType{};
    } else {
      Error(expr->loc, "cannot access member '" + expr->name + "' on type '" + objT.ToString() + "'");
      actual = ResolvedType{};
    }
    break;
  }

  case ExprKind::IncDec: {
    ResolvedType targetT = CheckLValueTarget(expr->operand.get(), expr->loc);
    if (targetT.tag != TypeTag::Unknown && targetT.tag != TypeTag::Number) {
      Error(expr->loc, "operator '" + expr->op + "' requires a number, found '" + targetT.ToString() + "'");
    }
    actual = ResolvedType::Number();
    break;
  }

  case ExprKind::Assign: {
    ResolvedType targetT = CheckLValueTarget(expr->lhs.get(), expr->loc);
    CheckExpr(expr->rhs.get(), &targetT);
    actual = targetT;
    break;
  }
  }

  expr->resolvedType = actual;
  if (expected && actual.tag != TypeTag::Unknown && expected->tag != TypeTag::Unknown && actual != *expected) {
    Error(expr->loc, "type mismatch: expected '" + expected->ToString() + "', found '" + actual.ToString() + "'");
  }
  return actual;
}

} // namespace ART
