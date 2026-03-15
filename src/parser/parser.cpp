/**
 * @file parser.cpp
 *
 * rules:
 * error handling always happens on higher levels than it occured
 * (handle errors from non terminals only, not from terminals)
 */
#include "parser.hpp"

#include <ecs/ecs.hpp>
#include <ecs/used_components.hpp>
#include <lexer/lexer.hpp>
#include <lexer/tokens.hpp>
#include <utils/type_tuple.hpp>

#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "ast.hpp"

// NOLINTBEGIN(misc-no-recursion)

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
  explicit Parser(std::vector<lexer::Lexer::TokenVariant> input);
  auto Parse() -> AstNode;

 private:
  auto ParseFunctionDefinition() -> AstNode;
  auto ParseTypeDeclaration() -> AstNode;
  auto ParseName() -> AstNode;
  auto ParseParameterList() -> AstNode;
  auto ParseParameterDecl() -> AstNode;
  auto ParseBlockStatement() -> AstNode;
  auto ParseStatement() -> AstNode;
  auto ParseVariableDefinition() -> AstNode;
  auto ParseIfStatement() -> AstNode;
  auto ParseWhileStatement() -> AstNode;
  auto ParseReturnStatement() -> AstNode;
  auto ParseAssignmentOrCall() -> AstNode;
  auto ParseAssignment(AstNode locator) -> AstNode;
  auto ParseCallStatement(AstNode locator) -> AstNode;
  auto ParseLocator() -> AstNode;
  auto ParseExpression() -> AstNode;
  auto ParseLogExpr() -> AstNode;
  auto ParseComparExpr() -> AstNode;
  auto ParseAddExpr() -> AstNode;
  auto ParseMulExpr() -> AstNode;
  auto ParseUnaryExpr() -> AstNode;
  auto ParseLogNotExpr() -> AstNode;
  auto ParsePrimaryExpr() -> AstNode;
  auto ParseLiteral() -> AstNode;
  auto ParseLocatorOrCall() -> AstNode;

  auto ParseCallSuffix(AstNode& call) -> bool;

  void NextToken();

  template <typename... Tokens>
  auto SkipUntil() -> bool;

  template <typename... Tokens>
  auto Consume() -> bool;

  template <typename... Tokens>
  auto Lookup() -> bool;

  std::vector<lexer::Lexer::TokenVariant> input_;
  lexer::Lexer::TokenVariant cur_token_;
  std::size_t next_token_index_{1};
};

Parser::Parser(std::vector<lexer::Lexer::TokenVariant> input)
    : input_(std::move(input)), cur_token_(input_[0]){};

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

  while (success && Lookup<lexer::tokens::SyntaxComma>()) {
    success &=
        Consume<lexer::tokens::SyntaxComma>() and
        (PushCheckErr(result, ParseParameterDecl()) or
         SkipUntil<lexer::tokens::SyntaxRparent, lexer::tokens::SyntaxComma>());
  }

  success &= Consume<lexer::tokens::SyntaxRparent>();

  result.has_active_error |= !success;

  return result;
}

auto Parser::ParseParameterDecl() -> AstNode {
  auto result = AstNode{.token = tokens::ParameterDecl{},
                        .has_active_error = false,
                        .children = {}};

  bool success = PushCheckErr(result, ParseTypeDeclaration()) and
                 PushCheckErr(result, ParseName());

  result.has_active_error |= !success;

  return result;
}

auto Parser::ParseBlockStatement() -> AstNode {
  auto result = AstNode{.token = tokens::BlockStatement{},
                        .has_active_error = false,
                        .children = {}};

  bool success = Consume<lexer::tokens::SyntaxLbrace>();

  while (success && !Lookup<lexer::tokens::SyntaxRbrace>()) {
    success &= PushCheckErr(result, ParseStatement()) or
               SkipUntil<lexer::tokens::SyntaxRbrace>();
  }

  success &= Consume<lexer::tokens::SyntaxRbrace>();

  result.has_active_error |= !success;

  return result;
}

