#include "lexer.hpp"

#include <ecs/ecs.hpp>
#include <ecs/used_components.hpp>
#include <utils/source_position.hpp>
#include <utils/trie.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <expected>
#include <string>
#include <utility>

#include "tokens.hpp"

namespace lexer {

namespace {

[[nodiscard]] Token CreateToken(TokenType type,
                                const utils::SourcePosition& start,
                                const utils::SourcePosition& stop) {
  auto res = Token(type);
  ecs::Set<ecs::TokenStart>(res, start);
  ecs::Set<ecs::TokenStop>(res, stop);
  return res;
}

}  // namespace

Lexer::Lexer(std::string input) : input_(std::move(input)) {
  BuildKeywords();
  BuildOtherTokens();
}

Token Lexer::NextToken() {
  if (reading_string_) {
    return GetString();
  }

  bool need_skip = true;
  while (need_skip) {
    need_skip = false;
    start_position_ = position_;

    auto skip_comment = TrySkipComment();
    if (!skip_comment.has_value()) {
      return skip_comment.error();
    }

    need_skip |= skip_comment.value();

    need_skip |= SkipWs();
  }

  start_position_ = position_;

  if (AtEof()) {
    return Token(TokenType::kEofToken);
  }

  if (Getchr() == '"') {
    return GetString();
  }

  if (std::isdigit(Getchr()) != 0) {
    return GetNum();
  }

  if (std::isalpha(Getchr()) != 0 || Getchr() == '_') {
    return GetIdOrKeyword();
  }

  if (Getchr() != '.') {
    return GetOther();
  }

  std::size_t next = position_.index + 1;
  if (next >= input_.size() || std::isdigit(input_[next]) == 0) {
    return GetOther();
  }

  return GetNum();
}

void Lexer::BuildKeywords() {
#define KEYWORD(name) \
  keywords_.Insert(std::string(#name), TokenType::kKw##name);

#include <language_data/keywords.dat>

#undef KEYWORD
}

void Lexer::BuildOtherTokens() {
#define EXPAND(name, type, str) \
  other_tokens_.Insert(std::string(str), TokenType::k##type##name);

#define UN_OP(name, str) EXPAND(name, Un, str)
#define BIN_OP(name, str) EXPAND(name, Bin, str)
#define SYNT_TOKEN(name, str) EXPAND(name, Synt, str)

  // clang-format off: order is important: need binary > unary
#include <language_data/un_ops.dat>
#include <language_data/bin_ops.dat>
#include <language_data/synt_tokens.dat>
  // clang-format on

#undef EXPAND

#undef UN_OP
#undef BIN_OP
#undef SYNT_TOKEN
}

std::expected<bool, Token> Lexer::TrySkipComment() {
  std::size_t cur = position_.index;
  std::size_t next = cur + 1;

  if (next >= input_.size()) {
    return false;
  }

  if (input_[cur] != '/') {
    return false;
  }

  if (input_[next] == '/') {
    return SkipCommentOneLiner();
  }

  if (input_[next] == '*') {
    return SkipCommentMultiLiner();
  }

  return false;
}

std::expected<bool, Token> Lexer::SkipCommentOneLiner() {
  NextPosition(2);

  while (!AtEof() && Getchr() != '\n') {
    NextPositionOnce();
  }

  if (!AtEof()) {  // we're at '\n'
    NextPositionOnce();
  }

  return true;
}

std::expected<bool, Token> Lexer::SkipCommentMultiLiner() {
  NextPosition(2);
  size_t comment_depth = 1;

  while (comment_depth > 0 /* && !AtEof() */) {
    std::size_t cur = position_.index;
    std::size_t next = cur + 1;

    if (next >= input_.size()) {
      break;
    }

    if (input_[cur] == '/' && input_[next] == '*') {
      ++comment_depth;
      NextPosition(2);
    } else if (input_[cur] == '*' && input_[next] == '/') {
      --comment_depth;
      NextPosition(2);
    } else {
      NextPositionOnce();
    }
  }

  if (comment_depth == 0) {
    return true;
  }

  auto error = CreateToken(TokenType::kErrorToken, start_position_, position_);
  ecs::Set<ecs::ErrorMessage>(error, std::string("comment not closed"));
  return std::unexpected(error);
}

Token Lexer::GetString() {
  if (!reading_string_) {
    NextPositionOnce();
    reading_string_ = true;
  }

  char chr = '\0';

  while (!AtEof() && (chr = Getchr()) != '"') {
    if (chr != '\\') {
      accumulated_string_.push_back(chr);
      NextPositionOnce();
      continue;
    }

    auto escape = GetEscapeSequence();

    if (escape.has_value()) {
      accumulated_string_.push_back(*escape);
    } else {
      return escape.error();
    }
  }

  reading_string_ = false;

  if (!AtEof()) {  // we're at '"'
    NextPositionOnce();
    auto result =
        CreateToken(TokenType::kStringLiteral, start_position_, position_);
    ecs::Set<ecs::StrValue>(result, std::move(accumulated_string_));
    return result;
  }

  auto error = CreateToken(TokenType::kErrorToken, start_position_, position_);
  ecs::Set<ecs::ErrorMessage>(error, std::string("string not terminated"));
  return error;
}

std::expected<char, Token> Lexer::GetEscapeSequence() {
  NextPositionOnce();

  if (AtEof()) {
    auto error =
        CreateToken(TokenType::kErrorToken, start_position_, position_);
    ecs::Set<ecs::ErrorMessage>(error, std::string("string not terminated"));
    return std::unexpected(error);
  }

  char chr = Getchr();
  NextPositionOnce();

  switch (chr) {
    case 'a':
      return '\a';
    case 'b':
      return '\b';
    case 'e':
      return '\e';
    case 'f':
      return '\f';
    case 'n':
      return '\n';
    case 'r':
      return '\r';
    case 't':
      return '\t';
    case 'v':
      return '\v';
    case '\\':
      return '\\';
    case '"':
      return '"';
    case 'x':
      return GetHexEscapeSequence();

    default: {
      auto error = Token(TokenType::kErrorToken);
      ecs::Set<ecs::TokenStop>(error, position_);
      ecs::Set<ecs::ErrorMessage>(error,
                                  std::string("unknown escape sequence"));
      return std::unexpected(error);
    }
  }
}

std::expected<char, Token> Lexer::GetHexEscapeSequence() {
  std::size_t cur = position_.index;
  std::size_t next = cur + 1;

  NextPosition(2);

  if (next >= input_.size()) {
    auto error =
        CreateToken(TokenType::kErrorToken, start_position_, position_);
    ecs::Set<ecs::ErrorMessage>(error, std::string("string not terminated"));
    return std::unexpected(error);
  }

  std::array<char, 3> raw = {input_[cur], input_[next], '\0'};
  char* end = nullptr;
  auto num = std::strtoul(raw.data(), &end, 16);

  if (end == raw.data() + 2) {
    return static_cast<char>(num);
  }

  auto error = Token(TokenType::kErrorToken);
  ecs::Set<ecs::TokenStop>(error, position_);
  ecs::Set<ecs::ErrorMessage>(error,
                              std::string("unknown hex escape sequence"));
  return std::unexpected(error);
}

/*
 * hate strtoxx functions, thery're evil
 */
Token Lexer::GetNum() {
  const char* start = input_.data() + position_.index;
  char* int_end = nullptr;
  char* float_end = nullptr;

  std::uint64_t int_value = std::strtoull(start, &int_end, 0);
  long double float_value = std::strtold(start, &float_end);

  NextPosition(std::max(int_end - start, float_end - start));

  if (int_end == float_end) {
    auto result =
        CreateToken(TokenType::kIntLiteral, start_position_, position_);
    ecs::Set<ecs::IntValue>(result, int_value);
    return result;
  }

  auto result =
      CreateToken(TokenType::kFloatLiteral, start_position_, position_);
  ecs::Set<ecs::FloatValue>(result, float_value);
  return result;
}

Token Lexer::GetIdOrKeyword() {
  auto chr = Getchr();
  std::string accum_id(1, chr);
  auto* node = keywords_.NextNode(chr);
  NextPositionOnce();

  while (!AtEof() && (std::isalnum(Getchr()) != 0 || Getchr() == '_')) {
    chr = Getchr();
    accum_id += chr;
    NextPositionOnce();

    if (node != nullptr) {
      node = keywords_.NextNode(chr, node);
    }
  }

  if (node != nullptr && node->HasValue()) {
    return CreateToken(node->Value(), start_position_, position_);
  }

  auto result = CreateToken(TokenType::kIdToken, start_position_, position_);
  ecs::Set<ecs::IdName>(result, std::move(accum_id));
  return result;
}

Token Lexer::GetOther() {
  char chr = Getchr();
  auto* node = other_tokens_.NextNode(chr);
  auto* last_node = node;

  NextPositionOnce();
  auto last_position = position_;

  while (!AtEof() && node != nullptr) {
    if (node->HasValue()) {
      last_position = position_;
      last_node = node;
    }

    chr = Getchr();
    NextPositionOnce();
    node = other_tokens_.NextNode(chr, node);
  }

  if (node == nullptr || !node->HasValue()) {
    position_ = last_position;
    node = last_node;
  }

  if (node != nullptr && node->HasValue()) {
    return CreateToken(node->Value(), start_position_, position_);
  }

  auto error = CreateToken(TokenType::kErrorToken, start_position_, position_);
  ecs::Set<ecs::ErrorMessage>(error, std::string("unexpected symbol"));
  return error;
}

void Lexer::NextPositionOnce() {
  if (AtEof()) {
    return;
  }

  if (Getchr() != '\n') {
    ++position_.index;
    ++position_.col;
    return;
  }

  ++position_.index;
  position_.col = 1;
  ++position_.line;
}

void Lexer::NextPosition(std::size_t n) {
  for (; n != 0; --n) {
    NextPositionOnce();
  }
}

bool Lexer::AtEof() { return position_.index == input_.size(); }
char Lexer::Getchr() { return input_[position_.index]; }

bool Lexer::SkipWs() {
  bool res = false;

  while (!AtEof() && std::isspace(Getchr()) != 0) {
    res = true;
    NextPositionOnce();
  }

  return res;
}

}  // namespace lexer
