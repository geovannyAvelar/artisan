#ifndef ART_AST_H
#define ART_AST_H

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "tokenizer.h"

namespace ART {

// Bare identifiers that are pure call-site sugar for a zero-arg function
// call, e.g. `document` for `ArtDocument()` - see Sema::CheckExpr's
// Identifier case for how this is actually applied (rewriting the Expr
// in place) and its own doc comment for the precedence rule (a real
// local/global always shadows this). Every entry's backing function
// still has to be declared by the project itself (this table doesn't
// declare anything on its own) - unlike a true builtin (e.g.
// numberToString), `ArtDocument` isn't self-contained: it depends on the
// project's own `Node`/`declare function ArtDocument` and the artisan
// runtime's C++ symbol behind it. `window` isn't included here yet -
// nothing in the DOM bridge is window-level (no timers/alerts/
// location/... exposed to ART yet), so there's nothing to desugar it to.
//
// Lives here (not sema.cpp, where it originated) because Parser also
// needs it - a raw, pre-resolution name check on a top-level `let`/
// `const` initializer (see Parser::ParseProgram), to automatically treat
// a document-touching one as a per-page-load local instead of rejecting
// it as an illegal persistent-global initializer (Sema::CheckGlobalDecl
// still rejects the one case that can't be rescued this way: an
// `export`ed one, which needs to be a real global by definition).
// `inline` so both translation units share one definition (C++17
// inline variables), not two independent copies that could drift.
inline const std::unordered_map<std::string, std::string> kAmbientGlobals = {
    {"document", "ArtDocument"},
};

// ---------------------------------------------------------------------
// Type syntax, as written in source (resolved into a ResolvedType by Sema)
// ---------------------------------------------------------------------

// Nullable: `T | null` - see ResolvedType's own TypeTag::Nullable doc
// comment for the full story (this is the one and only union shape ART
// supports - not general `T1 | T2` unions).
// Any: `any` - see TypeTag::Any's own doc comment.
enum class TypeSyntaxKind { Number, Boolean, String, Void, Array, Named, Handler, Nullable, Any };

struct TypeNode {
  TypeSyntaxKind kind;
  SourceLoc loc;
  std::string name;                 // Named: interface/opaque type name
  std::unique_ptr<TypeNode> element; // Array: element type; Nullable: the wrapped type (the `T` in `T | null`)
  std::vector<std::unique_ptr<TypeNode>> handlerParamTypes; // Handler: e.g. "(event: Event) => void"
  std::vector<std::unique_ptr<TypeNode>> genericArgs;       // Named: e.g. "Box<number>" - empty if not generic
};

// ---------------------------------------------------------------------
// Resolved types (filled in by Sema, consumed by Codegen)
// ---------------------------------------------------------------------

// Enum: a real, distinct type from Number - see EnumDecl's own doc
// comment for exactly what that buys (mainly: `let c: Color = 5;` is a
// type error, only `Color.Red`/another already-`Color`-typed value
// works) - even though its runtime representation (MapType, Codegen) is
// the exact same `double` a plain Number already is.
//
// Nullable: `T | null` - ART's ONE and only union shape (see
// art/README.md's "Optional values"/"Nullable types" sections for why:
// general `T1 | T2` unions need real member-access/narrowing rules
// across arbitrarily many cases, which is a much bigger feature than
// "this one specific type might be absent"). Represented uniformly as a
// boxed `ptr` at the Codegen level - a fresh GC cell holding the real T,
// or a null pointer for "absent" - REGARDLESS of what T's own
// unwrapped representation would otherwise be (a plain `double`/`i1`
// has no bit pattern that could double as "absent" without colliding
// with a real value, unlike a pointer, where null already does). Reading
// a Nullable(T) as a plain T needs narrowing first (see Expr::
// isNarrowedNonNull) - there's no implicit "just trust it's there".
// Any: `any` - the one type ART lets flow completely dynamically, real
// TS-style. Represented uniformly (same "one box shape regardless of
// what's inside" trick Nullable(T) already uses - see its own doc
// comment) as a boxed `ptr`: a fresh GC cell `{ i32 tag, ptr payload }`,
// `tag` one of AnyTag's values (below) saying what's actually in it right
// now, `payload` the value itself - a heap cell holding a copy for
// Number/Boolean/Enum (none of which have a spare pointer-shaped slot of
// their own to reuse), or the value's OWN pointer directly for anything
// that's already `ptr`-shaped (String/Struct/Array), or a heap cell
// holding the {fn,env} pair for Handler (see GetHandlerStructType's own
// doc comment for why that one's a 2-word aggregate, not a bare pointer).
//
// Unlike Nullable(T), assigning a plain, concrete value where `any` is
// expected widens IMPLICITLY (see Sema::CheckExpr's own tail
// actual-vs-expected check, and Expr::needsAnyBox/preBoxType) - that's
// the entire point of `any` in real TS, and disallowing it would defeat
// the feature. Getting a concrete value back OUT needs `typeof x ===
// "..."` narrowing first (see AnyTag, Sema::TryGetTypeofCheckedVar) -
// there's no implicit narrowing the other direction, same "prove it
// first" discipline Nullable(T) already has.
//
// Two real, deliberate limits, both because generalizing them needs
// runtime type identification ART doesn't have (see "What's not in
// ART"'s own "no downcasting or instanceof" bullet):
// - A `T | null` value can't be assigned into an `any` directly (box it
//   as its own tag once narrowed to plain T first) - `any` and Nullable
//   are two separate, non-interacting escape hatches in this version.
// - `typeof x === "object"` / `"function"` never narrows anything (see
//   AnyTag::Object's own doc comment for why: `any` erases exactly
//   which struct/array/handler-signature was boxed, the same "no
//   instanceof, no downcasting" gap real inheritance already has) -
//   only `"number"`/`"boolean"`/`"string"`, the three tags with one
//   single, unambiguous concrete type to narrow back to, actually do.
enum class TypeTag { Unknown, Number, Boolean, String, Void, Array, Struct, Handler, Enum, Nullable, Any };

// The runtime tag stored in every `any` value's own box (see TypeTag::
// Any's own doc comment) - one entry per distinct `typeof` result ART
// recognizes. Object covers BOTH a struct instance and an array -
// indistinguishable via `typeof` alone, exactly like real JS's own
// `typeof [1, 2, 3] === "object"` - and Null is `any`'s own `null`
// literal (`let x: any = null;`), which real JS also reports as
// `typeof null === "object"`, a deliberately-preserved quirk, not an
// oversight, since ART's `any` otherwise tries to match real JS/TS
// `typeof` behavior exactly.
enum class AnyTag { Number, Boolean, String, Object, Function, Null };

struct ResolvedType {
  TypeTag tag = TypeTag::Unknown;
  std::shared_ptr<ResolvedType> elementType;                    // Array
  std::string structName;                                       // Struct (interface/opaque type name); Enum (its own name)
  std::shared_ptr<std::vector<ResolvedType>> handlerParamTypes; // Handler

