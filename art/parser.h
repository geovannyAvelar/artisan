#ifndef ART_PARSER_H
#define ART_PARSER_H

#include <stdexcept>
#include <vector>

#include "ast.h"
#include "tokenizer.h"

namespace ART {

// Thrown internally on the first syntax error and caught at the top of
// Parser::ParseProgram - callers just check Diagnostics() after the call.
struct ParseError : std::runtime_error {
  using std::runtime_error::runtime_error;
};

class Parser {
public:
  // `jsxEnabled`: whether `<tag ...>...</tag>` is recognized as a JSX
  // element literal (see ParseJsxElement) rather than always being a
  // parse error - gated per-file on the .tsx extension (see main.cpp/
  // module_resolver.cpp), not on by default, so an ordinary .ts file's
  // `<`/`>` stay exclusively the comparison operators they always were.
  explicit Parser(std::vector<Token> tokens, bool jsxEnabled = false);

  // Parses the whole token stream into a Program. On a syntax error,
  // returns an empty-ish Program and Diagnostics() carries the message -
  // check Diagnostics().empty() before using the result.
  Program ParseProgram();

  const std::vector<std::string> &Diagnostics() const { return diagnostics; }

private:
  std::vector<Token> tokens;
  size_t pos = 0;
  bool jsxEnabled = false;
  std::vector<std::string> diagnostics;

  const Token &Cur() const;
  const Token &PeekAt(size_t offset) const;
  bool Check(TokenKind kind) const;
  bool Match(TokenKind kind);
  const Token &Expect(TokenKind kind, const std::string &context);
  [[noreturn]] void Fail(const std::string &message, SourceLoc loc);

  std::unique_ptr<ImportDecl> ParseImport();
  std::unique_ptr<InterfaceDecl> ParseInterface();
  std::unique_ptr<EnumDecl> ParseEnum();
  std::unique_ptr<FunctionDecl> ParseFunction();
  std::unique_ptr<InterfaceDecl> ParseDeclareType();
  std::unique_ptr<FunctionDecl> ParseDeclareFunction();
  std::unique_ptr<InterfaceDecl> ParseClass(bool isOpaque);
  void ParseClassBody(InterfaceDecl *decl, bool isOpaque);
  std::unique_ptr<FunctionDecl> ParseAccessor(InterfaceDecl *decl, bool isGetter);
  void InjectImplicitThis(FunctionDecl *method, const InterfaceDecl *decl);
  void ParseOptionalTypeParams(std::vector<std::string> &outTypeParams);
  void ParseParamsAndReturnType(FunctionDecl *decl);
  std::unique_ptr<TypeNode> ParseType();

  std::unique_ptr<Stmt> ParseStmt();
  std::unique_ptr<Stmt> ParseBlock();
  std::unique_ptr<Stmt> ParseVarDecl();
  std::unique_ptr<Stmt> ParseDestructureVarDecl();
  std::unique_ptr<Stmt> ParseIf();
  std::unique_ptr<Stmt> ParseWhile();
  std::unique_ptr<Stmt> ParseDoWhile();
  std::unique_ptr<Stmt> ParseFor();
  std::unique_ptr<Stmt> ParseForOf(SourceLoc loc);
  std::unique_ptr<Stmt> ParseReturn();
  std::unique_ptr<Stmt> ParseSwitch();
  std::unique_ptr<Stmt> ParseTry();
  std::unique_ptr<Stmt> ParseThrow();

  std::unique_ptr<Expr> ParseExpr();
  std::unique_ptr<Expr> ParseAssignment();
  std::unique_ptr<Expr> ParseConditional();
  std::unique_ptr<Expr> ParseLogicalOr();
  std::unique_ptr<Expr> ParseLogicalAnd();
  std::unique_ptr<Expr> ParseBitwiseOr();
  std::unique_ptr<Expr> ParseBitwiseXor();
  std::unique_ptr<Expr> ParseBitwiseAnd();
  std::unique_ptr<Expr> ParseEquality();
  std::unique_ptr<Expr> ParseRelational();
  std::unique_ptr<Expr> ParseShift();
  std::unique_ptr<Expr> ParseAdditive();
  std::unique_ptr<Expr> ParseMultiplicative();
  std::unique_ptr<Expr> ParseUnary();
  std::unique_ptr<Expr> ParsePostfix();
  std::unique_ptr<Expr> ParsePrimary();
  std::unique_ptr<Expr> ParseFunctionExpr();
  std::unique_ptr<Expr> ParseArrayLiteral();
  std::unique_ptr<Expr> ParseObjectLiteral();
  std::unique_ptr<Expr> ParseJsxElement();
  std::unique_ptr<Expr> ParseJsxBraceExpr(const char *context);
  std::unique_ptr<Expr> ParseTemplateLiteral();
  bool CheckJsxName() const;
  std::string ParseJsxName(const char *context);
};

} // namespace ART

#endif
