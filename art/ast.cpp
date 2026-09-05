#include "ast.h"

namespace ART {

bool ResolvedType::operator==(const ResolvedType &other) const {
  if (tag != other.tag) return false;
  switch (tag) {
  case TypeTag::Array:
  case TypeTag::Nullable:
    return *elementType == *other.elementType;
  case TypeTag::Struct:
  case TypeTag::Enum:
    return structName == other.structName;
  case TypeTag::Handler:
    return *handlerParamTypes == *other.handlerParamTypes;
  default:
    return true;
  }
}

std::string ResolvedType::ToString() const {
  switch (tag) {
  case TypeTag::Unknown: return "<unknown>";
  case TypeTag::Number: return "number";
  case TypeTag::Boolean: return "boolean";
  case TypeTag::String: return "string";
  case TypeTag::Void: return "void";
  case TypeTag::Array: return elementType->ToString() + "[]";
  case TypeTag::Struct: return structName;
  case TypeTag::Enum: return structName;
  case TypeTag::Nullable: return elementType->ToString() + " | null";
  case TypeTag::Any: return "any";
  case TypeTag::Handler: {
    std::string out = "(";
    for (size_t i = 0; i < handlerParamTypes->size(); i++) {
      if (i > 0) out += ", ";
      out += (*handlerParamTypes)[i].ToString();
    }
    return out + ") => void";
  }
  }
  return "?";
}

// ---------------------------------------------------------------------
// Deep-clone helpers (see the declaration in ast.h)
// ---------------------------------------------------------------------

namespace {
std::unique_ptr<TypeNode> CloneTypeNode(const TypeNode &node) {
  auto out = std::make_unique<TypeNode>();
  out->kind = node.kind;
  out->loc = node.loc;
  out->name = node.name;
  if (node.element) out->element = CloneTypeNode(*node.element);
  for (auto &p : node.handlerParamTypes) out->handlerParamTypes.push_back(CloneTypeNode(*p));
  for (auto &a : node.genericArgs) out->genericArgs.push_back(CloneTypeNode(*a));
  return out;
}

// Used only by CloneExpr's FunctionExpr case, to give each instantiation
// of a generic function/class containing a closure literal its own,
// independent copy of that closure's AST - same reasoning CloneStmt's
// own top-of-file doc comment gives for cloning at all. Deliberately
// does NOT copy `name`/`resolvedReturnType`/`captures`/`isExtern` - all
// Sema-computed (or, for isExtern, simply never applicable to a
// FunctionExpr), freshly (re)derived per instantiation the same way
// Expr::resolvedType/Stmt::resolvedVarType already are (see CloneExpr's
// and CloneStmt's own comments below).
std::unique_ptr<FunctionDecl> CloneFunctionDecl(const FunctionDecl &fn) {
  auto out = std::make_unique<FunctionDecl>();
  out->loc = fn.loc;
  out->params.reserve(fn.params.size());
  for (auto &p : fn.params) {
    Param cp;
    cp.name = p.name;
    cp.loc = p.loc;
    if (p.type) cp.type = CloneTypeNode(*p.type);
    out->params.push_back(std::move(cp));
  }
  if (fn.returnType) out->returnType = CloneTypeNode(*fn.returnType);
  if (fn.body) out->body = CloneStmt(*fn.body);
  return out;
}
} // namespace

std::unique_ptr<Expr> CloneExpr(const Expr &expr) {
  auto out = std::make_unique<Expr>();
  out->kind = expr.kind;
  out->loc = expr.loc;
  // resolvedType/resolvedCalleeName are deliberately NOT copied - the
  // clone gets its own instantiation-specific ones from a fresh Sema
  // check, never the template's (which may not even be valid: a
  // template's own resolvedType can carry a TypeParam that only makes
  // sense before substitution).
  out->numberValue = expr.numberValue;
  out->boolValue = expr.boolValue;
  out->name = expr.name;
  out->op = expr.op;
  out->isLengthAccess = expr.isLengthAccess;
  out->isPostfix = expr.isPostfix;

  if (expr.lhs) out->lhs = CloneExpr(*expr.lhs);
  if (expr.rhs) out->rhs = CloneExpr(*expr.rhs);
  if (expr.operand) out->operand = CloneExpr(*expr.operand);
  for (auto &e : expr.elements) out->elements.push_back(CloneExpr(*e));
  for (auto &f : expr.fields) out->fields.emplace_back(f.first, CloneExpr(*f.second));
  for (auto &t : expr.typeArgs) out->typeArgs.push_back(CloneTypeNode(*t));
  // FunctionExpr's own `fn` - see CloneFunctionDecl's doc comment for
  // exactly what is/isn't copied.
  if (expr.fn) out->fn = CloneFunctionDecl(*expr.fn);

  return out;
}

std::unique_ptr<Stmt> CloneStmt(const Stmt &stmt) {
  auto out = std::make_unique<Stmt>();
  out->kind = stmt.kind;
  out->loc = stmt.loc;
  out->isConst = stmt.isConst;
  out->varName = stmt.varName;
  if (stmt.declaredType) out->declaredType = CloneTypeNode(*stmt.declaredType);
  // resolvedVarType is deliberately left default - filled in fresh per
  // instantiation, same reasoning as Expr::resolvedType above.

  if (stmt.cond) out->cond = CloneExpr(*stmt.cond);
  if (stmt.body) out->body = CloneStmt(*stmt.body);
  if (stmt.elseBranch) out->elseBranch = CloneStmt(*stmt.elseBranch);
  if (stmt.finallyBody) out->finallyBody = CloneStmt(*stmt.finallyBody);
  if (stmt.initStmt) out->initStmt = CloneStmt(*stmt.initStmt);
  if (stmt.update) out->update = CloneExpr(*stmt.update);
  if (stmt.expr) out->expr = CloneExpr(*stmt.expr);
  for (auto &s : stmt.statements) out->statements.push_back(CloneStmt(*s));
  // resolvedType deliberately left default here too, same reasoning as
  // resolvedVarType above - only fieldName/localName are real source,
  // re-resolved fresh per instantiation.
  for (auto &b : stmt.destructureBindings) {
    DestructureBinding cb;
    cb.fieldName = b.fieldName;
    cb.localName = b.localName;
    cb.loc = b.loc;
    out->destructureBindings.push_back(std::move(cb));
  }

  return out;
}

} // namespace ART
