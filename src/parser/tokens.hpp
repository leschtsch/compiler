#include <ecs/ecs.hpp>
#include <utils/type_tuple.hpp>

#include <string>

namespace parser::tokens {

struct BaseToken : ecs::Entity {};

#define TOKEN(Token, Parent) \
  struct Token : public Parent {};

#include <language_data/grammar_tokens.dat>

#undef TOKEN

#define TOKEN(Token, Parent) Token,

using AllTokensTuple = utils::TypeTuple<
#include <language_data/grammar_tokens.dat>
    BaseToken>;

#undef TOKEN

template <typename T>
auto GetName(T) -> std::string = delete;

template <>
inline auto GetName<BaseToken>(BaseToken /* tok */) -> std::string {
  return "BaseToken";
}

#define TOKEN(Token, Parent)                         \
  template <>                                        \
  inline auto GetName<Token>(Token) -> std::string { \
    return #Token;                                   \
  }

#include <language_data/grammar_tokens.dat>

#undef TOKEN
}  // namespace parser::tokens
