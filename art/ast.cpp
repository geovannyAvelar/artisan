#include "ast.h"

namespace ART {

bool ResolvedType::operator==(const ResolvedType &other) const {
  if (tag != other.tag) return false;
  switch (tag) {
  case TypeTag::Array:
    return *elementType == *other.elementType;
  case TypeTag::Struct:
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
  if (stmt.initStmt) out->initStmt = CloneStmt(*stmt.initStmt);
  if (stmt.update) out->update = CloneExpr(*stmt.update);
  if (stmt.expr) out->expr = CloneExpr(*stmt.expr);
  for (auto &s : stmt.statements) out->statements.push_back(CloneStmt(*s));

  return out;
}

} // namespace ART
