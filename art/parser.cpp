#include "parser.h"

#include <sstream>

namespace ART {

Parser::Parser(std::vector<Token> tokens) : tokens(std::move(tokens)) {}

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
      } else if (Check(TokenKind::KwFunction)) {
        auto decl = ParseFunction();
        decl->isExported = exported;
        program.functions.push_back(std::move(decl));
      } else if (Check(TokenKind::KwLet) || Check(TokenKind::KwConst)) {
        auto decl = ParseVarDecl();
        decl->isExported = exported;
        program.globals.push_back(std::move(decl));
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
        } else {
          Fail("expected 'type' or 'function' after 'declare'", Cur().loc);
        }
      } else if (exported) {
        Fail("expected 'function', 'interface', 'let', 'const', or 'declare' after 'export'", Cur().loc);
      } else if (Check(TokenKind::KwImport)) {
        Fail("'import' must come before every other declaration in a file", Cur().loc);
      } else {
        Fail("expected 'function', 'interface', 'let', 'const', 'declare', or 'export' at top level", Cur().loc);
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
  auto lhs = ParseLogicalOr();
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

  Fail("expected an expression, found " + std::string(TokenKindName(Cur().kind)), loc);
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
