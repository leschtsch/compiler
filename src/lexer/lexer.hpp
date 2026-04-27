#pragma once

#include <utils/source_position.hpp>
#include <utils/trie.hpp>

#include <csignal>
#include <cstddef>
#include <expected>
#include <string>

#include "tokens.hpp"

namespace lexerv2 {

class Lexer {
 public:
  explicit Lexer(std::string input);

  Token NextToken();

 private:
  std::expected<bool, Token> TrySkipComment();
  std::expected<bool, Token> SkipCommentOneLiner();
  std::expected<bool, Token> SkipCommentMultiLiner();

  Token GetString();
  std::expected<char, Token> GetEscapeSequence();
  std::expected<char, Token> GetHexEscapeSequence();

  Token GetNum();
  Token GetIdOrKeyword();
  Token GetOther();

  void NextPositionOnce();
  void NextPosition(size_t n);
  bool SkipWs();

  [[nodiscard]] bool AtEof();
  [[nodiscard]] char Getchr();

  void BuildKeywords();
  void BuildOtherTokens();

  // NOTE: before you can change it to view, you need to implement GetNum
  // without strtoull
  std::string input_;
  utils::SourcePosition position_{};
  bool reading_string_{false};
  std::string accumulated_string_;
  utils::SourcePosition start_position_;

  utils::Trie<char, TokenType> keywords_;
  utils::Trie<char, TokenType> other_tokens_;
};

}  // namespace lexerv2
