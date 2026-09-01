#include "tokenizer.h"

#include <cctype>
#include <unordered_map>

namespace ART {

const char *TokenKindName(TokenKind kind) {
  switch (kind) {
  case TokenKind::Eof: return "eof";
  case TokenKind::Error: return "error";
  case TokenKind::Identifier: return "identifier";
  case TokenKind::NumberLiteral: return "number literal";
  case TokenKind::StringLiteral: return "string literal";
  case TokenKind::KwFunction: return "'function'";
  case TokenKind::KwLet: return "'let'";
  case TokenKind::KwConst: return "'const'";
  case TokenKind::KwIf: return "'if'";
  case TokenKind::KwElse: return "'else'";
  case TokenKind::KwWhile: return "'while'";
  case TokenKind::KwFor: return "'for'";
  case TokenKind::KwReturn: return "'return'";
  case TokenKind::KwTrue: return "'true'";
  case TokenKind::KwFalse: return "'false'";
  case TokenKind::KwInterface: return "'interface'";
  case TokenKind::KwNumber: return "'number'";
  case TokenKind::KwBoolean: return "'boolean'";
  case TokenKind::KwString: return "'string'";
  case TokenKind::KwVoid: return "'void'";
  case TokenKind::KwDeclare: return "'declare'";
  case TokenKind::KwType: return "'type'";
  case TokenKind::LParen: return "'('";
  case TokenKind::RParen: return "')'";
  case TokenKind::LBrace: return "'{'";
  case TokenKind::RBrace: return "'}'";
  case TokenKind::LBracket: return "'['";
  case TokenKind::RBracket: return "']'";
  case TokenKind::Comma: return "','";
  case TokenKind::Semicolon: return "';'";
  case TokenKind::Colon: return "':'";
  case TokenKind::ColonColon: return "'::'";
  case TokenKind::Dot: return "'.'";
  case TokenKind::Plus: return "'+'";
  case TokenKind::Minus: return "'-'";
  case TokenKind::Star: return "'*'";
  case TokenKind::Slash: return "'/'";
  case TokenKind::Percent: return "'%'";
  case TokenKind::Assign: return "'='";
  case TokenKind::EqEq: return "'=='";
  case TokenKind::NotEq: return "'!='";
  case TokenKind::Lt: return "'<'";
  case TokenKind::LtEq: return "'<='";
  case TokenKind::Gt: return "'>'";
  case TokenKind::GtEq: return "'>='";
  case TokenKind::AndAnd: return "'&&'";
  case TokenKind::OrOr: return "'||'";
  case TokenKind::Bang: return "'!'";
  case TokenKind::PlusPlus: return "'++'";
  case TokenKind::MinusMinus: return "'--'";
  case TokenKind::FatArrow: return "'=>'";
  }
  return "?";
}

namespace {
const std::unordered_map<std::string, TokenKind> kKeywords = {
    {"function", TokenKind::KwFunction},
    {"let", TokenKind::KwLet},
    {"const", TokenKind::KwConst},
    {"if", TokenKind::KwIf},
    {"else", TokenKind::KwElse},
    {"while", TokenKind::KwWhile},
    {"for", TokenKind::KwFor},
    {"return", TokenKind::KwReturn},
    {"true", TokenKind::KwTrue},
    {"false", TokenKind::KwFalse},
    {"interface", TokenKind::KwInterface},
    {"number", TokenKind::KwNumber},
    {"boolean", TokenKind::KwBoolean},
    {"string", TokenKind::KwString},
    {"void", TokenKind::KwVoid},
    {"declare", TokenKind::KwDeclare},
    {"type", TokenKind::KwType},
};
}

Tokenizer::Tokenizer(std::string source) : source(std::move(source)) {}

bool Tokenizer::AtEnd() const { return position >= source.size(); }

char Tokenizer::Current() const {
  return AtEnd() ? '\0' : source[position];
}

char Tokenizer::Peek(size_t offset) const {
  size_t index = position + offset;
  return index >= source.size() ? '\0' : source[index];
}

char Tokenizer::Advance() {
  char c = source[position++];
  if (c == '\n') {
    line++;
    col = 1;
  } else {
    col++;
  }
  return c;
}

void Tokenizer::SkipWhitespaceAndComments() {
  for (;;) {
    char c = Current();
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
      Advance();
    } else if (c == '/' && Peek() == '/') {
      while (!AtEnd() && Current() != '\n') Advance();
    } else if (c == '/' && Peek() == '*') {
      Advance();
      Advance();
      while (!AtEnd() && !(Current() == '*' && Peek() == '/')) Advance();
      if (AtEnd()) return; // unterminated - let the caller hit Eof
      Advance();
      Advance();
    } else {
      return;
    }
  }
}

Token Tokenizer::MakeToken(TokenKind kind, std::string text, SourceLoc loc) const {
  Token t;
  t.kind = kind;
  t.text = std::move(text);
  t.loc = loc;
  return t;
}

Token Tokenizer::MakeError(const std::string &message, SourceLoc loc) const {
  return MakeToken(TokenKind::Error, message, loc);
}

