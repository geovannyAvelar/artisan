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
  std::vector<std::unique_ptr<TypeNode>> genericArgs;       // Named: e.g. "Box<number>" - empty if not generic
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
  // A .tsx-only element literal, e.g. `<li class="item">{label}</li>` -
  // see Parser::ParseJsxElement. Reuses ObjectLiteral's `fields` for
  // attributes (name -> value expression, same "name: value" pair shape)
  // and Call/ArrayLiteral's `elements` for children (each either another
  // JsxElement or a plain `{ expr }` interpolation) rather than adding
  // JSX-specific fields - `name` is the tag itself. Resolves to type Node
  // (see Sema::CheckExpr) - a real expression, not restricted to
  // statement position, the same way an ObjectLiteral already builds a
  // heap value through a sequence of instructions before yielding it.
  //
  // A *fragment*, `<>child*</>`, is the same node with `name` left empty
  // (a real tag never is) - no element gets created and no attributes are
  // legal, and it resolves to `Node[]` instead of `Node`: an ordered
  // group of its children with no wrapping element of its own, letting a
  // helper function return several sibling nodes (or a variable-length
  // list built from a `Node[]` child, spread the same as anywhere else -
  // see Codegen's own JsxElement case) without an unwanted wrapper tag.
  JsxElement,
  // `cond ? thenExpr : elseExpr` - reuses Unary/IncDec's `operand` for
  // `cond` and Binary/Assign's `lhs`/`rhs` for the two branches, rather
  // than adding Conditional-specific fields. Both branches must resolve
  // to the same type (see Sema::CheckExpr) - that shared type is this
  // expression's own resolvedType.
  Conditional,
};

struct Expr {
  ExprKind kind;
  SourceLoc loc;
  ResolvedType resolvedType; // filled in by Sema

  double numberValue = 0;                    // NumberLiteral
  bool boolValue = false;                    // BoolLiteral
  std::string name;                          // Identifier / Member (field name) / Call (callee name) / StringLiteral (decoded value) / JsxElement (tag name)
  std::string op;                            // Binary / Unary / IncDec ("++"/"--") / Assign

  std::unique_ptr<Expr> lhs;                 // Binary lhs, Assign target, Index/Member object, Conditional then-branch
  std::unique_ptr<Expr> rhs;                 // Binary rhs, Assign value, Conditional else-branch
  std::unique_ptr<Expr> operand;             // Unary/IncDec target, Index's bracket expression, Conditional condition

  std::vector<std::unique_ptr<Expr>> elements; // Call args / ArrayLiteral elements / JsxElement children
  std::vector<std::pair<std::string, std::unique_ptr<Expr>>> fields; // ObjectLiteral / JsxElement attributes (name -> value)

  bool isLengthAccess = false; // Member: true when field is the built-in `.length` on an array/string
  bool isPostfix = false; // IncDec: `x++`/`x--` (evaluates to the OLD value) vs `++x`/`--x` (the NEW value)
  // Call: true when `lhs` is a Handler-*valued* expression (a variable,
  // array element, ...) rather than a named function or class method -
  // Codegen evaluates `lhs` itself to get the callee (a computed
  // function-pointer value) and emits an indirect call, instead of
  // looking `resolvedCalleeName` up in its own function table. See
  // Sema::CheckExpr's Call case.
  bool isIndirectCall = false;

  // Call to a generic function, e.g. `identity::<number>(5)` - the
  // explicit turbofish type argument list (never inferred - see
  // Sema::CheckExpr's Call case). Empty for an ordinary, non-generic
  // call. `resolvedCalleeName` is always set by Sema for a Call: either
  // the plain callee name (non-generic) or the mangled per-instantiation
  // name Codegen should actually invoke (generic) - see
  // Sema::MangleInstantiation.
  //
  // Reused for one other case: on a Member whose property is backed by a
  // `set` accessor (see FunctionDecl::isSetter) and that Member is the
  // target of an Assign, this holds the setter's own mangled name -
  // Codegen's Assign case checks it to decide between a plain store and
  // a setter call (see its own comment). Empty/unused for every other
  // Member (a Member backed by a `get` accessor is rewritten into a Call
  // in place instead - see Sema::CheckExpr's Member case - so it never
  // needs this).
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
  // Only meaningful for a top-level VarDecl (a Program::globals entry) -
  // see FunctionDecl's own isExported/sourceFile for what these mean.
  bool isExported = false;
  std::string sourceFile;

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
  // Which file this declaration came from (its resolved, canonical path -
  // see ModuleResolver), and whether `export` prefixed it - both empty/
  // false for a single-file compilation with no imports at all (the
  // ModuleResolver is skipped entirely there - see main.cpp), in which
  // case Sema enforces no cross-file visibility restriction whatsoever,
  // matching the language's original single-file-only behavior exactly.
  std::string sourceFile;
  bool isExported = false;

