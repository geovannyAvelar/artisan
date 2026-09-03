#include "parser.h"

#include <sstream>

namespace ART {

Parser::Parser(std::vector<Token> tokens, bool jsxEnabled)
    : tokens(std::move(tokens)), jsxEnabled(jsxEnabled) {}

const Token &Parser::Cur() const { return tokens[pos]; }

const Token &Parser::PeekAt(size_t offset) const {
  size_t index = pos + offset;
  return index < tokens.size() ? tokens[index] : tokens.back();
}

bool Parser::Check(TokenKind kind) const { return Cur().kind == kind; }

bool Parser::Match(TokenKind kind) {
  if (Check(kind)) {
    if (pos + 1 < tokens.size()) pos++;
    return true;
  }
  return false;
}

const Token &Parser::Expect(TokenKind kind, const std::string &context) {
  if (!Check(kind)) {
    std::ostringstream msg;
    msg << "expected " << TokenKindName(kind) << " " << context << ", found "
        << TokenKindName(Cur().kind)
        << (Cur().text.empty() ? "" : " ('" + Cur().text + "')");
    Fail(msg.str(), Cur().loc);
  }
  const Token &t = Cur();
  if (pos + 1 < tokens.size()) pos++;
  return t;
}

void Parser::Fail(const std::string &message, SourceLoc loc) {
  std::ostringstream oss;
  oss << loc.line << ":" << loc.col << ": " << message;
  throw ParseError(oss.str());
}

namespace {
// A raw, pre-resolution counterpart to Sema::ExprUsesAmbientDocument:
// that one matches the *resolved* form (a Call whose resolvedCalleeName
// is "ArtDocument"), which doesn't exist until Sema::CheckExpr runs -
// this one runs at parse time instead, so it has to match the bare
// spelling directly (an Identifier named "document", per
// kAmbientGlobals - see ast.h) rather than what it desugars to. Same
// recursive shape as ExprUsesAmbientDocument (lhs/rhs/operand/elements/
// fields - see ast.h's Expr field comments for what each covers,
// including call args and JSX children/attributes), just checked
// earlier and less precisely: with no symbol table yet, this can't tell
// a real local/global literally named "document" (which would legally
// shadow the ambient sugar - see kAmbientGlobals's own doc comment)
// from the ambient identifier itself. Accepted as a rare false positive
// (an unrelated global happening to be named "document" and used in
// another global's initializer becomes a per-page-load local
// needlessly) rather than worth building a parser-level symbol table
// for - see Parser::ParseProgram's own use of this.
bool ExprMayReferenceAmbientGlobal(const Expr *expr) {
  if (!expr) return false;
  if (expr->kind == ExprKind::Identifier && kAmbientGlobals.count(expr->name)) return true;
  if (ExprMayReferenceAmbientGlobal(expr->lhs.get())) return true;
  if (ExprMayReferenceAmbientGlobal(expr->rhs.get())) return true;
  if (ExprMayReferenceAmbientGlobal(expr->operand.get())) return true;
  for (auto &e : expr->elements)
    if (ExprMayReferenceAmbientGlobal(e.get())) return true;
  for (auto &f : expr->fields)
    if (ExprMayReferenceAmbientGlobal(f.second.get())) return true;
  return false;
}
} // namespace

Program Parser::ParseProgram() {
  Program program;
  try {
    if (Cur().kind == TokenKind::Error) {
      Fail(std::string("invalid token: ") + Cur().text, Cur().loc);
    }

    // Every import must come before any other top-level declaration - a
    // simple, fixed rule (no interleaving) that keeps the "what does
    // this file depend on" question answerable by scanning its start,
    // the same reason most real module systems have a similar rule.
    while (Check(TokenKind::KwImport)) {
      program.imports.push_back(ParseImport());
    }

    while (!Check(TokenKind::Eof)) {
      bool exported = Match(TokenKind::KwExport);

      if (Check(TokenKind::KwInterface)) {
        auto decl = ParseInterface();
        decl->isExported = exported;
        program.interfaces.push_back(std::move(decl));
      } else if (Check(TokenKind::KwClass)) {
        auto decl = ParseClass(/*isOpaque=*/false);
        decl->isExported = exported;
        program.interfaces.push_back(std::move(decl));
      } else if (Check(TokenKind::KwFunction)) {
        auto decl = ParseFunction();
        decl->isExported = exported;
        program.functions.push_back(std::move(decl));
      } else if (Check(TokenKind::KwLet) || Check(TokenKind::KwConst)) {
        auto decl = ParseVarDecl();
        decl->isExported = exported;
        // A non-exported declaration whose initializer touches an
        // ambient global (document - see kAmbientGlobals in ast.h) can
        // never be a real persistent global (Sema::CheckGlobalDecl
        // would reject it - document doesn't exist yet at process
        // start), so route it into topLevelStmts instead, right here,
        // to preserve its exact position relative to sibling top-level
        // statements (globals/topLevelStmts are two independently-
        // ordered lists by the time Sema runs - this split can only
        // happen correctly in this one sequential loop). Exactly as if
        // the user had wrapped it in a bare `{ }` block themselves - no
        // ceremony, same per-page-load-local result either way. An
        // exported one stays in `globals`, where Sema's existing check
        // still (correctly) rejects it: an export promises a real
        // persistent global, which this can't be.
        if (!exported && ExprMayReferenceAmbientGlobal(decl->expr.get())) {
          program.topLevelStmts.push_back(std::move(decl));
        } else {
          program.globals.push_back(std::move(decl));
        }
      } else if (Check(TokenKind::KwDeclare)) {
        pos++; // consume 'declare'
        if (Check(TokenKind::KwType)) {
          auto decl = ParseDeclareType();
          decl->isExported = exported;
          program.interfaces.push_back(std::move(decl));
        } else if (Check(TokenKind::KwFunction)) {
          auto decl = ParseDeclareFunction();
          decl->isExported = exported;
          program.externFunctions.push_back(std::move(decl));
        } else if (Check(TokenKind::KwClass)) {
          auto decl = ParseClass(/*isOpaque=*/true);
          decl->isExported = exported;
          program.interfaces.push_back(std::move(decl));
        } else {
          Fail("expected 'type', 'function', or 'class' after 'declare'", Cur().loc);
        }
      } else if (exported) {
        Fail("expected 'function', 'interface', 'class', 'let', 'const', or 'declare' after 'export'", Cur().loc);
      } else if (Check(TokenKind::KwImport)) {
        Fail("'import' must come before every other declaration in a file", Cur().loc);
      } else {
        // Not a declaration - a top-level *statement* instead (see
        // Program::topLevelStmts' own doc comment): an `if`/`while`/
        // `for`/block/bare expression, run once per page load as part
        // of the generated setupApp - the procedural alternative to
        // writing `function setupApp(): void { ... }` explicitly.
        // `return` parses here too (ParseStmt doesn't distinguish
        // context) but is rejected by Sema - there's nothing to return
        // from at the top level.
        program.topLevelStmts.push_back(ParseStmt());
      }
    }
  } catch (const ParseError &e) {
    diagnostics.push_back(e.what());
  }
  return program;
}

std::unique_ptr<ImportDecl> Parser::ParseImport() {
  auto decl = std::make_unique<ImportDecl>();
  decl->loc = Cur().loc;
  Expect(TokenKind::KwImport, "to start an import");
  Expect(TokenKind::LBrace, "to start an import's name list ('import { a, b } from \"...\";')");
  if (!Check(TokenKind::RBrace)) {
    do {
      ImportedName n;
      n.loc = Cur().loc;
      n.name = Expect(TokenKind::Identifier, "as an imported name").text;
      decl->names.push_back(std::move(n));
    } while (Match(TokenKind::Comma));
  }
  Expect(TokenKind::RBrace, "to close an import's name list");
  Expect(TokenKind::KwFrom, "after an import's name list");
  decl->path = Expect(TokenKind::StringLiteral, "as the import path").text;
  Expect(TokenKind::Semicolon, "after an import statement");
  return decl;
}

// ---------------------------------------------------------------------
// Declarations
// ---------------------------------------------------------------------

std::unique_ptr<InterfaceDecl> Parser::ParseInterface() {
  auto decl = std::make_unique<InterfaceDecl>();
  decl->loc = Cur().loc;
  Expect(TokenKind::KwInterface, "to start an interface declaration");
  decl->name = Expect(TokenKind::Identifier, "as the interface name").text;
  ParseOptionalTypeParams(decl->typeParams);
  Expect(TokenKind::LBrace, "to open the interface body");
  while (!Check(TokenKind::RBrace)) {
    InterfaceField field;
    field.loc = Cur().loc;
    field.name = Expect(TokenKind::Identifier, "as a field name").text;
    Expect(TokenKind::Colon, "after field name");
    field.type = ParseType();
    if (Check(TokenKind::Semicolon) || Check(TokenKind::Comma)) pos++;
    decl->fields.push_back(std::move(field));
  }
  Expect(TokenKind::RBrace, "to close the interface body");
  return decl;
}

// The implicit receiver - every method (plain or accessor) gets exactly
// this as its real first parameter, invisible in source (`obj.method
// (args)`/`obj.prop`/`obj.prop = v` never supply it explicitly - see
// Sema::CheckExpr's Call/Member/Assign handling, which splices the
// receiver expression in as the actual first argument). Always the
// class's own type: ART methods have no virtual dispatch to otherwise
// justify a different receiver type. For a generic class (`decl-
// >typeParams` non-empty), `this` is `ClassName<T1, T2, ...>` - its own
// type parameters, referenced bare, exactly like a generic interface's
// own field types already do (see ResolveType's Named case) - so `this`
// resolves back to whichever concrete instantiation is actually being
// checked (see Sema::InstantiateInterface), not the template itself.
void Parser::InjectImplicitThis(FunctionDecl *method, const InterfaceDecl *decl) {
  Param self;
  self.name = "this";
  self.loc = method->loc;
  auto selfType = std::make_unique<TypeNode>();
  selfType->kind = TypeSyntaxKind::Named;
  selfType->loc = method->loc;
  selfType->name = decl->name;
  for (const std::string &typeParam : decl->typeParams) {
    auto argType = std::make_unique<TypeNode>();
    argType->kind = TypeSyntaxKind::Named;
    argType->loc = method->loc;
    argType->name = typeParam;
    selfType->genericArgs.push_back(std::move(argType));
  }
  self.type = std::move(selfType);
  method->params.insert(method->params.begin(), std::move(self));
}

// Parses `get name(): T { body }` or `set name(value: T): void { body }` -
// see FunctionDecl::isGetter/isSetter's own doc comment for how these
// differ from a plain method at every later stage. `get`/`set` are
// contextual, not reserved words - ParseClassBody only recognizes them
// by lookahead ("identifier 'get'/'set', identifier, '('"), so they stay
// ordinary, usable identifiers everywhere else in the language (unlike
// `type`, which had to become fully reserved for `declare type` - see
// README.md's "Classes" section for that tradeoff, still relevant here:
// a property can't be named `type` either, for the same reason).
std::unique_ptr<FunctionDecl> Parser::ParseAccessor(InterfaceDecl *decl, bool isGetter) {
  auto method = std::make_unique<FunctionDecl>();
  method->loc = Cur().loc;
  pos++; // consume the contextual 'get'/'set' identifier
  method->name = Expect(TokenKind::Identifier, "as the property name").text;
  Expect(TokenKind::LParen, "to start the parameter list");
  if (isGetter) {
    Expect(TokenKind::RParen, "to close a getter's parameter list - it takes none");
    Expect(TokenKind::Colon, "after a getter's parameter list - its return type is required");
    method->returnType = ParseType();
  } else {
    Param value;
    value.loc = Cur().loc;
    value.name = Expect(TokenKind::Identifier, "as the setter's value parameter name").text;
    Expect(TokenKind::Colon, "after the setter's parameter name");
    value.type = ParseType();
    method->params.push_back(std::move(value));
    Expect(TokenKind::RParen, "to close the setter's parameter list - it takes exactly one");
    // A setter is always void - same as real TS, where a setter's own
    // return type (if written at all) must be 'void'/'any'; ART just
    // never lets you write one, there's nothing to say.
    auto voidType = std::make_unique<TypeNode>();
    voidType->kind = TypeSyntaxKind::Void;
    voidType->loc = Cur().loc;
    method->returnType = std::move(voidType);
  }
  method->body = ParseBlock();
  method->isGetter = isGetter;
  method->isSetter = !isGetter;
  InjectImplicitThis(method.get(), decl);
  return method;
}

// Shared by `class` and `declare class` - see InterfaceDecl::methods' own
// doc comment for why both reuse the same InterfaceDecl node. A `declare
// class` (isOpaque) may only contain methods/accessors, same restriction
// `declare type` already has on fields (it's a foreign handle - there's
// nothing here to lay out a struct for); a plain `class` may freely
// interleave field, method, and accessor declarations.
void Parser::ParseClassBody(InterfaceDecl *decl, bool isOpaque) {
  Expect(TokenKind::LBrace, "to open the class body");
  while (!Check(TokenKind::RBrace)) {
    bool isGetter = Check(TokenKind::Identifier) && Cur().text == "get" &&
                     PeekAt(1).kind == TokenKind::Identifier && PeekAt(2).kind == TokenKind::LParen;
    bool isSetter = !isGetter && Check(TokenKind::Identifier) && Cur().text == "set" &&
                     PeekAt(1).kind == TokenKind::Identifier && PeekAt(2).kind == TokenKind::LParen;
    if (isGetter || isSetter) {
      decl->methods.push_back(ParseAccessor(decl, isGetter));
    } else if (Check(TokenKind::KwFunction)) {
      auto method = ParseFunction();
      if (!method->typeParams.empty()) {
        Fail("a class method can't be generic yet", method->loc);
      }
      InjectImplicitThis(method.get(), decl);
      decl->methods.push_back(std::move(method));
    } else if (isOpaque) {
      Fail("'declare class' can only contain methods/accessors - it has no accessible fields, same as "
           "'declare type'",
           Cur().loc);
    } else {
      InterfaceField field;
      field.loc = Cur().loc;
      field.name =
          Expect(TokenKind::Identifier, "as a field name, or 'function'/'get'/'set' to start a method").text;
      Expect(TokenKind::Colon, "after field name");
      field.type = ParseType();
      if (Check(TokenKind::Semicolon) || Check(TokenKind::Comma)) pos++;
      decl->fields.push_back(std::move(field));
    }
  }
  Expect(TokenKind::RBrace, "to close the class body");
}

std::unique_ptr<InterfaceDecl> Parser::ParseClass(bool isOpaque) {
  auto decl = std::make_unique<InterfaceDecl>();
  decl->loc = Cur().loc;
  Expect(TokenKind::KwClass, isOpaque ? "after 'declare'" : "to start a class declaration");
  decl->isOpaque = isOpaque;
  decl->name = Expect(TokenKind::Identifier, "as the class name").text;
  ParseOptionalTypeParams(decl->typeParams);
  ParseClassBody(decl.get(), isOpaque);
  return decl;
}

// Consumes "<T, U, ...>" if present - a generic function/declare
// function/interface/declare type's own type parameter list. `<`/`>` are
// unambiguous here (unlike at a call site - see ParsePostfix's turbofish
// handling), since a declaration's type parameter list can never start
// where a comparison expression could.
void Parser::ParseOptionalTypeParams(std::vector<std::string> &outTypeParams) {
  if (!Match(TokenKind::Lt)) return;
  if (!Check(TokenKind::Gt)) {
    do {
      outTypeParams.push_back(Expect(TokenKind::Identifier, "as a type parameter name").text);
    } while (Match(TokenKind::Comma));
  }
  Expect(TokenKind::Gt, "to close a type parameter list");
}

// Consumes "( params... ) (: returnType)?" - shared by a full `function`
// declaration and a body-less `declare function` one.
void Parser::ParseParamsAndReturnType(FunctionDecl *decl) {
  Expect(TokenKind::LParen, "to start the parameter list");
  if (!Check(TokenKind::RParen)) {
    do {
      Param p;
      p.loc = Cur().loc;
      p.name = Expect(TokenKind::Identifier, "as a parameter name").text;
      Expect(TokenKind::Colon, "after parameter name");
      p.type = ParseType();
      decl->params.push_back(std::move(p));
    } while (Match(TokenKind::Comma));
  }
  Expect(TokenKind::RParen, "to close the parameter list");

  if (Match(TokenKind::Colon)) {
    decl->returnType = ParseType();
  } else {
    auto voidType = std::make_unique<TypeNode>();
    voidType->kind = TypeSyntaxKind::Void;
    voidType->loc = Cur().loc;
    decl->returnType = std::move(voidType);
  }
}

std::unique_ptr<FunctionDecl> Parser::ParseFunction() {
  auto decl = std::make_unique<FunctionDecl>();
  decl->loc = Cur().loc;
  Expect(TokenKind::KwFunction, "to start a function declaration");
  decl->name = Expect(TokenKind::Identifier, "as the function name").text;
  ParseOptionalTypeParams(decl->typeParams);
  ParseParamsAndReturnType(decl.get());
  decl->body = ParseBlock();
  return decl;
}

// Anonymous `function(params): returnType { body }` used as a Handler
// value - ART's only function-EXPRESSION form (contrast ParseFunction,
// which parses a top-level *declaration*; that path never reaches here,
// so there's no real grammar ambiguity to resolve). A *named* function
// where an expression is expected is deliberately rejected with a clear
// diagnostic instead of silently being treated as some other construct -
// it isn't a closure literal (those are always anonymous) and nested
// named function declarations aren't supported.
std::unique_ptr<Expr> Parser::ParseFunctionExpr() {
  SourceLoc loc = Cur().loc;
  Expect(TokenKind::KwFunction, "to start a function expression");
  if (Check(TokenKind::Identifier)) {
    Fail("a function expression is always anonymous - nested named function declarations aren't "
         "supported ('function " + Cur().text + "(...) {...}' isn't valid here; write 'function(...) {...}' "
         "instead)",
         Cur().loc);
  }
  auto e = MakeExpr(ExprKind::FunctionExpr, loc);
  e->fn = std::make_unique<FunctionDecl>();
  e->fn->loc = loc;
  ParseParamsAndReturnType(e->fn.get());
  e->fn->body = ParseBlock();
  return e;
}

std::unique_ptr<InterfaceDecl> Parser::ParseDeclareType() {
  auto decl = std::make_unique<InterfaceDecl>();
  decl->loc = Cur().loc;
  decl->isOpaque = true;
  Expect(TokenKind::KwType, "after 'declare'");
  decl->name = Expect(TokenKind::Identifier, "as the type name").text;
  ParseOptionalTypeParams(decl->typeParams);
  Expect(TokenKind::Semicolon, "after 'declare type ...'");
  return decl;
}

std::unique_ptr<FunctionDecl> Parser::ParseDeclareFunction() {
  auto decl = std::make_unique<FunctionDecl>();
  decl->loc = Cur().loc;
  Expect(TokenKind::KwFunction, "after 'declare'");
  decl->name = Expect(TokenKind::Identifier, "as the function name").text;
  decl->isExtern = true; // see FunctionDecl::isExtern's own doc comment
  ParseOptionalTypeParams(decl->typeParams);
  ParseParamsAndReturnType(decl.get());
  Expect(TokenKind::Semicolon, "after a 'declare function' signature (it has no body)");
  return decl; // decl->body stays null - bound to an external symbol at link time
}

std::unique_ptr<TypeNode> Parser::ParseType() {
  auto node = std::make_unique<TypeNode>();
  node->loc = Cur().loc;

  if (Match(TokenKind::KwNumber)) {
    node->kind = TypeSyntaxKind::Number;
  } else if (Match(TokenKind::KwBoolean)) {
    node->kind = TypeSyntaxKind::Boolean;
  } else if (Match(TokenKind::KwString)) {
    node->kind = TypeSyntaxKind::String;
  } else if (Match(TokenKind::KwVoid)) {
    node->kind = TypeSyntaxKind::Void;
  } else if (Check(TokenKind::LParen)) {
    // ART's only function type: a void-returning handler, e.g. `() =>
    // void` or `(event: Event) => void`. A parameter name is required for
    // readability (matching a real function's own parameter list) but is
    // purely decorative - only the types, in order, are ever checked
    // against an actual function's signature (see Sema::CheckExpr's
    // Identifier case). No return type other than 'void' parses.
    pos++;
    if (!Check(TokenKind::RParen)) {
      do {
        Expect(TokenKind::Identifier, "as a handler parameter name");
        Expect(TokenKind::Colon, "after handler parameter name");
        node->handlerParamTypes.push_back(ParseType());
      } while (Match(TokenKind::Comma));
    }
    Expect(TokenKind::RParen, "to close a handler type's parameter list");
    Expect(TokenKind::FatArrow, "after ')' in a handler type - expected '=> void'");
    Expect(TokenKind::KwVoid, "after '=>' - ART handler types must return 'void'");
    node->kind = TypeSyntaxKind::Handler;
  } else if (Check(TokenKind::Identifier)) {
    node->kind = TypeSyntaxKind::Named;
    node->name = Cur().text;
    pos++;
    // A generic interface/opaque-type instantiation, e.g. `Box<number>`.
    // Plain '<' (not the '::<' turbofish a generic function CALL needs)
    // is unambiguous here: unlike an expression, a type can never start
    // with something a '<'/'>' comparison could also parse as.
    if (Match(TokenKind::Lt)) {
      do {
        node->genericArgs.push_back(ParseType());
      } while (Match(TokenKind::Comma));
      Expect(TokenKind::Gt, "to close a generic type's argument list");
    }
  } else {
    Fail("expected a type ('number', 'boolean', 'string', 'void', '() => void', or an interface name)", Cur().loc);
  }

  while (Check(TokenKind::LBracket) && PeekAt(1).kind == TokenKind::RBracket) {
    pos += 2; // consume '[' ']'
    auto arr = std::make_unique<TypeNode>();
    arr->kind = TypeSyntaxKind::Array;
    arr->loc = node->loc;
    arr->element = std::move(node);
    node = std::move(arr);
  }

  return node;
}

// ---------------------------------------------------------------------
// Statements
// ---------------------------------------------------------------------

std::unique_ptr<Stmt> Parser::ParseStmt() {
  if (Check(TokenKind::LBrace)) return ParseBlock();
  if (Check(TokenKind::KwLet) || Check(TokenKind::KwConst)) return ParseVarDecl();
  if (Check(TokenKind::KwIf)) return ParseIf();
  if (Check(TokenKind::KwWhile)) return ParseWhile();
  if (Check(TokenKind::KwFor)) return ParseFor();
  if (Check(TokenKind::KwReturn)) return ParseReturn();

  auto stmt = MakeStmt(StmtKind::ExprStmt, Cur().loc);
  stmt->expr = ParseExpr();
  Expect(TokenKind::Semicolon, "after expression statement");
  return stmt;
}

std::unique_ptr<Stmt> Parser::ParseBlock() {
  auto block = MakeStmt(StmtKind::Block, Cur().loc);
  Expect(TokenKind::LBrace, "to start a block");
  while (!Check(TokenKind::RBrace)) {
    block->statements.push_back(ParseStmt());
  }
  Expect(TokenKind::RBrace, "to close a block");
  return block;
}

std::unique_ptr<Stmt> Parser::ParseVarDecl() {
  auto stmt = MakeStmt(StmtKind::VarDecl, Cur().loc);
  stmt->isConst = Check(TokenKind::KwConst);
  pos++; // consume 'let'/'const'
  stmt->varName = Expect(TokenKind::Identifier, "as the variable name").text;
  if (Match(TokenKind::Colon)) {
    stmt->declaredType = ParseType();
  }
  Expect(TokenKind::Assign, "to initialize the variable (declarations must have an initializer)");
  stmt->expr = ParseExpr();
  Expect(TokenKind::Semicolon, "after variable declaration");
  return stmt;
}

std::unique_ptr<Stmt> Parser::ParseIf() {
  auto stmt = MakeStmt(StmtKind::If, Cur().loc);
  pos++; // consume 'if'
  Expect(TokenKind::LParen, "after 'if'");
  stmt->cond = ParseExpr();
  Expect(TokenKind::RParen, "after if condition");
  stmt->body = ParseStmt();
  if (Match(TokenKind::KwElse)) {
    stmt->elseBranch = ParseStmt();
  }
  return stmt;
}

std::unique_ptr<Stmt> Parser::ParseWhile() {
  auto stmt = MakeStmt(StmtKind::While, Cur().loc);
  pos++; // consume 'while'
  Expect(TokenKind::LParen, "after 'while'");
  stmt->cond = ParseExpr();
  Expect(TokenKind::RParen, "after while condition");
  stmt->body = ParseStmt();
  return stmt;
}

std::unique_ptr<Stmt> Parser::ParseFor() {
  SourceLoc loc = Cur().loc;
  pos++; // consume 'for'
  Expect(TokenKind::LParen, "after 'for'");

  // `for (let x of ...)` - "of" is a contextual keyword (a plain identifier
  // everywhere else), so this is resolved by lookahead rather than a token.
  if ((Check(TokenKind::KwLet) || Check(TokenKind::KwConst)) && PeekAt(1).kind == TokenKind::Identifier &&
      PeekAt(2).kind == TokenKind::Identifier && PeekAt(2).text == "of") {
    return ParseForOf(loc);
  }

  auto stmt = MakeStmt(StmtKind::For, loc);
  if (Check(TokenKind::Semicolon)) {
    pos++; // no initializer
  } else if (Check(TokenKind::KwLet) || Check(TokenKind::KwConst)) {
    stmt->initStmt = ParseVarDecl(); // consumes its own trailing ';'
  } else {
    auto exprStmt = MakeStmt(StmtKind::ExprStmt, Cur().loc);
    exprStmt->expr = ParseExpr();
    Expect(TokenKind::Semicolon, "after for-loop initializer");
    stmt->initStmt = std::move(exprStmt);
  }

  if (!Check(TokenKind::Semicolon)) stmt->cond = ParseExpr();
  Expect(TokenKind::Semicolon, "after for-loop condition");

  if (!Check(TokenKind::RParen)) stmt->update = ParseExpr();
  Expect(TokenKind::RParen, "after for-loop update");

  stmt->body = ParseStmt();
  return stmt;
}

std::unique_ptr<Stmt> Parser::ParseForOf(SourceLoc loc) {
  auto stmt = MakeStmt(StmtKind::ForOf, loc);
  stmt->isConst = Check(TokenKind::KwConst);
  pos++; // consume 'let'/'const'
  stmt->varName = Expect(TokenKind::Identifier, "as the loop variable name").text;
  Expect(TokenKind::Identifier, "'of' after the loop variable in a for-of loop"); // consumes 'of'
  stmt->expr = ParseExpr();
  Expect(TokenKind::RParen, "to close a for-of loop");
  stmt->body = ParseStmt();
  return stmt;
}

std::unique_ptr<Stmt> Parser::ParseReturn() {
  auto stmt = MakeStmt(StmtKind::Return, Cur().loc);
  pos++; // consume 'return'
  if (!Check(TokenKind::Semicolon)) {
    stmt->expr = ParseExpr();
  }
  Expect(TokenKind::Semicolon, "after return statement");
  return stmt;
}

// ---------------------------------------------------------------------
// Expressions (precedence climbing)
// ---------------------------------------------------------------------

std::unique_ptr<Expr> Parser::ParseExpr() { return ParseAssignment(); }

std::unique_ptr<Expr> Parser::ParseAssignment() {
  auto lhs = ParseConditional();
  if (Check(TokenKind::Assign)) {
    SourceLoc loc = Cur().loc;
    pos++;
    auto rhs = ParseAssignment();
    auto e = MakeExpr(ExprKind::Assign, loc);
    e->lhs = std::move(lhs);
    e->rhs = std::move(rhs);
    return e;
  }
  return lhs;
}

// `cond ? then : else` - the condition is a LogicalOr-level expression
// (so `a || b ? c : d` parses as `(a || b) ? c : d`, matching real TS/JS
// precedence - a bare ternary can't be the condition without parens);
// each branch is parsed at ParseAssignment's own level, so both nested
// ternaries and assignments inside a branch work, and `a ? b : c ? d : e`
// is right-associative (`a ? b : (c ? d : e)`), same as real TS/JS.
std::unique_ptr<Expr> Parser::ParseConditional() {
  auto cond = ParseLogicalOr();
  if (Check(TokenKind::Question)) {
    SourceLoc loc = Cur().loc;
    pos++;
    auto thenExpr = ParseAssignment();
    Expect(TokenKind::Colon, "to separate a conditional expression's branches ('cond ? then : else')");
    auto elseExpr = ParseAssignment();
    auto e = MakeExpr(ExprKind::Conditional, loc);
    e->operand = std::move(cond);
    e->lhs = std::move(thenExpr);
    e->rhs = std::move(elseExpr);
    return e;
  }
  return cond;
}

std::unique_ptr<Expr> Parser::ParseLogicalOr() {
  auto lhs = ParseLogicalAnd();
  while (Check(TokenKind::OrOr)) {
    SourceLoc loc = Cur().loc;
    std::string op = Cur().text;
    pos++;
    auto rhs = ParseLogicalAnd();
    auto e = MakeExpr(ExprKind::Binary, loc);
    e->op = op;
    e->lhs = std::move(lhs);
    e->rhs = std::move(rhs);
    lhs = std::move(e);
  }
  return lhs;
}

std::unique_ptr<Expr> Parser::ParseLogicalAnd() {
  auto lhs = ParseEquality();
  while (Check(TokenKind::AndAnd)) {
    SourceLoc loc = Cur().loc;
    std::string op = Cur().text;
    pos++;
    auto rhs = ParseEquality();
    auto e = MakeExpr(ExprKind::Binary, loc);
    e->op = op;
    e->lhs = std::move(lhs);
    e->rhs = std::move(rhs);
    lhs = std::move(e);
  }
  return lhs;
}

std::unique_ptr<Expr> Parser::ParseEquality() {
  auto lhs = ParseRelational();
  while (Check(TokenKind::EqEq) || Check(TokenKind::NotEq)) {
    SourceLoc loc = Cur().loc;
    std::string op = Cur().text;
    pos++;
    auto rhs = ParseRelational();
    auto e = MakeExpr(ExprKind::Binary, loc);
    e->op = op;
    e->lhs = std::move(lhs);
    e->rhs = std::move(rhs);
    lhs = std::move(e);
  }
  return lhs;
}

std::unique_ptr<Expr> Parser::ParseRelational() {
  auto lhs = ParseAdditive();
  while (Check(TokenKind::Lt) || Check(TokenKind::LtEq) || Check(TokenKind::Gt) || Check(TokenKind::GtEq)) {
    SourceLoc loc = Cur().loc;
    std::string op = Cur().text;
    pos++;
    auto rhs = ParseAdditive();
    auto e = MakeExpr(ExprKind::Binary, loc);
    e->op = op;
    e->lhs = std::move(lhs);
    e->rhs = std::move(rhs);
    lhs = std::move(e);
  }
  return lhs;
}

std::unique_ptr<Expr> Parser::ParseAdditive() {
  auto lhs = ParseMultiplicative();
  while (Check(TokenKind::Plus) || Check(TokenKind::Minus)) {
    SourceLoc loc = Cur().loc;
    std::string op = Cur().text;
    pos++;
    auto rhs = ParseMultiplicative();
    auto e = MakeExpr(ExprKind::Binary, loc);
    e->op = op;
    e->lhs = std::move(lhs);
    e->rhs = std::move(rhs);
    lhs = std::move(e);
  }
  return lhs;
}

std::unique_ptr<Expr> Parser::ParseMultiplicative() {
  auto lhs = ParseUnary();
  while (Check(TokenKind::Star) || Check(TokenKind::Slash) || Check(TokenKind::Percent)) {
    SourceLoc loc = Cur().loc;
    std::string op = Cur().text;
    pos++;
    auto rhs = ParseUnary();
    auto e = MakeExpr(ExprKind::Binary, loc);
    e->op = op;
    e->lhs = std::move(lhs);
    e->rhs = std::move(rhs);
    lhs = std::move(e);
  }
  return lhs;
}

std::unique_ptr<Expr> Parser::ParseUnary() {
  if (Check(TokenKind::Bang) || Check(TokenKind::Minus)) {
    SourceLoc loc = Cur().loc;
    std::string op = Cur().text;
    pos++;
    auto e = MakeExpr(ExprKind::Unary, loc);
    e->op = op;
    e->operand = ParseUnary();
    return e;
  }
  if (Check(TokenKind::PlusPlus) || Check(TokenKind::MinusMinus)) {
    // Prefix only - ART has no postfix `x++`/`x--` yet.
    SourceLoc loc = Cur().loc;
    std::string op = Cur().text;
    pos++;
    auto e = MakeExpr(ExprKind::IncDec, loc);
    e->op = op;
    e->operand = ParseUnary();
    return e;
  }
  return ParsePostfix();
}

std::unique_ptr<Expr> Parser::ParsePostfix() {
  auto expr = ParsePrimary();
  for (;;) {
    if (Check(TokenKind::LParen)) {
      SourceLoc loc = Cur().loc;
      pos++;
      auto call = MakeExpr(ExprKind::Call, loc);
      call->lhs = std::move(expr);
      if (!Check(TokenKind::RParen)) {
        do {
          call->elements.push_back(ParseExpr());
        } while (Match(TokenKind::Comma));
      }
      Expect(TokenKind::RParen, "to close call arguments");
      expr = std::move(call);
    } else if (Check(TokenKind::ColonColon)) {
      // A generic call, e.g. `identity::<number>(5)` - '::<' rather than
      // plain '<' specifically to stay unambiguous with the '<'/'>'
      // comparison operators: the parser can't yet know whether an
      // identifier names a generic function (that's Sema's job, a later
      // pass), so a bare '<' after a call target would be a genuine
      // parse-time ambiguity with a comparison expression. No type
      // inference - the argument list is always required here.
      SourceLoc loc = Cur().loc;
      pos++;
      Expect(TokenKind::Lt, "after '::' to start a generic call's type argument list");
      auto call = MakeExpr(ExprKind::Call, loc);
      call->lhs = std::move(expr);
      do {
        call->typeArgs.push_back(ParseType());
      } while (Match(TokenKind::Comma));
      Expect(TokenKind::Gt, "to close a generic call's type argument list");
      Expect(TokenKind::LParen, "to start call arguments after a generic call's type argument list");
      if (!Check(TokenKind::RParen)) {
        do {
          call->elements.push_back(ParseExpr());
        } while (Match(TokenKind::Comma));
      }
      Expect(TokenKind::RParen, "to close call arguments");
      expr = std::move(call);
    } else if (Check(TokenKind::LBracket)) {
      SourceLoc loc = Cur().loc;
      pos++;
      auto index = MakeExpr(ExprKind::Index, loc);
      index->lhs = std::move(expr);
      index->operand = ParseExpr();
      Expect(TokenKind::RBracket, "to close index expression");
      expr = std::move(index);
    } else if (Check(TokenKind::Dot)) {
      SourceLoc loc = Cur().loc;
      pos++;
      std::string field = Expect(TokenKind::Identifier, "as a member name after '.'").text;
      auto member = MakeExpr(ExprKind::Member, loc);
      member->lhs = std::move(expr);
      member->name = field;
      expr = std::move(member);
    } else if (Check(TokenKind::PlusPlus) || Check(TokenKind::MinusMinus)) {
      // Postfix `x++`/`x--` - evaluates to the OLD value (contrast prefix
      // `++x`/`--x` in ParseUnary, which evaluates to the new one). Binds
      // to the fully-built postfix chain so far (`arr[i]++`, `obj.f++`,
      // ...), and - matching real C/TS/JS - is terminal: nothing chains
      // after it (`x++.foo`/`x++()` aren't expressions here either).
      SourceLoc loc = Cur().loc;
      std::string op = Cur().text;
      pos++;
      auto incdec = MakeExpr(ExprKind::IncDec, loc);
      incdec->op = op;
      incdec->isPostfix = true;
      incdec->operand = std::move(expr);
      expr = std::move(incdec);
      break;
    } else {
      break;
    }
  }
  return expr;
}

std::unique_ptr<Expr> Parser::ParsePrimary() {
  SourceLoc loc = Cur().loc;

  if (Check(TokenKind::NumberLiteral)) {
    auto e = MakeExpr(ExprKind::NumberLiteral, loc);
    e->numberValue = Cur().numberValue;
    pos++;
    return e;
  }
  if (Check(TokenKind::StringLiteral)) {
    auto e = MakeExpr(ExprKind::StringLiteral, loc);
    e->name = Cur().text;
    pos++;
    return e;
  }
  if (Check(TokenKind::TemplateStringMiddle) || Check(TokenKind::TemplateStringTail)) return ParseTemplateLiteral();
  if (Match(TokenKind::KwTrue)) {
    auto e = MakeExpr(ExprKind::BoolLiteral, loc);
    e->boolValue = true;
    return e;
  }
  if (Match(TokenKind::KwFalse)) {
    auto e = MakeExpr(ExprKind::BoolLiteral, loc);
    e->boolValue = false;
    return e;
  }
  if (Check(TokenKind::Identifier)) {
    auto e = MakeExpr(ExprKind::Identifier, loc);
    e->name = Cur().text;
    pos++;
    return e;
  }
  if (Match(TokenKind::LParen)) {
    auto e = ParseExpr();
    Expect(TokenKind::RParen, "to close parenthesized expression");
    return e;
  }
  if (Check(TokenKind::LBracket)) return ParseArrayLiteral();
  if (Check(TokenKind::LBrace)) return ParseObjectLiteral();
  if (Check(TokenKind::KwFunction)) return ParseFunctionExpr();
  // A bare '<' can only reach here (the start of a brand-new primary
  // expression) if it isn't a comparison operator - ParseRelational only
  // ever sees '<' *between* two already-parsed operands, never at an
  // expression's own start. So this is unambiguous, the same way `::<T>`
  // being required (not plain `<T>`) at a generic call site is - see
  // ParsePostfix's own comment on that. Gated on jsxEnabled (.tsx files
  // only) so an ordinary .ts file's stray '<' still gets the same "expected
  // an expression" error it always has, not a JSX parse attempt.
  if (jsxEnabled && Check(TokenKind::Lt)) return ParseJsxElement();

  Fail("expected an expression, found " + std::string(TokenKindName(Cur().kind)), loc);
}

// True if the current token can start a JSX tag/attribute name segment -
// an ordinary Identifier, or any keyword token (its raw lexeme is still
// exactly the keyword spelling - Token::text holds that regardless of
// kind). HTML attribute/tag names routinely collide with ART's own
// keywords (`class`, `for`, `type`, ...), so JSX names can't be
// restricted to real Identifier tokens the way everywhere else in the
// grammar is.
bool Parser::CheckJsxName() const {
  return Check(TokenKind::Identifier) ||
         (Cur().kind >= TokenKind::KwFunction && Cur().kind <= TokenKind::KwClass);
}

// A JSX tag or attribute name: one name segment (see CheckJsxName),
// optionally followed by more, each preceded by '-' (`data-index`,
// `aria-label`, a hyphenated custom-element tag like `my-component`) -
// ART's tokenizer has no notion of a hyphenated identifier, so this
// reassembles one from the separate Identifier/Minus/Identifier tokens
// it actually produces.
std::string Parser::ParseJsxName(const char *context) {
  if (!CheckJsxName()) {
    Fail("expected a JSX name " + std::string(context) + ", found " + std::string(TokenKindName(Cur().kind)),
         Cur().loc);
  }
  std::string name = Cur().text;
  pos++;
  while (Check(TokenKind::Minus) &&
         (PeekAt(1).kind == TokenKind::Identifier ||
          (PeekAt(1).kind >= TokenKind::KwFunction && PeekAt(1).kind <= TokenKind::KwClass))) {
    pos++; // '-'
    name += "-";
    name += Cur().text;
    pos++;
  }
  return name;
}

// `<tag attr="literal" attr={expr} ...>child*</tag>` or the self-closing
// `<tag .../>` - see ExprKind::JsxElement's own doc comment for the AST
// shape. Deliberately no bare/unquoted text content between tags (e.g.
// `<div>Hello</div>` doesn't parse) - only nested elements and `{ expr }`
// interpolations - since ART's tokenizer lexes the whole file up front
// into one flat token stream with no notion of a "raw text" mode; text
// content is written `<div>{"Hello"}</div>` instead. Every attribute
// value is required (no bare `<input disabled />`-style shorthand).
//
// Also parses a *fragment*, `<>child*</>` - no tag name, no attributes,
// no self-close - when the token right after '<' is '>' rather than a
// name. `e->name` stays empty (a real tag name is never empty) as the
// AST-level marker Sema/Codegen key off of: see ExprKind::JsxElement's
// own doc comment for why a fragment resolves to `Node[]` rather than
// `Node`.
std::unique_ptr<Expr> Parser::ParseJsxElement() {
  SourceLoc loc = Cur().loc;
  Expect(TokenKind::Lt, "to start a JSX element");

  bool isFragment = Check(TokenKind::Gt);
  auto e = MakeExpr(ExprKind::JsxElement, loc);
  std::string tagName; // stays empty for a fragment

  if (!isFragment) {
    tagName = ParseJsxName("as a JSX tag name");
    e->name = tagName;

    while (CheckJsxName()) {
      std::string attrName = ParseJsxName("as a JSX attribute name");
      Expect(TokenKind::Assign, "after a JSX attribute name ('" + attrName + "')");
      std::unique_ptr<Expr> value;
      if (Check(TokenKind::StringLiteral)) {
        value = MakeExpr(ExprKind::StringLiteral, Cur().loc);
        value->name = Cur().text;
        pos++;
      } else {
        value = ParseJsxBraceExpr("as a JSX attribute value");
      }
      e->fields.emplace_back(attrName, std::move(value));
    }

    if (Match(TokenKind::Slash)) {
      Expect(TokenKind::Gt, "to close a self-closing JSX element ('/>')");
      return e;
    }
  }
  Expect(TokenKind::Gt, isFragment ? "to open a JSX fragment ('<>')"
                                    : "to close a JSX element's opening tag ('<" + tagName + ">')");

  while (!(Check(TokenKind::Lt) && PeekAt(1).kind == TokenKind::Slash)) {
    if (Check(TokenKind::Lt)) {
      e->elements.push_back(ParseJsxElement());
    } else if (Check(TokenKind::LBrace)) {
      e->elements.push_back(ParseJsxBraceExpr("as a JSX child"));
    } else {
      Fail("expected a nested JSX element or '{ ... }', found " + std::string(TokenKindName(Cur().kind)) +
               " inside '<" + (isFragment ? "" : tagName) + ">'",
           Cur().loc);
    }
  }

  SourceLoc closeLoc = Cur().loc;
  Expect(TokenKind::Lt, "to start a JSX closing tag");
  Expect(TokenKind::Slash, "after '<' to start a JSX closing tag");
  if (isFragment) {
    Expect(TokenKind::Gt, "to close a JSX fragment ('</>')");
  } else {
    std::string closeTag = ParseJsxName("as the JSX closing tag name");
    if (closeTag != tagName) {
      Fail("mismatched JSX closing tag - expected '</" + tagName + ">', got '</" + closeTag + ">'", closeLoc);
    }
    Expect(TokenKind::Gt, "to close a JSX closing tag ('</" + tagName + ">')");
  }

  return e;
}

// `{ expr }` - the one place JSX embeds an arbitrary ART expression (an
// attribute value or a child), never going through ParsePrimary's own
// '{' handling (ParseObjectLiteral's `{ field: value, ... }` shape) since
// this is a completely different grammar sharing the same opening brace.
std::unique_ptr<Expr> Parser::ParseJsxBraceExpr(const char *context) {
  Expect(TokenKind::LBrace, context);
  auto value = ParseExpr();
  Expect(TokenKind::RBrace, "to close a JSX '{ ... }' expression");
  return value;
}

// `` `text ${expr} more` `` - see ExprKind::TemplateLiteral's own doc
// comment for the resulting AST shape. The lexer has already split this
// into TemplateStringMiddle/Tail tokens with ordinary tokens for each
// `${...}` interpolation spliced between them (see Tokenizer's own
// templateBraceDepth_ doc comment) - no explicit closing '}' to consume
// per interpolation here, the lexer already ate it and resumed
// template-text scanning in its place, so ParseExpr() alone is enough:
// every token that isn't part of a valid expression continuation
// (Middle/Tail included) makes it stop on its own.
std::unique_ptr<Expr> Parser::ParseTemplateLiteral() {
  SourceLoc loc = Cur().loc;
  auto e = MakeExpr(ExprKind::TemplateLiteral, loc);
  for (;;) {
    bool isTail = Check(TokenKind::TemplateStringTail);
    auto part = MakeExpr(ExprKind::StringLiteral, Cur().loc);
    part->name = Cur().text;
    pos++;
    e->elements.push_back(std::move(part));
    if (isTail) break;
    e->elements.push_back(ParseExpr());
  }
  return e;
}

std::unique_ptr<Expr> Parser::ParseArrayLiteral() {
  SourceLoc loc = Cur().loc;
  Expect(TokenKind::LBracket, "to start an array literal");
  auto e = MakeExpr(ExprKind::ArrayLiteral, loc);
  if (!Check(TokenKind::RBracket)) {
    do {
      e->elements.push_back(ParseExpr());
    } while (Match(TokenKind::Comma));
  }
  Expect(TokenKind::RBracket, "to close an array literal");
  return e;
}

std::unique_ptr<Expr> Parser::ParseObjectLiteral() {
  SourceLoc loc = Cur().loc;
  Expect(TokenKind::LBrace, "to start an object literal");
  auto e = MakeExpr(ExprKind::ObjectLiteral, loc);
  if (!Check(TokenKind::RBrace)) {
    do {
      std::string fieldName = Expect(TokenKind::Identifier, "as an object literal field name").text;
      Expect(TokenKind::Colon, "after object literal field name");
      auto value = ParseExpr();
      e->fields.emplace_back(fieldName, std::move(value));
    } while (Match(TokenKind::Comma));
  }
  Expect(TokenKind::RBrace, "to close an object literal");
  return e;
}

} // namespace ART