Token Tokenizer::LexNumber(SourceLoc loc) {
  size_t start = position;
  while (std::isdigit(static_cast<unsigned char>(Current()))) Advance();
  if (Current() == '.' && std::isdigit(static_cast<unsigned char>(Peek()))) {
    Advance();
    while (std::isdigit(static_cast<unsigned char>(Current()))) Advance();
  }
  std::string text = source.substr(start, position - start);
  Token t = MakeToken(TokenKind::NumberLiteral, text, loc);
  t.numberValue = std::stod(text);
  return t;
}

Token Tokenizer::LexString(SourceLoc loc) {
  Advance(); // consume opening '"'
  std::string value;
  while (Current() != '"') {
    if (AtEnd() || Current() == '\n') {
      return MakeError("unterminated string literal", loc);
    }
    char c = Advance();
    if (c != '\\') {
      value.push_back(c);
      continue;
    }
    if (AtEnd()) return MakeError("unterminated string literal", loc);
    char esc = Advance();
    switch (esc) {
    case 'n': value.push_back('\n'); break;
    case 't': value.push_back('\t'); break;
    case 'r': value.push_back('\r'); break;
    case '0': value.push_back('\0'); break;
    case '\\': value.push_back('\\'); break;
    case '"': value.push_back('"'); break;
    case '\'': value.push_back('\''); break;
    default:
      return MakeError(std::string("unknown escape sequence '\\") + esc + "'", loc);
    }
  }
  Advance(); // consume closing '"'
  return MakeToken(TokenKind::StringLiteral, value, loc);
}

Token Tokenizer::LexIdentifierOrKeyword(SourceLoc loc) {
  size_t start = position;
  while (std::isalnum(static_cast<unsigned char>(Current())) || Current() == '_') Advance();
  std::string text = source.substr(start, position - start);
  auto it = kKeywords.find(text);
  TokenKind kind = it != kKeywords.end() ? it->second : TokenKind::Identifier;
  return MakeToken(kind, text, loc);
}

Token Tokenizer::Next() {
  SkipWhitespaceAndComments();
  SourceLoc loc{line, col};

  if (AtEnd()) return MakeToken(TokenKind::Eof, "", loc);

  char c = Current();

  if (std::isdigit(static_cast<unsigned char>(c))) return LexNumber(loc);
  if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') return LexIdentifierOrKeyword(loc);
  if (c == '"') return LexString(loc);

  Advance();
  switch (c) {
  case '(': return MakeToken(TokenKind::LParen, "(", loc);
  case ')': return MakeToken(TokenKind::RParen, ")", loc);
  case '{': return MakeToken(TokenKind::LBrace, "{", loc);
  case '}': return MakeToken(TokenKind::RBrace, "}", loc);
  case '[': return MakeToken(TokenKind::LBracket, "[", loc);
  case ']': return MakeToken(TokenKind::RBracket, "]", loc);
  case ',': return MakeToken(TokenKind::Comma, ",", loc);
  case ';': return MakeToken(TokenKind::Semicolon, ";", loc);
  case ':':
    if (Current() == ':') { Advance(); return MakeToken(TokenKind::ColonColon, "::", loc); }
    return MakeToken(TokenKind::Colon, ":", loc);
  case '.': return MakeToken(TokenKind::Dot, ".", loc);
  case '+':
    if (Current() == '+') { Advance(); return MakeToken(TokenKind::PlusPlus, "++", loc); }
    return MakeToken(TokenKind::Plus, "+", loc);
  case '-':
    if (Current() == '-') { Advance(); return MakeToken(TokenKind::MinusMinus, "--", loc); }
    return MakeToken(TokenKind::Minus, "-", loc);
  case '*': return MakeToken(TokenKind::Star, "*", loc);
  case '/': return MakeToken(TokenKind::Slash, "/", loc);
  case '%': return MakeToken(TokenKind::Percent, "%", loc);
  case '=':
    if (Current() == '=') { Advance(); return MakeToken(TokenKind::EqEq, "==", loc); }
    if (Current() == '>') { Advance(); return MakeToken(TokenKind::FatArrow, "=>", loc); }
    return MakeToken(TokenKind::Assign, "=", loc);
  case '!':
    if (Current() == '=') { Advance(); return MakeToken(TokenKind::NotEq, "!=", loc); }
    return MakeToken(TokenKind::Bang, "!", loc);
  case '<':
    if (Current() == '=') { Advance(); return MakeToken(TokenKind::LtEq, "<=", loc); }
    return MakeToken(TokenKind::Lt, "<", loc);
  case '>':
    if (Current() == '=') { Advance(); return MakeToken(TokenKind::GtEq, ">=", loc); }
    return MakeToken(TokenKind::Gt, ">", loc);
  case '&':
    if (Current() == '&') { Advance(); return MakeToken(TokenKind::AndAnd, "&&", loc); }
    return MakeError("unexpected character '&' (did you mean '&&'?)", loc);
  case '|':
    if (Current() == '|') { Advance(); return MakeToken(TokenKind::OrOr, "||", loc); }
    return MakeError("unexpected character '|' (did you mean '||'?)", loc);
  default:
    return MakeError(std::string("unexpected character '") + c + "'", loc);
  }
}

std::vector<Token> Tokenizer::Tokenize() {
  std::vector<Token> tokens;
  for (;;) {
    Token t = Next();
    bool stop = t.kind == TokenKind::Eof || t.kind == TokenKind::Error;
    tokens.push_back(t);
    if (stop) break;
  }
  return tokens;
}

} // namespace ART
