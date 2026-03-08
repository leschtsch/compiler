#include "lexer.hpp"

#include <ecs/ecs.hpp>
#include <ecs/used_components.hpp>

#include <array>
#include <cstddef>
#include <cstdlib>
#include <expected>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include "tokens.hpp"

namespace lexer {

Lexer::Lexer(std::string_view input) : input_(input) {}

auto Lexer::NextToken() -> Lexer::TokenVariant {
  if (reading_string_) {
    return GetString();
  }

  auto skip_comment = TrySkipComment();
  if (!std::holds_alternative<tokens::NopToken>(skip_comment)) {
    return skip_comment;
  }

  if (AtEof()) {
    return tokens::EofToken{};
  }

  if (Getchr() == '"') {
    return GetString();
  }

  return tokens::EofToken{};
}

auto Lexer::TrySkipComment() -> TokenVariant {
  std::size_t cur = position_.index;
  std::size_t next = cur + 1;

  if (next >= input_.size()) {
    return tokens::NopToken{};
  }

  if (input_[cur] != '/') {
    return tokens::NopToken{};
  }

  if (input_[next] == '/') {
    return SkipCommentOneLiner();
  }

  if (input_[next] == '*') {
    return SkipCommentMultiLiner();
  }

  return tokens::NopToken{};
}

auto Lexer::SkipCommentOneLiner() -> TokenVariant {
  NextPosition(2);

  while (!AtEof() && Getchr() != '\n') {
    NextPositionOnce();
  }

  if (!AtEof()) {  // we're at '\n'
    NextPositionOnce();
  }

  return tokens::NopToken{};
}

auto Lexer::SkipCommentMultiLiner() -> TokenVariant {
  stash_position_ = position_;

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
    return tokens::NopToken{};
  }

  auto error = tokens::ErrorToken{};
  ecs::Set<ErrorStart>(error, stash_position_);
  ecs::Set<ErrorStop>(error, position_);
  ecs::Set<ErrorMessage>(error, std::string("comment not closed"));
  return error;
}

auto Lexer::GetString() -> TokenVariant {
  if (!reading_string_) {
    stash_position_ = position_;
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
    auto result = tokens::String{};
    ecs::Set<StrValue>(result, std::move(accumulated_string_));
    return result;
  }

  auto error = tokens::ErrorToken{};
  ecs::Set<ErrorStart>(error, stash_position_);
  ecs::Set<ErrorStop>(error, position_);
  ecs::Set<ErrorMessage>(error, std::string("string not terminated"));
  return error;
}

auto Lexer::GetEscapeSequence() -> std::expected<char, tokens::ErrorToken> {
  NextPositionOnce();

  if (AtEof()) {
    auto error = tokens::ErrorToken{};
    ecs::Set<ErrorStart>(error, stash_position_);
    ecs::Set<ErrorStop>(error, position_);
    ecs::Set<ErrorMessage>(error, std::string("string not terminated"));
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
      ecs::Set<ErrorStop>(error, position_);
      ecs::Set<ErrorMessage>(error, std::string("unknown escape sequence"));
      return std::unexpected(error);
    }
  }
}

auto Lexer::GetHexEscapeSequence() -> std::expected<char, tokens::ErrorToken> {
  std::size_t cur = position_.index;
  std::size_t next = cur + 1;

  NextPosition(2);

  if (next >= input_.size()) {
    auto error = tokens::ErrorToken{};
    ecs::Set<ErrorStart>(error, stash_position_);
    ecs::Set<ErrorStop>(error, position_);
    ecs::Set<ErrorMessage>(error, std::string("string not terminated"));
    return std::unexpected(error);
  }

  std::array<char, 3> raw = {input_[cur], input_[next], '\0'};
  char* end = nullptr;
  auto num = std::strtoul(raw.data(), &end, 16);

  if (end == raw.data() + 2) {
    return static_cast<char>(num);
  }

  auto error = tokens::ErrorToken{};
  ecs::Set<ErrorStop>(error, position_);
  ecs::Set<ErrorMessage>(error, std::string("unknown hex escape sequence"));
  return std::unexpected(error);
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

};  // namespace lexer
