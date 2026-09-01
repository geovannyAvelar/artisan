#ifndef ART_TOKENIZER_H
#define ART_TOKENIZER_H

#include <string>
#include <vector>

namespace ART {

enum class TokenKind {
  Eof,
  Error,

  Identifier,
  NumberLiteral,
  StringLiteral,

  // Keywords
  KwFunction,
  KwLet,
  KwConst,
  KwIf,
  KwElse,
  KwWhile,
  KwFor,
  KwReturn,
  KwTrue,
  KwFalse,
  KwInterface,
  KwNumber,
  KwBoolean,
  KwString,
  KwVoid,
  KwDeclare,
  KwType,

  // Punctuation
  LParen,
  RParen,
  LBrace,
  RBrace,
  LBracket,
  RBracket,
  Comma,
  Semicolon,
  Colon,
  ColonColon,
  Dot,

  // Operators
  Plus,
  Minus,
  Star,
  Slash,
  Percent,
  Assign,
  EqEq,
  NotEq,
  Lt,
  LtEq,
  Gt,
  GtEq,
  AndAnd,
  OrOr,
  Bang,
  PlusPlus,
  MinusMinus,
  FatArrow,
};

struct SourceLoc {
  int line = 1;
  int col = 1;
};

struct Token {
  TokenKind kind = TokenKind::Eof;
  std::string text;      // raw lexeme (identifiers, keywords, error message)
  double numberValue = 0;
  SourceLoc loc;
};

const char *TokenKindName(TokenKind kind);

class Tokenizer {
public:
  explicit Tokenizer(std::string source);

  // Lexes the whole source up front. The returned vector always ends with
  // a single Eof token; a lexical error produces an Error token (also
  // terminal - lexing stops there) instead of throwing.
  std::vector<Token> Tokenize();

private:
  std::string source;
  size_t position = 0;
  int line = 1;
  int col = 1;

  bool AtEnd() const;
  char Current() const;
  char Advance();
  char Peek(size_t offset = 1) const;
  void SkipWhitespaceAndComments();
  Token Next();
  Token MakeToken(TokenKind kind, std::string text, SourceLoc loc) const;
  Token MakeError(const std::string &message, SourceLoc loc) const;
  Token LexNumber(SourceLoc loc);
  Token LexIdentifierOrKeyword(SourceLoc loc);
  Token LexString(SourceLoc loc);
};

} // namespace ART

#endif
