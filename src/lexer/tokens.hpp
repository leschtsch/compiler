/**
 * @file tokens.hpp
 *
 * @note although lexer can't distinguish between unary
 * and binary operators, such as unary and binary minus, or multiplication and
 * dereference they're declared twice. Lexer should prefer binary, parser may
 * fix that.
 */
#pragma once

#include <ecs/ecs.hpp>

#include <cstdint>

namespace lexer {

#define UN_OP(name, x) kUn##name,
#define BIN_OP(name, x) kBin##name,
#define KEYWORD(name) kKw##name,
#define SYNT_TOKEN(name, x) kSynt##name,

enum class TokenType : std::uint8_t {
#include <language_data/bin_ops.dat>
#include <language_data/keywords.dat>
#include <language_data/synt_tokens.dat>
#include <language_data/un_ops.dat>

  kIntLiteral,
  kFloatLiteral,
  kStringLiteral,

  kErrorToken,
  kEofToken,
  kIdToken,
  kNTokenTypes
};

#undef UN_OP
#undef BIN_OP
#undef KEYWORD
#undef SYNT_TOKEN

class Token : public ecs::Entity {
 public:
  [[nodiscard]] explicit Token(TokenType type) : type_(type) {}

  [[nodiscard]] TokenType GetType() const { return type_; };

 private:
  TokenType type_{TokenType::kErrorToken};
};

}  // namespace lexer
