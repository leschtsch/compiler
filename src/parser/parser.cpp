#include "parser.hpp"

#include <ecs/ecs.hpp>
#include <ecs/used_components.hpp>
#include <lexer/lexer.hpp>
#include <lexer/tokens.hpp>
#include <utils/type_tuple.hpp>

#include <string>
#include <utility>
#include <variant>

#include "ast.hpp"

namespace parser {

namespace {

template <typename T, utils::IsTypeTuple TT>
auto CreateNodeAndGrabAttributes(ecs::Entity entity) -> AstNode {
  auto tok = T{};
  auto result =
      AstNode{.token = tok, .has_active_error = false, .children = {}};

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

template <utils::IsTypeTuple Attrs, utils::IsTypeTuple... Pairs>
class ParserVisitorOne;

template <utils::IsTypeTuple Attrs, typename T, typename U>
class ParserVisitorOne<Attrs, utils::TypeTuple<T, U>> {
 public:
  auto operator()(T tok) -> AstNode {
    return CreateNodeAndGrabAttributes<U, Attrs>(tok);
  }
};

template <utils::IsTypeTuple Attrs, utils::IsTypeTuple... Pairs>
class ParserVisitor : private ParserVisitorOne<Attrs, Pairs>... {
 public:
  explicit ParserVisitor(std::string err_msg) : err_msg_(std::move(err_msg)) {}

  auto operator()(auto tok) -> AstNode {
    AstNode res = CreateNodeAndGrabAttributes<tokens::ErrorToken, Attrs>(tok);
    res.has_active_error = true;

    auto entity = std::get<tokens::ErrorToken>(res.token);
    ecs::Set<ecs::ErrorMessage>(entity, std::move(err_msg_));
    return res;
  }

  using ParserVisitorOne<Attrs, Pairs>::operator()...;

 private:
  std::string err_msg_;
};

auto PushCheckErr(AstNode& parent, AstNode&& child) -> bool {
  bool is_error = (child.has_active_error);
  parent.children.push_back(std::move(child));

  return !is_error;
}

}  // namespace

class Parser {
 public:
  explicit Parser(const std::string& input);
  auto Parse() -> AstNode;

 private:
  auto ParseFunctionDefinition() -> AstNode;
  auto ParseTypeDeclaration() -> AstNode;
  auto ParseName() -> AstNode;
  auto ParseParameterList() -> AstNode;
  auto ParseParameterDecl() -> AstNode;
  auto ParseBlockStatement() -> AstNode;

  void NextToken();

  template <typename... Tokens>
  auto SkipUntil() -> bool;

  template <typename... Tokens>
  auto Consume() -> bool;

  template <typename... Tokens>
  auto Lookup() -> bool;