auto Parser::ParseStatement() -> AstNode {
  auto result = AstNode{
      .token = tokens::Statement{}, .has_active_error = false, .children = {}};

  bool success = true;

  if (Lookup<lexer::tokens::Keyword_char,
             lexer::tokens::Keyword_int,
             lexer::tokens::Keyword_uint,
             lexer::tokens::Keyword_float>()) {
    success &= PushCheckErr(result, ParseVariableDefinition()) or
               (SkipUntil<lexer::tokens::SyntaxSemicolon,
                          lexer::tokens::SyntaxRbrace>() and
                Consume<lexer::tokens::SyntaxSemicolon>());
  } else if (Lookup<lexer::tokens::Keyword_if>()) {
    success &= PushCheckErr(result, ParseIfStatement());
  } else if (Lookup<lexer::tokens::Keyword_while>()) {
    success &= PushCheckErr(result, ParseWhileStatement());
  } else if (Lookup<lexer::tokens::Keyword_return>()) {
    success &= PushCheckErr(result, ParseReturnStatement()) or
               (SkipUntil<lexer::tokens::SyntaxSemicolon,
                          lexer::tokens::SyntaxRbrace>() and
                Consume<lexer::tokens::SyntaxSemicolon>());
  } else if (Lookup<lexer::tokens::IdToken>()) {
    success &= PushCheckErr(result, ParseAssignmentOrCall()) or
               (SkipUntil<lexer::tokens::SyntaxSemicolon,
                          lexer::tokens::SyntaxRbrace>() and
                Consume<lexer::tokens::SyntaxSemicolon>());
  } else {
    success &= false;
    NextToken();
  }

  result.has_active_error |= !success;
  return result;
}

auto Parser::ParseVariableDefinition() -> AstNode {
  auto result = AstNode{.token = tokens::VariableDefinition{},
                        .has_active_error = false,
                        .children = {}};

  bool success = PushCheckErr(result, ParseTypeDeclaration()) and
                 PushCheckErr(result, ParseName()) and
                 Consume<lexer::tokens::BinAssign>() and
                 PushCheckErr(result, ParseExpression()) and
                 Consume<lexer::tokens::SyntaxSemicolon>();

  result.has_active_error |= !success;
  return result;
}

auto Parser::ParseIfStatement() -> AstNode {
  auto result = AstNode{.token = tokens::IfStatement{},
                        .has_active_error = false,
                        .children = {}};

  bool success = Consume<lexer::tokens::Keyword_if>() and
                 PushCheckErr(result, ParseExpression()) and
                 PushCheckErr(result, ParseBlockStatement());

  while (Lookup<lexer::tokens::Keyword_else>()) {
    success &= Consume<lexer::tokens::Keyword_else>();

    if (Lookup<lexer::tokens::Keyword_if>()) {
      success &= PushCheckErr(result, ParseIfStatement());
    } else {
      success &= PushCheckErr(result, ParseBlockStatement());
    }
  }

  result.has_active_error |= !success;
  return result;
}

auto Parser::ParseWhileStatement() -> AstNode {
  auto result = AstNode{.token = tokens::WhileStatement{},
                        .has_active_error = false,
                        .children = {}};

  bool success = Consume<lexer::tokens::Keyword_while>() and
                 PushCheckErr(result, ParseExpression()) and
                 PushCheckErr(result, ParseBlockStatement());

  result.has_active_error |= !success;
  return result;
}

auto Parser::ParseReturnStatement() -> AstNode {
  auto result = AstNode{.token = tokens::ReturnStatement{},
                        .has_active_error = false,
                        .children = {}};

  bool success = Consume<lexer::tokens::Keyword_return>();

  if (!Lookup<lexer::tokens::SyntaxSemicolon>()) {
    success &= PushCheckErr(result, ParseExpression());
  }

  success &= Consume<lexer::tokens::SyntaxSemicolon>();

  result.has_active_error |= !success;
  return result;
}

auto Parser::ParseAssignmentOrCall() -> AstNode {
  auto locator = ParseLocator();

  if (Lookup<lexer::tokens::SyntaxLparent>()) {
    return ParseCallStatement(std::move(locator));
  }

  return ParseAssignment(std::move(locator));
}

auto Parser::ParseAssignment(AstNode locator) -> AstNode {
  auto result = AstNode{
      .token = tokens::Assignment{}, .has_active_error = false, .children = {}};

  bool success = PushCheckErr(result, std::move(locator)) and
                 Consume<lexer::tokens::BinAssign>() and
                 PushCheckErr(result, ParseExpression()) and
                 Consume<lexer::tokens::SyntaxSemicolon>();

  result.has_active_error |= !success;
  return result;
}

auto Parser::ParseCallStatement(AstNode locator) -> AstNode {
  auto result = AstNode{.token = tokens::CallStatement{},
                        .has_active_error = false,
                        .children = {}};

  bool success = PushCheckErr(result, std::move(locator)) and
                 ParseCallSuffix(result) and
                 Consume<lexer::tokens::SyntaxSemicolon>();

  result.has_active_error |= !success;

  return result;
}

