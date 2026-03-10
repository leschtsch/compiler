#pragma once

#include <utils/source_position.hpp>
#include <utils/trie.hpp>
#include <utils/type_tuple.hpp>

#include <csignal>
#include <cstddef>
#include <expected>
#include <string>
#include <variant>

#include "tokens.hpp"

namespace lexer {

class Lexer {
 public:
  using TokenVariant = utils::ConvertTo<tokens::AllTokensTuple, std::variant>;

  explicit Lexer(std::string input);

  auto NextToken() -> TokenVariant;

 private:
  auto TrySkipComment() -> std::expected<bool, tokens::ErrorToken>;
  auto SkipCommentOneLiner() -> std::expected<bool, tokens::ErrorToken>;
  auto SkipCommentMultiLiner() -> std::expected<bool, tokens::ErrorToken>;

  auto GetString() -> TokenVariant;
  auto GetEscapeSequence() -> std::expected<char, tokens::ErrorToken>;
  auto GetHexEscapeSequence() -> std::expected<char, tokens::ErrorToken>;

  auto GetNum() -> TokenVariant;
  auto GetIdOrKeyword() -> TokenVariant;
  auto GetOther() -> TokenVariant;

  void NextPositionOnce();
  void NextPosition(size_t n);
  auto AtEof() -> bool;
  auto Getchr() -> char;
  auto SkipWs() -> bool;

  void BuildKeywords();
  void BuildOtherTokens();

  // NOTE: before you can change it to view, you need to implement GetNumwithout strtoull
  std::string input_;
  utils::SourcePosition position_{};
  bool reading_string_{false};
  std::string accumulated_string_;
  utils::SourcePosition start_position_;

  using TokenCreator = TokenVariant (*)(const utils::SourcePosition& start,
                                        const utils::SourcePosition& stop);
  utils::Trie<char, TokenCreator> keywords_;
  utils::Trie<char, TokenCreator> other_tokens_;
};

}  // namespace lexer
