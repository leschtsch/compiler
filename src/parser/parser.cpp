#include "parser.hpp"

#include <ecs/ecs.hpp>
#include <ecs/used_components.hpp>
#include <lexer/lexer.hpp>
#include <utils/overloaded.hpp>

#include <string>
#include <variant>

#include "ast.hpp"

namespace parser {

class Parser {
 public:
  explicit Parser(const std::string& input);
  auto Parse() -> AstNode;

 private:
  auto ParseFunctionDefinition() -> AstNode;
  auto ParseTypeDeclaration() -> AstNode;
  auto ParseName() -> AstNode;
  auto ParseParameterList() -> AstNode;

  void NextToken();

  template <typename T, utils::IsTypeTuple Tt>
  static auto CreateTokenAndGrabAttributes(ecs::Entity entity) -> AstNode;

  /*
   * TODO
   *
   * may be probably used for some error recovery, but not yet
   * also need to return positions of error, and produce meaningful messages...
   */
  template <typename... Tokens>
  void SkipUntil();

  lexer::Lexer lexer_;
  lexer::Lexer::TokenVariant cur_token_;
};

Parser::Parser(const std::string& input)
    : lexer_(input), cur_token_(lexer_.NextToken()) {};

auto Parser::Parse() -> AstNode {
  auto result = AstNode{.token = tokens::Program{}, .children = {}};

  while (!std::holds_alternative<lexer::tokens::EofToken>(cur_token_)) {
    result.children.push_back(ParseFunctionDefinition());
  }

  return result;
}

auto Parser::ParseFunctionDefinition() -> AstNode {
  auto result = AstNode{.token = tokens::FunctionDefinition{}, .children = {}};

  result.children.push_back(ParseTypeDeclaration());
  SkipUntil<lexer::tokens::EofToken, lexer::tokens::IdToken>();

  result.children.push_back(ParseName());
  SkipUntil<lexer::tokens::EofToken, lexer::tokens::SyntaxLparent>();

  result.children.push_back(ParseParameterList());
  SkipUntil<lexer::tokens::EofToken, lexer::tokens::SyntaxLbrace>();

  return result;
}

auto Parser::ParseTypeDeclaration() -> AstNode {
  using Attrs = utils::TypeTuple<ecs::TokenStart, ecs::TokenStop>;
  auto result = AstNode{.token = tokens::TypeDeclaration{}, .children = {}};
  auto tmp = std::visit(
      utils::Overloaded{
          [](auto tok) -> AstNode {
            return CreateTokenAndGrabAttributes<tokens::ErrorToken, Attrs>(tok);
          },
          [](lexer::tokens::Keyword_char tok) -> AstNode {
            return CreateTokenAndGrabAttributes<tokens::KeywordChar, Attrs>(
                tok);
          },
          [](lexer::tokens::Keyword_int tok) -> AstNode {
            return CreateTokenAndGrabAttributes<tokens::KeywordInt, Attrs>(tok);
          },
          [](lexer::tokens::Keyword_uint tok) -> AstNode {
            return CreateTokenAndGrabAttributes<tokens::KeywordUint, Attrs>(
                tok);
          },
          [](lexer::tokens::Keyword_float tok) -> AstNode {
            return CreateTokenAndGrabAttributes<tokens::KeywordFloat, Attrs>(
                tok);
          },
      },
      cur_token_);

  result.children.push_back(std::move(tmp));
  NextToken();
  return result;
}

auto Parser::ParseName() -> AstNode {
  using Attrs = utils::TypeTuple<ecs::TokenStart, ecs::TokenStop, ecs::IdName>;
  auto result = AstNode{.token = tokens::Name{}, .children = {}};
  auto tmp = std::visit(
      utils::Overloaded{
          [](auto tok) -> AstNode {
            return CreateTokenAndGrabAttributes<tokens::ErrorToken, Attrs>(tok);
          },
          [](lexer::tokens::IdToken tok) -> AstNode {
            return CreateTokenAndGrabAttributes<tokens::IdToken, Attrs>(tok);
          },
      },
      cur_token_);
  result.children.push_back(std::move(tmp));
  NextToken();
  return result;
}

auto Parser::ParseParameterList() -> AstNode {
  auto result = AstNode{.token = tokens::ParameterList{}, .children = {}};
  NextToken();
  return result;
}

void Parser::NextToken() { cur_token_ = lexer_.NextToken(); }

template <typename T, utils::IsTypeTuple TT>
auto Parser::CreateTokenAndGrabAttributes(ecs::Entity entity) -> AstNode {
  auto tok = T{};
  auto result = AstNode{.token = tok, .children = {}};

  [&tok, &entity]<typename... Attrs>(utils::TypeTuple<Attrs...>) -> void {
    (
        [&tok, &entity]() -> void {
          auto attr = ecs::Get<Attrs>(entity);

          if (attr.HasValue()) {
            ecs::Set<Attrs>(tok, std::move(attr.Value()));
          }

          ecs::Drop<Attrs>(entity);
        }(),
        ...);
  }(TT{});

  return result;
}
template <typename... Tokens>
void Parser::SkipUntil() {
  while (!(std::holds_alternative<Tokens>(cur_token_) || ...)) {
    NextToken();
  }
}

auto Parse(const std::string& input) -> AstNode {
  Parser parser(input);
  return parser.Parse();
}

}  // namespace parser