auto Parser::ParseLocator() -> AstNode {
  auto result = AstNode{
      .token = tokens::Locator{}, .has_active_error = false, .children = {}};

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

auto Parser::ParseExpression() -> AstNode { return ParseLogExpr(); }

auto Parser::ParseLogExpr() -> AstNode {
  auto result = AstNode{
      .token = tokens::LogExpr{}, .has_active_error = false, .children = {}};

  ParserVisitor<
      // Attrs
      utils::TypeTuple<ecs::TokenStart, ecs::TokenStop>,
      // Tokens
      utils::TypeTuple<lexer::tokens::BinLogicalAnd, tokens::LogAnd>,
      utils::TypeTuple<lexer::tokens::BinLogicalOr, tokens::LogOr>>
      visitor("expected && or ||");

  bool success = PushCheckErr(result, ParseComparExpr());
  while (success &&
         Lookup<lexer::tokens::BinLogicalAnd, lexer::tokens::BinLogicalOr>()) {
    success &= PushCheckErr(result, std::visit(visitor, cur_token_));
    NextToken();
    success &= PushCheckErr(result, ParseComparExpr());
  }

  result.has_active_error |= !success;
  return result;
}

auto Parser::ParseComparExpr() -> AstNode {
  auto result = AstNode{
      .token = tokens::ComparExpr{}, .has_active_error = false, .children = {}};

  ParserVisitor<
      // Attrs
      utils::TypeTuple<ecs::TokenStart, ecs::TokenStop>,
      // Tokens
      utils::TypeTuple<lexer::tokens::BinEq, tokens::BinEq>,
      utils::TypeTuple<lexer::tokens::BinNeq, tokens::BinNeq>,
      utils::TypeTuple<lexer::tokens::BinLt, tokens::BinLt>,
      utils::TypeTuple<lexer::tokens::BinLeq, tokens::BinLeq>,
      utils::TypeTuple<lexer::tokens::BinGt, tokens::BinGt>,
      utils::TypeTuple<lexer::tokens::BinGeq, tokens::BinGeq>>
      visitor("expected comparison operator");

  bool success = PushCheckErr(result, ParseAddExpr());
  while (success && Lookup<lexer::tokens::BinEq,
                           lexer::tokens::BinNeq,
                           lexer::tokens::BinLt,
                           lexer::tokens::BinLeq,
                           lexer::tokens::BinGt,
                           lexer::tokens::BinGeq>()) {
    success &= PushCheckErr(result, std::visit(visitor, cur_token_));
    NextToken();
    success &= PushCheckErr(result, ParseAddExpr());
  }

  result.has_active_error |= !success;
  return result;
}

auto Parser::ParseAddExpr() -> AstNode {
  auto result = AstNode{
      .token = tokens::AddExpr{}, .has_active_error = false, .children = {}};

  ParserVisitor<
      // Attrs
      utils::TypeTuple<ecs::TokenStart, ecs::TokenStop>,
      // Tokens
      utils::TypeTuple<lexer::tokens::BinPlus, tokens::BinPlus>,
      utils::TypeTuple<lexer::tokens::BinMinus, tokens::BinMinus>>
      visitor("expected + or -");

  bool success = PushCheckErr(result, ParseMulExpr());
  while (success && Lookup<lexer::tokens::BinPlus, lexer::tokens::BinMinus>()) {
    success &= PushCheckErr(result, std::visit(visitor, cur_token_));
    NextToken();
    success &= PushCheckErr(result, ParseMulExpr());
  }

  result.has_active_error |= !success;
  return result;
}

auto Parser::ParseMulExpr() -> AstNode {
  auto result = AstNode{
      .token = tokens::MulExpr{}, .has_active_error = false, .children = {}};

  ParserVisitor<
      // Attrs
      utils::TypeTuple<ecs::TokenStart, ecs::TokenStop>,
      // Tokens
      utils::TypeTuple<lexer::tokens::BinMul, tokens::BinMul>,
      utils::TypeTuple<lexer::tokens::BinDiv, tokens::BinDiv>,
      utils::TypeTuple<lexer::tokens::BinMod, tokens::BinMod>>
      visitor("expected *, / or %");

  bool success = PushCheckErr(result, ParseUnaryExpr());
  while (success && Lookup<lexer::tokens::BinMul,
                           lexer::tokens::BinDiv,
                           lexer::tokens::BinMod>()) {
    success &= PushCheckErr(result, std::visit(visitor, cur_token_));
    NextToken();
    success &= PushCheckErr(result, ParseUnaryExpr());
  }

  result.has_active_error |= !success;
  return result;
}

auto Parser::ParseUnaryExpr() -> AstNode {
  auto result = AstNode{
      .token = tokens::UnaryExpr{}, .has_active_error = false, .children = {}};

  bool success = true;

  if (Lookup<lexer::tokens::UnLogicalNot>()) {
    success = PushCheckErr(result, ParseLogNotExpr());
  } else {
    success = PushCheckErr(result, ParsePrimaryExpr());
  }

  result.has_active_error |= !success;
  return result;
}

auto Parser::ParseLogNotExpr() -> AstNode {
  auto result = AstNode{
      .token = tokens::LogNotExpr{}, .has_active_error = false, .children = {}};

  bool success = Consume<lexer::tokens::UnLogicalNot>() and
                 PushCheckErr(result, ParsePrimaryExpr());

  result.has_active_error |= !success;
  return result;
}

auto Parser::ParsePrimaryExpr() -> AstNode {
  auto result = AstNode{.token = tokens::PrimaryExpr{},
                        .has_active_error = false,
                        .children = {}};

  bool success = true;

  if (Lookup<lexer::tokens::SyntaxLparent>()) {
    // clang-format off
    success = (
                Consume<lexer::tokens::SyntaxLparent>() and
                PushCheckErr(result, ParseExpression()) and
                Consume<lexer::tokens::SyntaxRparent>()
              ) or (
                SkipUntil<lexer::tokens::SyntaxRparent,
                         lexer::tokens::SyntaxSemicolon,
                         lexer::tokens::SyntaxRbrace>() and
                Consume<lexer::tokens::SyntaxRparent>()
              );
    // clang-format on

  } else if (Lookup<lexer::tokens::IdToken>()) {
    success = PushCheckErr(result, ParseLocatorOrCall());
  } else {
    success = PushCheckErr(result, ParseLiteral());
  }

  result.has_active_error |= !success;
  return result;
}

auto Parser::ParseLocatorOrCall() -> AstNode {
  auto locator = ParseLocator();

  if (!Lookup<lexer::tokens::SyntaxLparent>()) {
    return locator;
  }

  auto result = AstNode{.token = tokens::CallExpression{},
                        .has_active_error = false,
                        .children = {}};

  bool success =
      PushCheckErr(result, std::move(locator)) and ParseCallSuffix(result);
  result.has_active_error |= !success;
  return result;
}

auto Parser::ParseCallSuffix(AstNode& call) -> bool {
  bool success = Consume<lexer::tokens::SyntaxLparent>() and
                 (Lookup<lexer::tokens::SyntaxRparent>()
                      ? true
                      : (PushCheckErr(call, ParseExpression()) or
                         SkipUntil<lexer::tokens::SyntaxRparent,
                                   lexer::tokens::SyntaxComma>()));

  while (success && Lookup<lexer::tokens::SyntaxComma>()) {
    success &=
        Consume<lexer::tokens::SyntaxComma>() and
        (PushCheckErr(call, ParseExpression()) or
         SkipUntil<lexer::tokens::SyntaxRparent, lexer::tokens::SyntaxComma>());
  }

  success &= Consume<lexer::tokens::SyntaxRparent>();

  return success;
}

auto Parser::ParseLiteral() -> AstNode {
  ParserVisitor<
      // Attrs
      utils::TypeTuple<ecs::TokenStart,
                       ecs::TokenStop,
                       ecs::IntValue,
                       ecs::FloatValue,
                       ecs::StrValue>,
      // Tokens
      utils::TypeTuple<lexer::tokens::Int, tokens::IntLiteral>,
      utils::TypeTuple<lexer::tokens::Float, tokens::FloatLiteral>,
      utils::TypeTuple<lexer::tokens::String, tokens::StrLiteral>>
      visitor("expected literal");

  auto res = std::visit(visitor, cur_token_);

  if (!res.has_active_error) {
    NextToken();
  }

  return res;
}

void Parser::NextToken() { 
  if (next_token_index_ == input_.size())
  {
    return;
  }

  cur_token_ = input_[next_token_index_++]; }

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

auto Parse(std::vector<lexer::Lexer::TokenVariant> input) -> AstNode{
  Parser parser(std::move(input));
  return parser.Parse();
}

}  // namespace parser

// NOLINTEND(misc-no-recursion)