  lexer::Lexer lexer_;
  lexer::Lexer::TokenVariant cur_token_;
};

Parser::Parser(const std::string& input)
    : lexer_(input), cur_token_(lexer_.NextToken()) {};

auto Parser::Parse() -> AstNode {
  auto result = AstNode{
      .token = tokens::Program{}, .has_active_error = false, .children = {}};

  while (!Lookup<lexer::tokens::EofToken>()) {
    bool success = PushCheckErr(result, ParseFunctionDefinition()) or
                   SkipUntil<lexer::tokens::Keyword_char,
                             lexer::tokens::Keyword_int,
                             lexer::tokens::Keyword_uint,
                             lexer::tokens::Keyword_float>();
    result.has_active_error |= !success;
  }

  return result;
}

auto Parser::ParseFunctionDefinition() -> AstNode {
  auto result = AstNode{.token = tokens::FunctionDefinition{},
                        .has_active_error = false,
                        .children = {}};

  bool success = PushCheckErr(result, ParseTypeDeclaration()) and
                 PushCheckErr(result, ParseName()) and
                 PushCheckErr(result, ParseParameterList()) and
                 PushCheckErr(result, ParseBlockStatement());
  result.has_active_error |= !success;

  return result;
}

auto Parser::ParseTypeDeclaration() -> AstNode {
  auto result = AstNode{.token = tokens::TypeDeclaration{},
                        .has_active_error = false,
                        .children = {}};

  ParserVisitor<
      // Attrs
      utils::TypeTuple<ecs::TokenStart, ecs::TokenStop>,
      // Tokens
      utils::TypeTuple<lexer::tokens::Keyword_char, tokens::KeywordChar>,
      utils::TypeTuple<lexer::tokens::Keyword_int, tokens::KeywordInt>,
      utils::TypeTuple<lexer::tokens::Keyword_uint, tokens::KeywordUint>,
      utils::TypeTuple<lexer::tokens::Keyword_float, tokens::KeywordFloat>>
      visitor("expected type");

  auto tmp = std::visit(visitor, cur_token_);

  bool success = PushCheckErr(result, std::move(tmp));
  result.has_active_error |= !success;

  NextToken();
  return result;
}

auto Parser::ParseName() -> AstNode {
  auto result = AstNode{
      .token = tokens::Name{}, .has_active_error = false, .children = {}};

  ParserVisitor<
      // Attrs
      utils::TypeTuple<ecs::TokenStart, ecs::TokenStop, ecs::IdName>,
      // Tokens
      utils::TypeTuple<lexer::tokens::IdToken, tokens::IdToken>>
      visitor("expected id");

  auto tmp = std::visit(visitor, cur_token_);

  bool success = PushCheckErr(result, std::move(tmp));
  result.has_active_error |= !success;

  NextToken();
  return result;
}

auto Parser::ParseParameterList() -> AstNode {
  auto result = AstNode{.token = tokens::ParameterList{},
                        .has_active_error = false,
                        .children = {}};

  bool success = Consume<lexer::tokens::SyntaxLparent>() and
                 (Lookup<lexer::tokens::SyntaxRparent>()
                      ? true
                      : (PushCheckErr(result, ParseParameterDecl()) or
                         SkipUntil<lexer::tokens::SyntaxRparent,
                                   lexer::tokens::SyntaxComma>()));

  while (Lookup<lexer::tokens::SyntaxComma>()) {
    success &=
        Consume<lexer::tokens::SyntaxComma>() and
        (PushCheckErr(result, ParseParameterDecl()) or
         SkipUntil<lexer::tokens::SyntaxRparent, lexer::tokens::SyntaxComma>());
  }

  success &= Consume<lexer::tokens::SyntaxRparent>();

  result.has_active_error &= !success;

  return result;
}

auto Parser::ParseParameterDecl() -> AstNode {
  auto result = AstNode{.token = tokens::ParameterDecl{},
                        .has_active_error = false,
                        .children = {}};

  bool success = PushCheckErr(result, ParseTypeDeclaration()) and
                 PushCheckErr(result, ParseName());

  result.has_active_error &= !success;

  return result;
}

auto Parser::ParseBlockStatement() -> AstNode {
  auto result = AstNode{.token = tokens::BlockStatement{},
                        .has_active_error = false,
                        .children = {}};

  bool success = Consume<lexer::tokens::SyntaxLbrace>() &&
                 Consume<lexer::tokens::SyntaxRbrace>();
  result.has_active_error &= !success;

  return result;
}

//TODO token_iterator++
void Parser::NextToken() { cur_token_ = lexer_.NextToken(); }

template <typename... Tokens>
auto Parser::SkipUntil() -> bool {
  while (true) {
    if ((std::holds_alternative<Tokens>(cur_token_) || ...)) {
      return true;
    }

    if (std::holds_alternative<lexer::tokens::EofToken>(cur_token_)) {
      return false;
    }

    NextToken();
  }
  return true;
}

// TODO error report (probably just print, no error tokens)
template <typename... Tokens>
auto Parser::Consume() -> bool {
  if (Lookup<Tokens...>()) {
    NextToken();
    return true;
  }

  return false;
}

template <typename... Tokens>
auto Parser::Lookup() -> bool {
  return static_cast<bool>((std::holds_alternative<Tokens>(cur_token_) || ...));
}

auto Parse(const std::string& input) -> AstNode {
  Parser parser(input);
  return parser.Parse();
}

}  // namespace parser