  bool operator==(const ResolvedType &other) const;
  bool operator!=(const ResolvedType &other) const { return !(*this == other); }
  std::string ToString() const;

  static ResolvedType Number() { return MakeSimple(TypeTag::Number); }
  static ResolvedType Boolean() { return MakeSimple(TypeTag::Boolean); }
  static ResolvedType String() { return MakeSimple(TypeTag::String); }
  static ResolvedType Void() { return MakeSimple(TypeTag::Void); }
  static ResolvedType Any() { return MakeSimple(TypeTag::Any); }
  static ResolvedType Struct(std::string name) {
    ResolvedType t;
    t.tag = TypeTag::Struct;
    t.structName = std::move(name);
    return t;
  }
  static ResolvedType Enum(std::string name) {
    ResolvedType t;
    t.tag = TypeTag::Enum;
    t.structName = std::move(name);
    return t;
  }
  static ResolvedType ArrayOf(ResolvedType elem) {
    ResolvedType t;
    t.tag = TypeTag::Array;
    t.elementType = std::make_shared<ResolvedType>(std::move(elem));
    return t;
  }
  // `T | null` - `elem` (reusing Array's own field) is the wrapped `T`.
  // Nullable(Nullable(T)) is nonsensical and never constructed - Sema's
  // own ResolveType rejects `T | null | null` at the syntax level before
  // this could ever be called with an already-Nullable `elem`.
  static ResolvedType NullableOf(ResolvedType elem) {
    ResolvedType t;
    t.tag = TypeTag::Nullable;
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

struct FunctionDecl; // forward decl - Expr::fn needs it before FunctionDecl itself is defined below

enum class ExprKind {
  NumberLiteral,
  BoolLiteral,
  StringLiteral,
  // `null` - always needs an expected Nullable(T) type to resolve
  // against (same "can't infer from nothing" deal an empty array
  // literal has) - see Sema::CheckExpr's own NullLiteral case.
  NullLiteral,
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
  // `` `text ${expr} more` `` - see Parser::ParseTemplateLiteral and
  // Tokenizer's TemplateStringMiddle/Tail tokens. Reuses ArrayLiteral's
  // own `elements` for an alternating, always-odd-length sequence of
  // literal-text StringLiteral parts and interpolated expressions -
  // `part[0], expr[1], part[2], expr[3], ..., part[2n]` for n
  // interpolations (`n == 0` is legal: a template with none is just one
  // element, its whole literal text). Each interpolated expression must
  // resolve to `string` or `number` (a `number` is stringified via
  // `numberToString`, same as everywhere else in ART - no implicit
  // conversion for anything else, matching JSX's own child-type rule).
  // Always resolves to `string`.
  TemplateLiteral,
  // Anonymous `function(params): void { body }`, used as a Handler value
  // wherever one's expected (a `let` init, a call argument, a class
  // field init, an array/object element, a return value) - ART's only
  // function-EXPRESSION form, as opposed to a top-level `function`
  // *declaration* or a class method (see Parser::ParseFunctionExpr).
  // Unlike a plain function reference, a FunctionExpr can capture -
  // reference - locals from its enclosing function/closure; see `fn`
  // below and FunctionDecl::captures.
  FunctionExpr,
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

  std::vector<std::unique_ptr<Expr>> elements; // Call args / ArrayLiteral elements / JsxElement children / TemplateLiteral parts
  std::vector<std::pair<std::string, std::unique_ptr<Expr>>> fields; // ObjectLiteral / JsxElement attributes (name -> value)

  bool isLengthAccess = false; // Member: true when field is the built-in `.length` on an array/string
  bool isPostfix = false; // IncDec: `x++`/`x--` (evaluates to the OLD value) vs `++x`/`--x` (the NEW value)

  // Identifier only - true iff Sema's own narrowing (see its own
  // narrowedNonNull member and CheckStmt's If case) proved, at THIS
  // specific reference, that a Nullable(T)-declared variable can't
  // actually be null right now (an `if (x != null) { ...here... }`
  // check, or code after an `if (x == null) { <exits> }` with no
  // `else`) - deliberately per-reference, not per-declaration: the SAME
  // variable can be narrowed at one use and not another, and Sema
  // proves it fresh each time rather than caching a blanket "this
  // variable is now always non-null" fact. When true, `resolvedType` is
  // already the UNWRAPPED T (not Nullable(T)) - Codegen reads this flag
  // to know it must unbox the variable's own boxed-cell storage (an
  // extra load through the pointer) to actually produce a T-typed
  // value, rather than returning the raw boxed pointer the variable is
  // really stored as. This is the ONLY place narrowing has any runtime
  // effect at all - a deliberately narrow, single-choke-point design
  // rather than threading a temporarily-different type through every
  // other expression kind that might reference this variable.
  bool isNarrowedNonNull = false;
  // Identifier only - the `any`-flavored counterpart to isNarrowedNonNull
  // above, set when Sema's own `typeof x === "..."` narrowing (see
  // narrowedAny, TryGetTypeofCheckedVar) proved, at THIS specific
  // reference, that an Any-declared variable currently holds one of the
  // three narrowable concrete shapes (number/boolean/string - see
  // TypeTag::Any's own doc comment for why not object/function too).
  // Same per-reference, proven-fresh-each-time discipline
  // isNarrowedNonNull already has, and the same "resolvedType is already
  // the narrowed concrete type, Codegen just needs to know to unbox"
  // shape: reads the variable's own boxed-any storage, then unwraps
  // `payload` according to which concrete type this narrowed to.
  bool isNarrowedAny = false;
  // True when Sema's own actual-vs-expected tail check (see CheckExpr)
  // widened a concrete, boxable value (see TypeTag::Any's own doc
  // comment for exactly which tags qualify) into an `any` implicitly -
  // `resolvedType` is already TypeTag::Any at that point, so `preBoxType`
  // is where Codegen finds the ORIGINAL concrete type it actually needs
  // to know how to box (GenBoxAny branches on it to decide the runtime
  // AnyTag and how to build the payload). Never set alongside
  // isNarrowedNonNull/isNarrowedAny (those UN-box; this BOXES) - the two
  // directions never overlap on the same reference.
  bool needsAnyBox = false;
  ResolvedType preBoxType; // valid only when needsAnyBox is true
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

  // Call only, and only for an instance method call (`obj.method(args)`)
  // whose resolved method is FunctionDecl::isVirtual - the vtable slot
  // to dispatch through (same value as that method's own vtableSlot),
  // rather than calling `resolvedCalleeName` directly the way every
  // other Call does. -1 (the default) means "not virtual" - an ordinary
  // direct call, unaffected by any of this (the overwhelming majority of
  // calls, and the only kind that existed before inheritance did).
  // `resolvedCalleeName` is still set alongside this (to whichever
  // method's own implementation Sema resolved statically), but Codegen's
  // own Call case ignores it in favor of the indirect dispatch this
  // triggers - see its own doc comment for why it's kept anyway.
  int virtualSlot = -1;

  // FunctionExpr only - owns the closure's own params/body, reusing
  // FunctionDecl's existing shape wholesale rather than inventing a
  // parallel node. Sema assigns `fn->name` a synthesized, globally-unique
  // symbol ("$closureN") the first time it actually checks this node
  // (never earlier - a generic template's own FunctionExpr, never
  // checked in template form, never gets one; each concrete
  // instantiation's own clone gets its own name/captures when Sema
  // checks *it*). Sema also fills in `fn->captures` here - see
  // FunctionDecl::captures' own doc comment.
  std::unique_ptr<FunctionDecl> fn;
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

enum class StmtKind {
  VarDecl,
  If,
  While,
  For,
  ForOf,
  Return,
  ExprStmt,
  Block,
  // `break;`/`continue;` - no fields of their own; see Sema's
  // loopDepth/switchDepth (break: either > 0; continue: loopDepth > 0)
  // for where each is validated, and Codegen's breakTargets stack for
  // where each is actually generated.
  Break,
  Continue,
  // `do { body } while (cond);` - reuses While's own cond/body fields;
  // the only difference from While is Codegen running the body once
  // before the first condition check (see Codegen::GenStmt's own
  // DoWhile case) - Sema's own check is textually identical to While's.
  DoWhile,
  // `switch (expr) { case v1: stmts case v2: stmts default: stmts }` -
  // `expr` (reusing Return/ExprStmt's own field) is the discriminant;
  // `statements` (reusing Block's own field) holds one StmtKind::Case
  // per arm, in source order. A real switch, not a chain of ifs: cases
  // fall through into the next one unless a `break` ends them, matching
  // real TS/JS semantics exactly now that `break` exists.
  Switch,
  // One arm of a Switch, always a direct child of its `statements` -
  // never appears anywhere else. `expr` (null for `default:`) is the
  // case's own value, checked for equality against the switch's
  // discriminant; `statements` is this arm's own statement list, no
  // implicit block scope of its own (matching real JS: `let x` in one
  // case is visible to a later case it falls through into).
  Case,
  // `let { a, b: renamed } = expr;` - object (interface/class struct)
  // destructuring only, see Stmt::destructureBindings' own doc comment
  // for exactly what that means and why array destructuring/defaults
  // aren't included. `expr` (reusing Return/ExprStmt's own field) is the
  // struct being destructured, evaluated exactly once regardless of how
  // many bindings read from it; `isConst` applies to every binding alike
  // (there's no per-binding `let`/`const` mix - matching how a plain
  // `let x = 1, y = 2;` isn't legal in ART either, only one declaration
  // per statement).
  Destructure,
  // `try { body } catch (name: Error) { catchBody }` - reuses `body` for
  // the try block (same field While/DoWhile/If already use) and
  // `elseBranch` for the catch block (same field If's own `else` uses -
  // Try never needs an actual else, so this is free to reuse), plus
  // VarDecl's own varName/declaredType/resolvedVarType for the catch
  // binding. No `finally` yet, and `declaredType` must resolve to
  // exactly `Error` (Sema rejects anything else) - see the builtin
  // `Error` interface's own doc comment (Sema::SeedBuiltins) for why:
  // with only one throwable type possible, every active handler always
  // matches whatever's thrown, so there's no type-tag-based selective
  // catching to get wrong yet - real polymorphic catching needs runtime
  // type identification, which doesn't exist in ART yet.
  Try,
  // `throw expr;` - reuses Return/ExprStmt's own `expr` field. `expr`
  // must resolve to exactly `Error` - see StmtKind::Try's own doc
  // comment for why only one throwable type exists right now.
  Throw,
};

// One `name` or `name: renamed` inside a Destructure statement's `{...}`
// pattern - see StmtKind::Destructure's own doc comment.
struct DestructureBinding {
  std::string fieldName; // the source struct's own field name
  std::string localName; // the new local's name - == fieldName unless renamed
  SourceLoc loc;
  ResolvedType resolvedType; // filled in by Sema (the field's own type)
};

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

  // VarDecl only (ForOf reuses this the same way it already reuses
  // isConst/varName/resolvedVarType above) - true iff some closure
  // nested inside this variable's own owning function/closure captures
  // it, set by Sema::Lookup (see its own doc comment), read by Codegen's
  // `let`/ForOf codegen to decide plain-alloca vs. heap-boxed-cell
  // storage. Never true for anything Sema didn't itself mark - Codegen
  // never infers this independently. Deliberately NOT copied by
  // CloneStmt, same as resolvedVarType isn't - see CloneStmt's own
  // top-of-file comment.
  //
  // Destructure also reuses this - but per WHOLE PATTERN, not per
  // binding: if any one of a `let {a, b} = x;`'s locals is captured,
  // Codegen boxes every binding from that statement, even ones that
  // individually aren't (see its own Destructure case) - simpler than
  // threading capture tracking through Declare/VarInfo on a per-binding
  // basis, and safe (boxing unnecessarily is never wrong, just slightly
  // wasteful).
  bool isCapturedByClosure = false;

  // VarDecl (a Program::globals entry) only - true only for an enum
  // member's own synthetic global (see Sema::Check's enum-registration
  // pass, which constructs one per member, fully resolved already:
  // resolvedVarType and expr->resolvedType are both set to the member's
  // real Enum(enumName) type up front). Tells CheckGlobalDecl's own
  // per-global loop to skip re-deriving/re-checking this one entirely -
  // running it through the ordinary path would recompute its type from
  // the bare NumberLiteral initializer alone (always plain `number`,
  // CheckExpr's NumberLiteral case has no way to know it's "supposed to"
  // be some Enum instead) and then reject its own already-correct
  // Enum-typed resolvedVarType as a mismatch against that. Never true
  // for anything else - an ordinary global (including a static field)
  // always goes through the real check, since its initializer genuinely
  // does need one.
  bool isPreCheckedGlobal = false;

  // If / While / For share cond + body; If additionally uses elseBranch
  std::unique_ptr<Expr> cond;
  std::unique_ptr<Stmt> body; // ForOf also uses this
  std::unique_ptr<Stmt> elseBranch;

  // For: optional VarDecl or ExprStmt initializer, and optional update expression
  std::unique_ptr<Stmt> initStmt;
  std::unique_ptr<Expr> update;

  // Return / ExprStmt / VarDecl init / ForOf (the iterable expression) /
  // Switch (the discriminant) / Case (the case value, null for
  // `default:`) / Destructure (the struct being destructured)
  std::unique_ptr<Expr> expr;

  // Block / Switch (one StmtKind::Case per arm) / Case (that arm's own
  // statement list)
  std::vector<std::unique_ptr<Stmt>> statements;

  // Destructure only - see StmtKind::Destructure's own doc comment.
  std::vector<DestructureBinding> destructureBindings;
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

  // True iff some closure nested inside this parameter's own owning
  // function/closure captures it - same meaning/lifecycle as
  // Stmt::isCapturedByClosure, just for a parameter instead of a
  // `let`/`const` local. See that field's own doc comment.
  bool isCapturedByClosure = false;

  // `function f(...args: T[]): void` - only legal on a plain top-level
  // function's own LAST parameter (see Sema::RegisterFunctionSignature;
  // rejected on a class method/generic function/closure/`declare
  // function` in this first pass - see its own error message for why).
  // `type`/`resolvedType` are the parameter's own DECLARED type exactly
  // as written (always `T[]`, same array type the body sees `args` as -
  // there's no separate "element type" spelling the way real TS's
  // `...args: T` shorthand has) - a call site's own trailing arguments
  // (from this parameter's position onward) are collected into a real,
  // freshly allocated array at the call, checked one at a time against
  // the array's own element type (see CheckExpr's Call case) - not
  // "spread" syntax at the call site, which this pass doesn't add (see
  // README's own note on the gap that leaves: an existing `T[]` a caller
  // already has can't be forwarded directly into a rest parameter,
  // only a literal, fixed-at-the-call-site argument list can).
  bool isRest = false;
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

  // True only for a `declare function` (or a generic instantiation of
  // one) - a real native-ABI boundary, as opposed to a body-less
  // *internal* FunctionDecl Codegen hand-generates itself
  // (numberToString/stringToNumber/makeArray<T> - see Sema's own
  // builtin-seeding). Deliberately NOT inferred from `body == nullptr`:
  // that's also true for those builtins, and at least one of them
  // (makeArray<T>) can genuinely be called with a Handler-typed argument
  // (see art/tests/signals.ts's `makeArray::<() => void>(...)`) -
  // conflating the two would wrongly apply the native-bridge
  // Handler-unpacking ABI rule (see Codegen's declare-function-call
  // codegen) to an ordinary internal call. Set by
  // Parser::ParseDeclareFunction; copied through by Sema when cloning a
  // generic declare-function template's instantiation.
  bool isExtern = false;

  // True only for a class method parsed from `static function name(...)`
  // (see Parser::ParseClassBody) - no implicit `this` receiver (the
  // parser skips InjectImplicitThis for one of these), called as
  // `ClassName.name(args)` instead of `instance.name(args)`. Mangled the
  // same "$static$" way a static field is (see
  // InterfaceDecl::staticFields' own doc comment and Sema::MangleStatic)
  // rather than a plain method's "ClassName$name", so a static and an
  // instance member can share a name with no collision - matching real
  // TS/JS, where the two live in genuinely separate namespaces. Once
  // qualified, a static method is registered/checked exactly like any
  // other top-level function (see Sema::Check) - Codegen never needs to
  // know a function is "static" at all, only that it has no implicit
  // first parameter, which is already just what its own `params` list
  // says.
  bool isStatic = false;

  // True only for a plain instance method (never a getter/setter/static/
  // extern one - see Sema::Check's own inheritance pass) belonging to a
  // class that's actually part of an inheritance relationship (has a
  // base, or is one) - see InterfaceDecl::baseClass's own doc comment
  // for why that scoping matters. `vtableSlot` is this method's own
  // index into every class-in-that-family's vtable (the SAME slot
  // number across the whole family - e.g. Animal's and Dog's own
  // "speak" both live at slot 0 - is what makes an override actually
  // override: calling through slot 0 on a Dog instance reaches Dog's
  // own function pointer, on a plain Animal instance reaches Animal's).
  // Assigned once, in base-to-derived order, when the family's own
  // virtual method set is first computed - see Sema::Check.
  bool isVirtual = false;
  int vtableSlot = -1;

  // Non-empty only for a closure (ExprKind::FunctionExpr's own `fn`)
  // that actually references something from an enclosing frame - see
  // Sema::Lookup's own doc comment for exactly how/when this gets
  // populated, including the "thread-through" case (an intermediate
  // closure that doesn't itself use a variable but still needs to carry
  // its cell pointer to a closure nested inside IT). Always empty for a
  // plain top-level function or class method. Order matters: it's also
  // the closure's own env struct's field order (see Codegen's
  // FunctionExpr codegen and GenClosureFunction).
  struct CapturedVar {
    std::string name;
    ResolvedType type;
  };
  std::vector<CapturedVar> captures;
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
  // `readonly name: Type;` - still set the normal way, through a `{}`
  // object literal at construction (an interface/class instance is
  // never built any other way - see InterfaceDecl's own doc comment),
  // but rejected as an assignment/increment-decrement TARGET afterward
  // (see Sema::CheckLValueTarget's own Member case) - the read path
  // (CheckExpr's Member case) is completely unaffected, same as a local
  // `const`'s own read path already is. Purely a Sema-time restriction;
  // Codegen needs no changes at all; since the write path this would
  // reach is simply never generated for a rejected program.
  bool isReadonly = false;
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
  // the actual first argument - classes still can't be generic. A class
  // outside any inheritance relationship at all resolves this statically
  // against obj's own declared type, same as always (Sema qualifies the
  // method's own `name` to "ClassName$methodName" and registers/checks
  // it exactly like any other top-level function - Codegen never needs
  // to know a method came from a class at all). A class that DOES
  // extend, or get extended, is different - see baseClassName/baseClass
  // below for what changes for those.
  std::vector<std::unique_ptr<FunctionDecl>> methods;
  // `class Dog extends Animal { ... }` - `baseClassName` is the raw
  // source spelling ("Animal" above), empty if there's no `extends` at
  // all; `baseClass` is Sema's own resolved pointer to it, filled in
  // during class registration (see Sema::Check) once every class name is
  // known. Single inheritance only, classes only - a plain `interface`
  // or `declare class` can't extend anything, and a class can't extend
  // either of those either (see Sema::Check's own validation).
  //
  // Only a class actually touched by *some* extends relationship (has a
  // baseClass, or IS one for something else) gets real virtual dispatch
  // - see FunctionDecl::isVirtual - so a class that stands alone
  // (the overwhelming majority of existing code, and the only kind that
  // existed before this) is completely unaffected: same direct-call
  // codegen, same struct layout (no vtable pointer prepended), byte-for-
  // byte identical to before inheritance existed at all.
  //
  // A derived class's own effective field list is its base's own
  // (recursively, for a multi-level chain) followed by its own new
  // fields, in that order - what makes upcasting a Dog* to an Animal*
  // a pure no-op pointer reinterpretation (the base's own fields sit at
  // the exact same struct offsets in both layouts) rather than needing
  // any real conversion. A derived class's own new field can't reuse a
  // name from anywhere in its base chain (Sema rejects it) - no field
  // shadowing.
  std::string baseClassName;
  InterfaceDecl *baseClass = nullptr;
  // True iff this class is actually part of an inheritance family - has
  // a baseClass, or is one for some OTHER class (computed once every
  // class's own baseClass is resolved - see Sema::Check). This is the
  // one flag that decides whether Codegen gives this class's own
  // instances a vtable-pointer field at all, and whether a call to one
  // of its methods goes through that vtable (indirect, real dynamic
  // dispatch) or stays a plain direct call the way every method call
  // already worked before inheritance existed. A class untouched by any
  // `extends` relationship anywhere always has this false, keeping it
  // byte-for-byte identical (same struct layout, same call codegen) to
  // before this feature existed at all.
  bool hasVirtualDispatch = false;
  // `static name: Type = initExpr;` - a class-scoped value, not part of
  // any instance (never appears in a `{}` object literal, and isn't
  // enumerable through one) - a real value that exists exactly once,
  // same lifetime/initialization-order deal a top-level `let`/`const`
  // has (see Program::globals' own doc comment: a literal is a real
  // compile-time constant, anything else computed once via the same
  // generated module-constructor). Requires an initializer (unlike an
  // instance field, which never has one - an instance is always built
  // structurally via `{}`, so there's nowhere for a default value to
  // even go), which is also why this is a real VarDecl Stmt rather than
  // an InterfaceField the way instance fields are - see
  // Parser::ParseClassBody. Accessed as `ClassName.name`, mangled
  // "ClassName$static$name" (see Sema::MangleStatic) - a distinct
  // namespace from instance fields/methods, so a static and instance
  // member can share a name with no collision, matching real TS/JS.
  std::vector<std::unique_ptr<Stmt>> staticFields;
  SourceLoc loc;
  bool isOpaque = false; // `declare type Name;`/`declare class Name { ... }` -
                          // a foreign handle with no accessible fields, never
                          // constructible with `{}` (a declare class's methods
                          // are still real ART code, though - typically thin
                          // wrappers around `declare function`s)
  std::string sourceFile; // see FunctionDecl's own doc comment
  bool isExported = false;
};

// `enum Name { Member, Member2 = 5, Member3 }` - a closed set of named
// numeric constants, auto-numbered from 0 (each bare member is the
// previous member's value + 1, or 0 for the first) unless a member gives
// its own explicit `= NumberLiteral` (which the auto-numbering then
// continues from - `Member3` above is 6, not 2). `value` is filled in by
// Sema (EnumDecl itself only records what the source literally wrote:
// `hasExplicitValue`/`explicitValue`); accessed as `Name.Member`, exactly
// the same static-member access syntax a class's own `static` field
// already has (see Sema::MangleEnumMember and its own reuse of the
// static-access rewrite machinery) - an enum member IS, under the hood,
// nothing more than a `static readonly Member: Name = value;` on a class
// with no instance side at all.
//
// Deliberately NOT interchangeable with a plain `number`, even though
// its runtime representation is identical (a real `double` - see
// TypeTag::Enum) - `let c: Color = 5;` is a type error, matching real
// TS's own numeric enums. Unlike real TS, though, arithmetic (`+`, `<`,
// ...) isn't allowed directly on an enum-typed value either (only `==`/
// `!=`, the same "same tag" rule Struct/Handler already have) - ART's
// "no implicit anything" philosophy (no implicit stringification, no
// implicit widening) extends naturally to "no implicit
// enum-participates-in-arithmetic-as-a-number" too, rather than
// special-casing an exception for enums alone. Write
// `numberToString`/an explicit cast-shaped helper function if arithmetic
// on an enum's underlying value is genuinely needed.
struct EnumMember {
  std::string name;
  SourceLoc loc;
  bool hasExplicitValue = false;
  double explicitValue = 0; // only meaningful if hasExplicitValue
  double value = 0;         // filled in by Sema - the member's real, final value
};

struct EnumDecl {
  std::string name;
  std::vector<EnumMember> members;
  SourceLoc loc;
  std::string sourceFile;
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
  std::vector<std::unique_ptr<EnumDecl>> enums;
  std::vector<std::unique_ptr<FunctionDecl>> functions;
  std::vector<std::unique_ptr<FunctionDecl>> externFunctions; // `declare function ...;` - body is always null
  // Top-level `let`/`const` (each a StmtKind::VarDecl) that's actually a
  // real persistent global - handler state that outlives any single
  // setupApp/function call. Any type, any initializer expression (see
  // Sema::CheckGlobalDecl) - a bare number/boolean/string literal
  // becomes a real compile-time constant; anything else (a call, an
  // object/array literal, ...) is computed once, in declaration order,
  // by a generated module constructor (see Codegen::GenGlobalInit) that
  // runs before main/setupApp, the same "real code, run once, before
  // anything else" mechanism the garbage collector's own initialization
  // already uses. NOT every top-level `let`/`const` ends up here, see
  // topLevelStmts below.
  std::vector<std::unique_ptr<Stmt>> globals;
  // Bare top-level statements (`if`/`while`/`for`/block/expression, plus
  // one exception: a `let`/`const` whose initializer touches an ambient
  // global like `document` - see kAmbientGlobals and
  // Parser::ParseProgram - lands here too, instead of `globals` above,
  // since it can't be a real persistent global (document doesn't exist
  // yet at process start); this is exactly what wrapping it in a bare
  // `{ }` block already meant, just without requiring the user to write
  // that themselves) - the procedural alternative to writing an
  // explicit `function setupApp(): void { ... }`: collected, across
  // every file in the merged program (dependency-first order, same as
  // `globals`), into a generated function literally named "setupApp"
  // (see Codegen::GenSetupAppBody) - main.cpp's existing trampoline
  // already calls exactly that symbol once per page load, so nothing
  // about the C++ integration needs to change. Mutually exclusive with
  // an explicit `function setupApp()` - see Sema::Check. Unlike
  // `globals` (initialized once, at process start, via a module
  // constructor), these run every time the generated `setupApp` itself
  // is called - the two lists exist specifically because those are
  // different lifetimes, not interchangeable ways to write the same
  // thing.
  std::vector<std::unique_ptr<Stmt>> topLevelStmts;
};

} // namespace ART

#endif
