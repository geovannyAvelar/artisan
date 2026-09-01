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
  case TypeSyntaxKind::Named:
    if (!interfaces.count(node->name)) {
      Error(node->loc, "unknown type '" + node->name + "'");
      return ResolvedType{};
    }
    return ResolvedType::Struct(node->name);
  }
  return ResolvedType{};
}

void Sema::PushScope() { scopes.emplace_back(); }
void Sema::PopScope() { scopes.pop_back(); }

Sema::VarInfo *Sema::Lookup(const std::string &name) {
  for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
    auto found = it->find(name);
    if (found != it->end()) return &found->second;
  }
  auto globalIt = globals.find(name);
  if (globalIt != globals.end()) return &globalIt->second;
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
  builtins.push_back(std::move(numberToString));
}

bool Sema::Check(Program &program) {
  SeedBuiltins();

  for (auto &iface : program.interfaces) {
    if (interfaces.count(iface->name)) {
      Error(iface->loc, "interface '" + iface->name + "' is already declared");
    } else {
      interfaces[iface->name] = iface.get();
    }
  }

  for (auto &iface : program.interfaces) {
    std::unordered_set<std::string> seen;
    for (auto &field : iface->fields) {
      field.resolvedType = ResolveType(field.type.get());
      if (!seen.insert(field.name).second) {
        Error(field.loc, "duplicate field '" + field.name + "' in interface '" + iface->name + "'");
      }
    }
  }

  // Function signatures are registered before globals (even though a
  // global initializer must ultimately be a literal - see
  // CheckGlobalDecl) so a global's own diagnostics - e.g. a rejected
  // call expression's arguments - can still resolve a forward-declared
  // function instead of spuriously reporting it as undefined.
  for (auto &fn : program.functions) RegisterFunctionSignature(fn.get());
  for (auto &fn : program.externFunctions) RegisterFunctionSignature(fn.get());

  for (auto &g : program.globals) CheckGlobalDecl(g.get());

  for (auto &fn : program.functions) CheckFunctionBody(fn.get());

  return diagnostics.empty();
}

void Sema::CheckGlobalDecl(Stmt *stmt) {
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
  if (functions.count(fn->name)) {
    Error(fn->loc, "function '" + fn->name + "' is already declared");
  } else {
    functions[fn->name] = fn;
  }
  std::unordered_set<std::string> seen;
  for (auto &p : fn->params) {
    p.resolvedType = ResolveType(p.type.get());
    if (!seen.insert(p.name).second) {
      Error(p.loc, "duplicate parameter '" + p.name + "' in function '" + fn->name + "'");
    }
  }
  fn->resolvedReturnType = ResolveType(fn->returnType.get());
}

void Sema::CheckFunctionBody(FunctionDecl *decl) {
  currentFunction = decl;
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
      Error(target->loc, "undefined identifier '" + target->name + "'");
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
    if (v) {
      actual = v->type;
    } else if (auto it = functions.find(expr->name); it != functions.end()) {
      // A bare function name (not a call) - the only first-class value ART
      // functions have. Only a void-returning function can be one (ART has
      // no closures, so this is always just a plain code address, never a
      // captured environment) - its own parameter types become the
      // Handler's, checked structurally like any other type against
      // whatever "(params...) => void" the use site actually expects.
      FunctionDecl *fn = it->second;
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
    } else {
      Error(expr->loc, "undefined identifier '" + expr->name + "'");
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
    const std::string &callee = expr->lhs->name;
    auto it = functions.find(callee);
    if (it == functions.end()) {
      Error(expr->loc, "call to undefined function '" + callee + "'");
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
