#pragma once

#include <utils/source_position.hpp>
#include <utils/type_tuple.hpp>

#include <csignal>
#include <cstddef>
#include <expected>
#include <string>
#include <string_view>
#include <variant>

#include "tokens.hpp"

namespace lexer {

class Lexer {
 public:
  using TokenVariant = utils::ConvertTo<tokens::AllTokensTuple, std::variant>;

  explicit Lexer(std::string_view input);

  auto NextToken() -> TokenVariant;

 private:
  std::string_view input_;
  utils::SourcePosition position_{};
  bool reading_string_{false};
  std::string accumulated_string_;
  utils::SourcePosition stash_position_;

  auto TrySkipComment() -> TokenVariant;
  auto SkipCommentOneLiner() -> TokenVariant;
  auto SkipCommentMultiLiner() -> TokenVariant;

  auto GetString() -> TokenVariant;
  auto GetEscapeSequence() -> std::expected<char, tokens::ErrorToken>;
  auto GetHexEscapeSequence() -> std::expected<char, tokens::ErrorToken>;

  void NextPositionOnce();
  void NextPosition(size_t n);
  auto AtEof() -> bool;
  auto Getchr() -> char;
};

}  // namespace lexer