  // Only ever true for a class method (InterfaceDecl::methods) parsed
  // from `get name(): T { ... }`/`set name(value: T): void { ... }`
  // (see Parser::ParseAccessor) - mutually exclusive, both false for an
  // ordinary method or any top-level function. Accessed as a property,
  // never with call syntax: `obj.name`/`obj.name = value` rather than
  // `obj.name()` - see Sema::CheckExpr's Member/Assign handling for how
  // that's actually wired (a getter is rewritten in place into an
  // ordinary zero-arg Call; a setter-backed Assign keeps its own Assign
  // shape but calls through Expr::resolvedCalleeName instead of storing
  // to a field - see that field's own doc comment). Qualified with a
  // "$get$"/"$set$" infix (see Sema::Check) rather than plain method's
  // "$", so a getter and setter sharing the same property name - a
  // read/write pair - never collide as two identically-named top-level
  // functions once qualified.
  bool isGetter = false;
  bool isSetter = false;
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
  std::vector<std::string> typeParams; // e.g. ["T"] for `interface Box<T>`; empty if not generic
  std::vector<InterfaceField> fields;
  // Non-empty only for a `class`/`declare class` (a plain `interface` never
  // has any) - `class Foo { ... function bar(...): T { ... } ... }` and
  // `declare class` share this same InterfaceDecl representation rather
  // than being a distinct AST node: a class is exactly an interface
  // (same heap-allocated-struct-of-fields representation, same `Struct`
  // ResolvedType, same opaque/non-opaque split) that additionally owns a
  // set of methods. Each method's parser-injected first parameter (named
  // "this", typed as this very class) is its implicit receiver;
  // `obj.method(args)` is pure call-site sugar (see Sema::CheckExpr's
  // Call case) for a plain call to that method with `obj` spliced in as
  // the actual first argument - resolved statically against obj's
  // declared type, same as everything else in ART (no vtable, no
  // dynamic dispatch, classes can't be generic yet). Sema qualifies each
  // method's own `name` to "ClassName$methodName" and registers/checks
  // it exactly like any other top-level function (see Sema::Check) -
  // Codegen never needs to know a method came from a class at all.
  std::vector<std::unique_ptr<FunctionDecl>> methods;
  SourceLoc loc;
  bool isOpaque = false; // `declare type Name;`/`declare class Name { ... }` -
                          // a foreign handle with no accessible fields, never
                          // constructible with `{}` (a declare class's methods
                          // are still real ART code, though - typically thin
                          // wrappers around `declare function`s)
  std::string sourceFile; // see FunctionDecl's own doc comment
  bool isExported = false;
};

// `import { name, ... } from "path";` - always at the top of a file,
// before any other declaration (see Parser::ParseProgram). `path` is
// resolved relative to the importing file's own directory by
// ModuleResolver, `.ts` implied if not written; each `name` must be a
// top-level declaration in the target file marked `export`.
struct ImportedName {
  std::string name;
  SourceLoc loc;
};

struct ImportDecl {
  std::string path;
  SourceLoc loc;
  std::vector<ImportedName> names;
};

struct Program {
  // Only ever populated by the parser for ONE file at a time - see
  // ModuleResolver, which resolves/merges every file an entry point
  // (transitively) imports into a single, final Program with these left
  // empty (every import already flattened into interfaces/functions/
  // .../globals below, each tagged with its own sourceFile/isExported).
  std::vector<std::unique_ptr<ImportDecl>> imports;

  std::vector<std::unique_ptr<InterfaceDecl>> interfaces;
  std::vector<std::unique_ptr<FunctionDecl>> functions;
  std::vector<std::unique_ptr<FunctionDecl>> externFunctions; // `declare function ...;` - body is always null
  // Top-level `let`/`const` (each a StmtKind::VarDecl) - handler state
  // that outlives any single setupApp/function call. Any type, any
  // initializer expression (see Sema::CheckGlobalDecl) - a bare number/
  // boolean/string literal becomes a real compile-time constant;
  // anything else (a call, an object/array literal, ...) is computed
  // once, in declaration order, by a generated module constructor (see
  // Codegen::GenGlobalInit) that runs before main/setupApp, the same
  // "real code, run once, before anything else" mechanism the garbage
  // collector's own initialization already uses.
  std::vector<std::unique_ptr<Stmt>> globals;
  // Bare top-level statements (an `if`/`while`/`for`/block/expression -
  // never a VarDecl, which always goes to `globals` above instead) - the
  // procedural alternative to writing an explicit `function setupApp():
  // void { ... }`: collected, across every file in the merged program
  // (dependency-first order, same as `globals`), into a generated
  // function literally named "setupApp" (see Codegen::GenSetupAppBody) -
  // main.cpp's existing trampoline already calls exactly that symbol
  // once per page load, so nothing about the C++ integration needs to
  // change. Mutually exclusive with an explicit `function setupApp()` -
  // see Sema::Check. Unlike `globals` (initialized once, at process
  // start, via a module constructor), these run every time the
  // generated `setupApp` itself is called - the two lists exist
  // specifically because those are different lifetimes, not
  // interchangeable ways to write the same thing.
  std::vector<std::unique_ptr<Stmt>> topLevelStmts;
};

} // namespace ART

#endif
