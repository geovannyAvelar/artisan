#ifndef ART_AST_H
#define ART_AST_H

#include <memory>
#include <string>
#include <vector>

#include "tokenizer.h"

namespace ART {

// ---------------------------------------------------------------------
// Type syntax, as written in source (resolved into a ResolvedType by Sema)
// ---------------------------------------------------------------------

enum class TypeSyntaxKind { Number, Boolean, String, Void, Array, Named, Handler };

struct TypeNode {
  TypeSyntaxKind kind;
  SourceLoc loc;
  std::string name;                 // Named: interface/opaque type name
  std::unique_ptr<TypeNode> element; // Array: element type
  std::vector<std::unique_ptr<TypeNode>> handlerParamTypes; // Handler: e.g. "(event: Event) => void"
};

// ---------------------------------------------------------------------
// Resolved types (filled in by Sema, consumed by Codegen)
// ---------------------------------------------------------------------

enum class TypeTag { Unknown, Number, Boolean, String, Void, Array, Struct, Handler };

struct ResolvedType {
  TypeTag tag = TypeTag::Unknown;
  std::shared_ptr<ResolvedType> elementType;                    // Array
  std::string structName;                                       // Struct (interface/opaque type name)
  std::shared_ptr<std::vector<ResolvedType>> handlerParamTypes; // Handler

  bool operator==(const ResolvedType &other) const;
  bool operator!=(const ResolvedType &other) const { return !(*this == other); }
  std::string ToString() const;

  static ResolvedType Number() { return MakeSimple(TypeTag::Number); }
  static ResolvedType Boolean() { return MakeSimple(TypeTag::Boolean); }
  static ResolvedType String() { return MakeSimple(TypeTag::String); }
  static ResolvedType Void() { return MakeSimple(TypeTag::Void); }
  static ResolvedType Struct(std::string name) {
    ResolvedType t;
    t.tag = TypeTag::Struct;
    t.structName = std::move(name);
    return t;
  }
  static ResolvedType ArrayOf(ResolvedType elem) {
    ResolvedType t;
    t.tag = TypeTag::Array;
    t.elementType = std::make_shared<ResolvedType>(std::move(elem));
    return t;
  }
  // A reference to a void-returning top-level function whose parameter
  // types match `params` - ART's only function-pointer-shaped value,
  // written `(name: Type, ...) => void` (zero or more parameters; names
  // are parsed but purely decorative - only the types are matched). No
  // captures (ART functions can't close over anything but their own
  // params), so this is just a plain code address - see codegen.cpp.
  static ResolvedType Handler(std::vector<ResolvedType> params) {
    ResolvedType t;
    t.tag = TypeTag::Handler;
    t.handlerParamTypes = std::make_shared<std::vector<ResolvedType>>(std::move(params));
    return t;
  }

private:
  static ResolvedType MakeSimple(TypeTag tag) {
    ResolvedType t;
    t.tag = tag;
    return t;
  }
};

// ---------------------------------------------------------------------
// Expressions
// ---------------------------------------------------------------------

enum class ExprKind {
  NumberLiteral,
  BoolLiteral,
  StringLiteral,
  Identifier,
  Binary,
  Unary,
  IncDec,
  Call,
  ArrayLiteral,
  ObjectLiteral,
  Index,
  Member,
  Assign,
};

struct Expr {
  ExprKind kind;
  SourceLoc loc;
  ResolvedType resolvedType; // filled in by Sema

  double numberValue = 0;                    // NumberLiteral
  bool boolValue = false;                    // BoolLiteral
  std::string name;                          // Identifier / Member (field name) / Call (callee name) / StringLiteral (decoded value)
  std::string op;                            // Binary / Unary / IncDec ("++"/"--") / Assign

  std::unique_ptr<Expr> lhs;                 // Binary lhs, Assign target, Index/Member object
  std::unique_ptr<Expr> rhs;                 // Binary rhs, Assign value
  std::unique_ptr<Expr> operand;             // Unary/IncDec target, Index's bracket expression

  std::vector<std::unique_ptr<Expr>> elements; // Call args / ArrayLiteral elements
  std::vector<std::pair<std::string, std::unique_ptr<Expr>>> fields; // ObjectLiteral

  bool isLengthAccess = false; // Member: true when field is the built-in `.length` on an array/string
  bool isPostfix = false; // IncDec: `x++`/`x--` (evaluates to the OLD value) vs `++x`/`--x` (the NEW value)

