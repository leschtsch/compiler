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

template <typename T>
auto CreateToken(const utils::SourcePosition& start,
                 const utils::SourcePosition& stop) {
  auto res = T{};
  ecs::Set<ecs::TokenStart>(res, start);
  ecs::Set<ecs::TokenStop>(res, stop);
  return res;
}

template <typename T>
auto CreateTokenVariant(const utils::SourcePosition& start,
                        const utils::SourcePosition& stop)
    -> Lexer::TokenVariant {
  return Lexer::TokenVariant(CreateToken<T>(start, stop));
}

}  // namespace

Lexer::Lexer(std::string input) : input_(std::move(input)) {
  BuildKeywords();
  BuildOtherTokens();
}

auto Lexer::NextToken() -> Lexer::TokenVariant {
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
    return tokens::EofToken{};
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

auto Lexer::TrySkipComment() -> std::expected<bool, tokens::ErrorToken> {
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

auto Lexer::SkipCommentOneLiner() -> std::expected<bool, tokens::ErrorToken> {
  NextPosition(2);

  while (!AtEof() && Getchr() != '\n') {
    NextPositionOnce();
  }

  if (!AtEof()) {  // we're at '\n'
    NextPositionOnce();
  }

  return true;
}

auto Lexer::SkipCommentMultiLiner() -> std::expected<bool, tokens::ErrorToken> {
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

  auto error = CreateToken<tokens::ErrorToken>(start_position_, position_);
  ecs::Set<ecs::ErrorMessage>(error, std::string("comment not closed"));
  return std::unexpected(error);
}

auto Lexer::GetString() -> TokenVariant {
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
    auto result = CreateToken<tokens::String>(start_position_, position_);
    ecs::Set<ecs::StrValue>(result, std::move(accumulated_string_));
    return result;
  }

  auto error = CreateToken<tokens::ErrorToken>(start_position_, position_);
  ecs::Set<ecs::ErrorMessage>(error, std::string("string not terminated"));
  return error;
}

auto Lexer::GetEscapeSequence() -> std::expected<char, tokens::ErrorToken> {
  NextPositionOnce();

  if (AtEof()) {
    auto error = CreateToken<tokens::ErrorToken>(start_position_, position_);
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
      auto error = tokens::ErrorToken{};
      ecs::Set<ecs::TokenStop>(error, position_);
      ecs::Set<ecs::ErrorMessage>(error,
                                  std::string("unknown escape sequence"));
      return std::unexpected(error);
    }
  }
}

auto Lexer::GetHexEscapeSequence() -> std::expected<char, tokens::ErrorToken> {
  std::size_t cur = position_.index;
  std::size_t next = cur + 1;

  NextPosition(2);

  if (next >= input_.size()) {
    auto error = CreateToken<tokens::ErrorToken>(start_position_, position_);
    ecs::Set<ecs::ErrorMessage>(error, std::string("string not terminated"));
    return std::unexpected(error);
  }

  std::array<char, 3> raw = {input_[cur], input_[next], '\0'};
  char* end = nullptr;
  auto num = std::strtoul(raw.data(), &end, 16);

  if (end == raw.data() + 2) {
    return static_cast<char>(num);
  }

  auto error = tokens::ErrorToken{};
  ecs::Set<ecs::TokenStop>(error, position_);
  ecs::Set<ecs::ErrorMessage>(error,
                              std::string("unknown hex escape sequence"));
  return std::unexpected(error);
}

/*
 * hate strtoxx functions, thery're evil
 */
auto Lexer::GetNum() -> TokenVariant {
  const char* start = input_.data() + position_.index;
  char* int_end = nullptr;
  char* float_end = nullptr;

  std::uint64_t int_value = std::strtoull(start, &int_end, 0);
  long double float_value = std::strtold(start, &float_end);

  NextPosition(std::max(int_end - start, float_end - start));

  if (int_end == float_end) {
    auto result = CreateToken<tokens::Int>(start_position_, position_);
    ecs::Set<ecs::IntValue>(result, int_value);
    return result;
  }

  auto result = CreateToken<tokens::Float>(start_position_, position_);
  ecs::Set<ecs::FloatValue>(result, float_value);
  return result;
}

auto Lexer::GetIdOrKeyword() -> TokenVariant {
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
    return node->Value()(start_position_, position_);
  }

  auto result = CreateToken<tokens::IdToken>(start_position_, position_);
  ecs::Set<ecs::IdName>(result, std::move(accum_id));
  return result;
}

auto Lexer::GetOther() -> TokenVariant {
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
    return node->Value()(start_position_, position_);
  }

  auto error = CreateToken<tokens::ErrorToken>(start_position_, position_);
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

auto Lexer::AtEof() -> bool { return position_.index == input_.size(); }
auto Lexer::Getchr() -> char { return input_[position_.index]; }

auto Lexer::SkipWs() -> bool {
  bool res = false;

  while (!AtEof() && std::isspace(Getchr()) != 0) {
    res = true;
    NextPositionOnce();
  }

  return res;
}

void Lexer::BuildKeywords() {
#define KEYWORD(name)                                                 \
  {                                                                   \
    using KwType = tokens::Keyword_##name;                            \
    keywords_.Insert(std::string(#name), CreateTokenVariant<KwType>); \
  }

#include <language_data/keywords.dat>

#undef KEYWORD
}

void Lexer::BuildOtherTokens() {
#define EXPAND(name, type, str)                                          \
  {                                                                      \
    using KwType = tokens::type##name;                                   \
    other_tokens_.Insert(std::string(str), &CreateTokenVariant<KwType>); \
  }

#define UN_OP(name, str) EXPAND(name, Un, str)
#define BIN_OP(name, str) EXPAND(name, Bin, str)
#define SYNT_TOKEN(name, str) EXPAND(name, Syntax, str)

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

}  // namespace lexer
