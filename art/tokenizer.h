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
  // A template literal's own literal-text chunks (backtick-quoted,
  // `${expr}` interpolation) - see Tokenizer::LexTemplateStringPart and
  // Parser::ParseTemplateLiteral. Middle: this chunk is followed by
  // `${` (more to come, already consumed - normal tokenizing resumes
  // right after it for the interpolated expression). Tail: this chunk
  // closes the literal (the closing '`' is already consumed) - a
  // template with zero interpolations is just one Tail token, nothing
  // else. There's no separate token for the '}' that closes an
  // interpolation back into template text: the lexer consumes it itself
  // when it's tracked the brace nesting back down to the interpolation's
  // own starting depth (see Tokenizer::templateBraceDepth_) and
  // immediately resumes template-text scanning in its place.
  TemplateStringMiddle,
  TemplateStringTail,

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
  KwImport,
  KwFrom,
  KwExport,
  KwClass,
  KwBreak,
  KwContinue,
  KwDo,
  KwSwitch,
  KwCase,
  KwDefault,
  KwEnum,
  KwStatic,
  KwReadonly,
  KwTry,
  KwCatch,
  KwThrow,
  KwFinally,
  KwExtends,
  KwNull,
  // `any` - see TypeTag::Any's own doc comment for the runtime
  // representation (a tagged box), and `typeof` below for the one way
  // to narrow one back to a concrete type.
  KwAny,
  // `typeof x` - valid only on an `any`-typed operand, yields a string
  // among "number"/"boolean"/"string"/"object"/"function" - see
  // Sema::TryGetTypeofCheckedVar for the `typeof x === "..."` narrowing
  // pattern this exists to support.
  KwTypeof,

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
  Ellipsis, // '...' - a rest parameter (`function f(...args: T[])`)
  Question,

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
  Amp,    // '&' (bitwise AND) - not to be confused with AndAnd ('&&')
  Pipe,   // '|' (bitwise OR) - not to be confused with OrOr ('||')
  Caret,  // '^' (bitwise XOR)
  Tilde,  // '~' (bitwise NOT, unary)
  Shl,    // '<<' (shift left)
  // '>>' (arithmetic shift right) and '>>>' (unsigned/logical shift
  // right) deliberately have NO tokens of their own - they're each just
  // two/three adjacent Gt tokens, merged only when ParseShift's operator
  // loop actually needs one (see its own doc comment for why: merging
  // them here, at the tokenizer level, would silently break every
  // existing `Box<Box<T>>`-shaped nested generic close, which relies on
  // two separate Gt tokens).
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

  // One entry per currently-open `${...}` template interpolation (see
  // TemplateStringMiddle/Tail's own doc comment), tracking how many
  // un-matched '{' have been seen inside *that* interpolation so far -
  // starts at 0 right after '${', incremented by every '{' Next() sees
  // while this is non-empty (still returned as an ordinary LBrace token;
  // only what closes the interpolation itself is special) and
  // decremented by '}'. When a '}' would decrement the top entry below
  // zero, that's instead the interpolation's own closing brace: this
  // entry is popped and Next() resumes template-text scanning right
  // there instead of returning an RBrace token.
  std::vector<int> templateBraceDepth_;

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
  Token LexTemplateStringPart(SourceLoc loc);
};

} // namespace ART

#endif