  // Call to a generic function, e.g. `identity::<number>(5)` - the
  // explicit turbofish type argument list (never inferred - see
  // Sema::CheckExpr's Call case). Empty for an ordinary, non-generic
  // call. `resolvedCalleeName` is always set by Sema for a Call: either
  // the plain callee name (non-generic) or the mangled per-instantiation
  // name Codegen should actually invoke (generic) - see
  // Sema::MangleInstantiation.
  std::vector<std::unique_ptr<TypeNode>> typeArgs;
  std::string resolvedCalleeName;
};

inline std::unique_ptr<Expr> MakeExpr(ExprKind kind, SourceLoc loc) {
  auto e = std::make_unique<Expr>();
  e->kind = kind;
  e->loc = loc;
  return e;
}

// ---------------------------------------------------------------------
// Statements
// ---------------------------------------------------------------------

enum class StmtKind { VarDecl, If, While, For, ForOf, Return, ExprStmt, Block };

struct Stmt {
  StmtKind kind;
  SourceLoc loc;

  // VarDecl; ForOf also uses isConst/varName/resolvedVarType for its loop variable
  bool isConst = false;
  std::string varName;
  std::unique_ptr<TypeNode> declaredType; // optional
  ResolvedType resolvedVarType;

  // If / While / For share cond + body; If additionally uses elseBranch
  std::unique_ptr<Expr> cond;
  std::unique_ptr<Stmt> body; // ForOf also uses this
  std::unique_ptr<Stmt> elseBranch;

  // For: optional VarDecl or ExprStmt initializer, and optional update expression
  std::unique_ptr<Stmt> initStmt;
  std::unique_ptr<Expr> update;

  // Return / ExprStmt / VarDecl init / ForOf (the iterable expression)
  std::unique_ptr<Expr> expr;

  // Block
  std::vector<std::unique_ptr<Stmt>> statements;
};

inline std::unique_ptr<Stmt> MakeStmt(StmtKind kind, SourceLoc loc) {
  auto s = std::make_unique<Stmt>();
  s->kind = kind;
  s->loc = loc;
  return s;
}

// ---------------------------------------------------------------------
// Declarations
// ---------------------------------------------------------------------

struct Param {
  std::string name;
  std::unique_ptr<TypeNode> type;
  ResolvedType resolvedType;
  SourceLoc loc;
};

struct FunctionDecl {
  std::string name;
  std::vector<std::string> typeParams; // e.g. ["T", "U"] for `function foo<T, U>(...)`; empty if not generic
  std::vector<Param> params;
  std::unique_ptr<TypeNode> returnType; // never null - defaults to Void
  ResolvedType resolvedReturnType;
  std::unique_ptr<Stmt> body; // Block; always null for a `declare function` OR an uninstantiated generic template
  SourceLoc loc;
};

// A deep copy of a Stmt/Expr subtree - used to give each concrete
// instantiation of a generic function's body its own AST, since
// Sema::CheckStmt/CheckExpr mutate resolvedType in place and two
// instantiations (e.g. identity::<number> and identity::<string>) must
// not share nodes or one would clobber the other's annotations.
std::unique_ptr<Stmt> CloneStmt(const Stmt &stmt);
std::unique_ptr<Expr> CloneExpr(const Expr &expr);

struct InterfaceField {
  std::string name;
  std::unique_ptr<TypeNode> type;
  ResolvedType resolvedType;
  SourceLoc loc;
};

struct InterfaceDecl {
  std::string name;
  std::vector<InterfaceField> fields;
  SourceLoc loc;
  bool isOpaque = false; // `declare type Name;` - a foreign handle with no
                          // accessible fields, never constructible with `{}`
};

struct Program {
  std::vector<std::unique_ptr<InterfaceDecl>> interfaces;
  std::vector<std::unique_ptr<FunctionDecl>> functions;
  std::vector<std::unique_ptr<FunctionDecl>> externFunctions; // `declare function ...;` - body is always null
  // Top-level `let`/`const` (each a StmtKind::VarDecl) - handler state that
  // outlives any single setupApp/function call. Restricted to a literal
  // number/boolean/string initializer (see Sema::CheckGlobalDecl): ART has
  // no static-initialization-order mechanism to run arbitrary code before
  // setupApp/main, and an array/interface global would need a `malloc`
  // call, which can never be a compile-time constant initializer anyway.
  std::vector<std::unique_ptr<Stmt>> globals;
};

} // namespace ART

#endif
