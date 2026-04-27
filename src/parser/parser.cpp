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
#include <lexer/tokens.hpp>
#include <utils/overloaded.hpp>

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "ast.hpp"

// NOLINTBEGIN(misc-no-recursion)

namespace parser {

namespace {

//===========================V=HELPERS=V============================================================

[[nodiscard]] bool IsHealthy(const nodes::NodesVariant& node) {
  auto visitor = [](const auto& ptr) -> bool { return ptr->Healthy(); };
  return std::visit(visitor, node);
}

bool StoreCheckErr(nodes::NodesVariant& store, nodes::NodesVariant&& child) {
  bool is_healthy = IsHealthy(child);
  store = std::move(child);
  return is_healthy;
}

bool StoreChild(nodes::NodesVariant& parent,
                nodes::NodesVariant&& child,
                std::size_t idx) {
  bool success = IsHealthy(child);

  auto visitor = utils::Overloaded{
      [](nodes::BaseNode*) -> void {
        throw std::runtime_error("setting child of empty node");
      },
      [&child, idx]<std::size_t n_children>(nodes::NaryNode<n_children>* ptr)
          -> void { ptr->Children()[idx] = std::move(child); },
      [&child, idx](nodes::ArbitraryNode* ptr) -> void {
        ptr->Children()[idx] = std::move(child);
      }};

  nodes::VisitPtr(visitor, parent);

  return success;
}

bool PushCheckErr(std::vector<nodes::NodesVariant>& store,
                  nodes::NodesVariant&& child) {
  bool is_healthy = IsHealthy(child);
  store.push_back(std::move(child));
  return is_healthy;
}

void SetHealthy(nodes::NodesVariant& node, bool healthy) {
  std::visit([healthy](auto& ptr) -> void { ptr->Healthy() = healthy; }, node);
}

template <typename... Attrs>
void MoveAttributes(ecs::Entity dst, ecs::Entity src) {
  (
      [&dst, &src]() -> void {
        auto attr = ecs::Get<Attrs>(src);
        ecs::Set<Attrs>(dst, std::move(attr.Value()));
        ecs::Drop<Attrs>(src);
      }(),
      ...);
}

//===========================^=HELPERS=^============================================================

//===========================V=TOKEN=TO=NODE=V======================================================

template <lexer::TokenType token_type, typename AstNode, typename... Attrs>
struct TokenCase {
  static bool CreateNode(nodes::NodesVariant& result,
                         lexer::Token& token,
                         lexer::TokenType type) {
    if (token_type != type) {
      return false;
    }

    auto tmp = std::make_unique<AstNode>();
    MoveAttributes<Attrs...>(*tmp, token);
    result = std::move(tmp);

    return true;
  }
};

/**
 * Syntactic sugar.
 *
 * @note It's like when you put sugar in your porridge. You hate sugar,
 * but you hate porridge more. It's still tastes awfull, but now you can eat it,
 * at least.
 *
 * This function matches cases against current token's type.
 * If match found, node of type AstNode is created and all Attrs from ecs
 * are copied from token to new node.
 *
 * Else ErrorNode is created, TokenStart and TokenStop are copied and
 * ErrorMessage set to error_msg.
 */
template <typename... Cases>
[[nodiscard]] nodes::NodesVariant TokenToNode(lexer::Token& token,
                                              std::string error_msg) {
  nodes::NodesVariant result(std::unique_ptr<nodes::BaseNode>(nullptr));
  lexer::TokenType token_type = token.GetType();

  bool success = ((Cases::CreateNode(result, token, token_type)) || ...);

  if (success) {
    return result;
  }

  auto tmp = std::make_unique<nodes::ErrorNode>();

  tmp->Healthy() = false;
  success = false;

  ecs::Set<ecs::ErrorMessage>(*tmp, error_msg);
  MoveAttributes<ecs::TokenStart, ecs::TokenStop>(*tmp, token);

  return tmp;
};

//===========================^=TOKEN=TO=NODE=^======================================================

//===========================V=PARSER=V=============================================================

class Parser {
 public:
  [[nodiscard]] explicit Parser(std::vector<lexer::Token> input);
  [[nodiscard]] nodes::NodesVariant Parse();

 private:
  [[nodiscard]] nodes::NodesVariant ParseFunctionDefinition();
  [[nodiscard]] nodes::NodesVariant ParseTypeDeclaration();
  [[nodiscard]] nodes::NodesVariant ParseName();
  [[nodiscard]] nodes::NodesVariant ParseParameterList();
  [[nodiscard]] nodes::NodesVariant ParseParameterDecl();
  [[nodiscard]] nodes::NodesVariant ParseBlockStatement();
  void TryFixStatement(nodes::NodesVariant& result);
  [[nodiscard]] nodes::NodesVariant ParseStatement();
  [[nodiscard]] nodes::NodesVariant ParseVariableDefinition();
  [[nodiscard]] nodes::NodesVariant ParseIfStatement();
  [[nodiscard]] nodes::NodesVariant ParseWhileStatement();
  [[nodiscard]] nodes::NodesVariant ParseReturnStatement();
  [[nodiscard]] nodes::NodesVariant ParseAssignmentOrCall();
  [[nodiscard]] nodes::NodesVariant ParseAssignment(
      nodes::NodesVariant&& locator);
  [[nodiscard]] nodes::NodesVariant ParseCallStatement(
      nodes::NodesVariant&& locator);
  [[nodiscard]] nodes::NodesVariant ParseLocator();
  [[nodiscard]] nodes::NodesVariant ParseExpression();
  [[nodiscard]] nodes::NodesVariant ParseLogExpr();
  [[nodiscard]] nodes::NodesVariant ParseComparExpr();
  [[nodiscard]] nodes::NodesVariant ParseAddExpr();
  [[nodiscard]] nodes::NodesVariant ParseMulExpr();
  [[nodiscard]] nodes::NodesVariant ParseUnaryExpr();
  [[nodiscard]] nodes::NodesVariant ParseLogNotExpr();
  [[nodiscard]] nodes::NodesVariant ParsePrimaryExpr();
  [[nodiscard]] nodes::NodesVariant ParseLiteral();
  [[nodiscard]] nodes::NodesVariant ParseLocatorOrCall();
  [[nodiscard]] nodes::NodesVariant ParseCallSuffix();

  void NextToken();

  template <lexer::TokenType... tokens>
  bool SkipUntil();

  template <lexer::TokenType... tokens>
  bool Consume();

  template <lexer::TokenType... tokens>
  [[nodiscard]] bool Lookup();

  std::vector<lexer::Token> input_;
  lexer::Token cur_token_;
  std::size_t next_token_index_{1};
};

Parser::Parser(std::vector<lexer::Token> input)
    : input_(std::move(input)), cur_token_(input_[0]) {};

nodes::NodesVariant Parser::Parse() {
  auto result = std::make_unique<nodes::Program>();

  while (!Lookup<lexer::TokenType::kEofToken>()) {
    bool success =
        PushCheckErr(result->Children(), ParseFunctionDefinition()) or
        SkipUntil<lexer::TokenType::kKwchar,
                  lexer::TokenType::kKwint,
                  lexer::TokenType::kKwuint,
                  lexer::TokenType::kKwfloat>();
    result->Healthy() &= success;
  }

  return result;
}

nodes::NodesVariant Parser::ParseFunctionDefinition() {
  auto result = std::make_unique<nodes::FunctionDefinition>();

  bool success =
      StoreCheckErr(result->Children()[0], ParseTypeDeclaration()) and
      StoreCheckErr(result->Children()[1], ParseName()) and
      StoreCheckErr(result->Children()[2], ParseParameterList()) and
      StoreCheckErr(result->Children()[3], ParseBlockStatement());

  result->Healthy() &= success;

  return result;
}

nodes::NodesVariant Parser::ParseTypeDeclaration() {
  auto result = std::make_unique<nodes::TypeDeclaration>();

  // clang-format off
  auto child = TokenToNode<
    TokenCase<lexer::TokenType::kKwchar, nodes::KeywordChar, ecs::TokenStart, ecs::TokenStop>,
    TokenCase<lexer::TokenType::kKwint, nodes::KeywordInt, ecs::TokenStart, ecs::TokenStop>,
    TokenCase<lexer::TokenType::kKwuint, nodes::KeywordUint, ecs::TokenStart, ecs::TokenStop>,
    TokenCase<lexer::TokenType::kKwfloat, nodes::KeywordFloat, ecs::TokenStart, ecs::TokenStop>
  >(cur_token_, "expected type");
  // clang-format on

  bool success = StoreCheckErr(result->Children()[0], std::move(child));
  result->Healthy() &= success;

  NextToken();

  return result;
}

nodes::NodesVariant Parser::ParseName() {
  auto result = std::make_unique<nodes::Name>();

  // clang-format off
  auto child = TokenToNode<
    TokenCase<lexer::TokenType::kIdToken, nodes::Name, ecs::TokenStart, ecs::TokenStop, ecs::IdName>
  >(cur_token_, "expected id");
  // clang-format on

  bool success = StoreCheckErr(result->Children()[0], std::move(child));
  result->Healthy() &= success;

  NextToken();

  return result;
}

nodes::NodesVariant Parser::ParseParameterList() {
  auto result = std::make_unique<nodes::ParameterList>();

  bool success =
      Consume<lexer::TokenType::kSyntLparent>() and
      (Lookup<lexer::TokenType::kSyntRparent>()
           ? true
           : (PushCheckErr(result->Children(), ParseParameterDecl()) or
              SkipUntil<lexer::TokenType::kSyntRparent,
                        lexer::TokenType::kSyntComma>()));

  while (success && Lookup<lexer::TokenType::kSyntComma>()) {
    success &= Consume<lexer::TokenType::kSyntComma>() and
               (PushCheckErr(result->Children(), ParseParameterDecl()) or
                SkipUntil<lexer::TokenType::kSyntRparent,
                          lexer::TokenType::kSyntComma>());
  }

  success &= Consume<lexer::TokenType::kSyntRparent>();

  result->Healthy() &= success;

  return result;
}

nodes::NodesVariant Parser::ParseParameterDecl() {
  auto result = std::make_unique<nodes::ParameterDecl>();

  bool success =
      StoreCheckErr(result->Children()[0], ParseTypeDeclaration()) and
      StoreCheckErr(result->Children()[1], ParseName());

  result->Healthy() &= success;

  return result;
}

nodes::NodesVariant Parser::ParseBlockStatement() {
  bool success = Consume<lexer::TokenType::kSyntLbrace>();
  std::vector<nodes::NodesVariant> stmts;

  while (success && !Lookup<lexer::TokenType::kSyntRbrace>()) {
    success &= PushCheckErr(stmts, ParseStatement()) or
               SkipUntil<lexer::TokenType::kSyntRbrace>();
  }

  success &= Consume<lexer::TokenType::kSyntRbrace>();

  if (stmts.size() == 1) {
    auto result = std::move(stmts.back());
    std::visit([success](auto& ptr) -> void { ptr->Healthy() &= success; },
               result);
    return result;
  }

  auto result = std::make_unique<nodes::BlockStatement>();
  result->Children() = std::move(stmts);
  result->Healthy() &= success;
  return result;
}

void Parser::TryFixStatement(nodes::NodesVariant& result) {
  bool success = IsHealthy(result);

  if (!success) {
    success = SkipUntil<lexer::TokenType::kSyntSemicolon,
                        lexer::TokenType::kSyntRbrace>() and
              Consume<lexer::TokenType::kSyntSemicolon>();
  }

  SetHealthy(result, success);
}

nodes::NodesVariant Parser::ParseStatement() {
  if (Lookup<lexer::TokenType::kKwif>()) {
    return ParseIfStatement();
  }

  if (Lookup<lexer::TokenType::kKwwhile>()) {
    return ParseWhileStatement();
  }

  if (Lookup<lexer::TokenType::kKwreturn>()) {
    auto result = ParseReturnStatement();
    TryFixStatement(result);
    return result;
  }

  if (Lookup<lexer::TokenType::kKwchar,
             lexer::TokenType::kKwint,
             lexer::TokenType::kKwuint,
             lexer::TokenType::kKwfloat>()) {

    auto result = ParseVariableDefinition();
    TryFixStatement(result);
    return result;
  }

  if (Lookup<lexer::TokenType::kIdToken>()) {
    auto result = ParseAssignmentOrCall();
    TryFixStatement(result);
    return result;
  }

  auto result = std::make_unique<nodes::ErrorNode>();
  ecs::Set<ecs::ErrorMessage>(*result, std::string("expected statement"));

  NextToken();

  return result;
}

nodes::NodesVariant Parser::ParseVariableDefinition() {
  auto result = std::make_unique<nodes::VariableDefinition>();

  bool success =
      StoreCheckErr(result->Children()[0], ParseTypeDeclaration()) and
      StoreCheckErr(result->Children()[1], ParseName()) and
      Consume<lexer::TokenType::kBinAssign>() and
      StoreCheckErr(result->Children()[2], ParseExpression()) and
      Consume<lexer::TokenType::kSyntSemicolon>();

  result->Healthy() &= success;
  return result;
}

nodes::NodesVariant Parser::ParseIfStatement() {
  auto result = std::make_unique<nodes::IfStatement>();

  bool success = Consume<lexer::TokenType::kKwif>() and
                 StoreCheckErr(result->Children()[0], ParseExpression()) and
                 StoreCheckErr(result->Children()[1], ParseBlockStatement());

  if (Lookup<lexer::TokenType::kKwelse>()) {
    success &= Consume<lexer::TokenType::kKwelse>();

    if (Lookup<lexer::TokenType::kKwif>()) {
      success &= StoreCheckErr(result->Children()[2], ParseIfStatement());
    } else {
      success &= StoreCheckErr(result->Children()[2], ParseBlockStatement());
    }
  }

  result->Healthy() &= success;
  return result;
}

nodes::NodesVariant Parser::ParseWhileStatement() {
  auto result = std::make_unique<nodes::WhileStatement>();

  bool success = Consume<lexer::TokenType::kKwwhile>() and
                 StoreCheckErr(result->Children()[0], ParseExpression()) and
                 StoreCheckErr(result->Children()[1], ParseBlockStatement());

  result->Healthy() &= success;
  return result;
}

nodes::NodesVariant Parser::ParseReturnStatement() {
  auto result = std::make_unique<nodes::ReturnStatement>();

  bool success = Consume<lexer::TokenType::kKwreturn>();

  if (!Lookup<lexer::TokenType::kSyntSemicolon>()) {
    success &= StoreCheckErr(result->Children()[0], ParseExpression());
  }

  success &= Consume<lexer::TokenType::kSyntSemicolon>();

  result->Healthy() &= success;
  return result;
}

nodes::NodesVariant Parser::ParseAssignmentOrCall() {
  auto locator = ParseLocator();

  if (Lookup<lexer::TokenType::kSyntLparent>()) {
    return ParseCallStatement(std::move(locator));
  }

  return ParseAssignment(std::move(locator));
}

auto Parser::ParseAssignment(nodes::NodesVariant&& locator)
    -> nodes::NodesVariant {
  auto result = std::make_unique<nodes::Assignment>();

  bool success = StoreCheckErr(result->Children()[0], std::move(locator)) and
                 Consume<lexer::TokenType::kBinAssign>() and
                 StoreCheckErr(result->Children()[1], ParseExpression()) and
                 Consume<lexer::TokenType::kSyntSemicolon>();

  result->Healthy() &= success;
  return result;
}

auto Parser::ParseCallStatement(nodes::NodesVariant&& locator)
    -> nodes::NodesVariant {
  auto result = std::make_unique<nodes::CallStatement>();

  bool success = StoreCheckErr(result->Children()[0], std::move(locator)) and
                 StoreCheckErr(result->Children()[1], ParseCallSuffix()) and
                 Consume<lexer::TokenType::kSyntSemicolon>();

  result->Healthy() &= success;
  return result;
}

nodes::NodesVariant Parser::ParseLocator() {
  auto result = std::make_unique<nodes::Locator>();

  // clang-format off
  auto child = TokenToNode<
    TokenCase<lexer::TokenType::kIdToken, nodes::Name, ecs::TokenStart, ecs::TokenStop, ecs::IdName>
  >(cur_token_, "expected id");
  // clang-format on

  bool success = StoreCheckErr(result->Children()[0], std::move(child));

  result->Healthy() &= success;

  NextToken();

  return result;
}

nodes::NodesVariant Parser::ParseExpression() { return ParseLogExpr(); }

nodes::NodesVariant Parser::ParseLogExpr() {
  // clang-format off
  auto node_creator = &TokenToNode<
    TokenCase<lexer::TokenType::kBinLogicalOr, nodes::LogOr, ecs::TokenStart, ecs::TokenStop>,
    TokenCase<lexer::TokenType::kBinLogicalAnd, nodes::LogAnd, ecs::TokenStart, ecs::TokenStop>
  >;
  // clang-format on

  auto last_node = ParseComparExpr();
  bool success = IsHealthy(last_node);

  while (success && Lookup<lexer::TokenType::kBinLogicalAnd,
                           lexer::TokenType::kBinLogicalOr>()) {
    auto tmp = node_creator(cur_token_, "expected && or ||");
    StoreChild(tmp, std::move(last_node), 0);
    last_node = std::move(tmp);
    NextToken();

    success &= StoreChild(last_node, ParseComparExpr(), 1);
  }

  SetHealthy(last_node, success);
  return last_node;
}

nodes::NodesVariant Parser::ParseComparExpr() {
  // clang-format off
  auto node_creator = &TokenToNode<
    TokenCase<lexer::TokenType::kBinEq, nodes::BinEq, ecs::TokenStart, ecs::TokenStop>,
    TokenCase<lexer::TokenType::kBinNeq, nodes::BinNeq, ecs::TokenStart, ecs::TokenStop>,
    TokenCase<lexer::TokenType::kBinLt, nodes::BinLt, ecs::TokenStart, ecs::TokenStop>,
    TokenCase<lexer::TokenType::kBinLeq, nodes::BinLeq, ecs::TokenStart, ecs::TokenStop>,
    TokenCase<lexer::TokenType::kBinGt, nodes::BinGt, ecs::TokenStart, ecs::TokenStop>,
    TokenCase<lexer::TokenType::kBinGeq, nodes::BinGeq, ecs::TokenStart, ecs::TokenStop>
  >;
  // clang-format on

  auto last_node = ParseAddExpr();
  bool success = IsHealthy(last_node);

  while (success && Lookup<lexer::TokenType::kBinEq,
                           lexer::TokenType::kBinNeq,
                           lexer::TokenType::kBinLt,
                           lexer::TokenType::kBinLeq,
                           lexer::TokenType::kBinGt,
                           lexer::TokenType::kBinGeq>()) {
    auto tmp = node_creator(cur_token_, "expected comparison operator");
    StoreChild(tmp, std::move(last_node), 0);
    last_node = std::move(tmp);
    NextToken();

    success &= StoreChild(last_node, ParseAddExpr(), 1);
  }

  SetHealthy(last_node, success);
  return last_node;
}

nodes::NodesVariant Parser::ParseAddExpr() {
  // clang-format off
  auto node_creator = &TokenToNode<
    TokenCase<lexer::TokenType::kBinPlus, nodes::BinPlus, ecs::TokenStart, ecs::TokenStop>,
    TokenCase<lexer::TokenType::kBinMinus, nodes::BinMinus, ecs::TokenStart, ecs::TokenStop>
  >;
  // clang-format on

  auto last_node = ParseMulExpr();
  bool success = IsHealthy(last_node);

  while (
      success &&
      Lookup<lexer::TokenType::kBinPlus, lexer::TokenType::kBinMinus>()) {
    auto tmp = node_creator(cur_token_, "expected + or -");
    StoreChild(tmp, std::move(last_node), 0);
    last_node = std::move(tmp);
    NextToken();

    success &= StoreChild(last_node, ParseMulExpr(), 1);
  }

  SetHealthy(last_node, success);
  return last_node;
}

nodes::NodesVariant Parser::ParseMulExpr() {
  // clang-format off
  auto node_creator = &TokenToNode<
    TokenCase<lexer::TokenType::kBinMul, nodes::BinMul, ecs::TokenStart, ecs::TokenStop>,
    TokenCase<lexer::TokenType::kBinDiv, nodes::BinDiv, ecs::TokenStart, ecs::TokenStop>,
    TokenCase<lexer::TokenType::kBinMod, nodes::BinMod, ecs::TokenStart, ecs::TokenStop>
  >;
  // clang-format on

  auto last_node = ParseUnaryExpr();
  bool success = IsHealthy(last_node);

  while (success && Lookup<lexer::TokenType::kBinMul,
                           lexer::TokenType::kBinDiv,
                           lexer::TokenType::kBinMod>()) {
    auto tmp = node_creator(cur_token_, "expected *, / or %");
    StoreChild(tmp, std::move(last_node), 0);
    last_node = std::move(tmp);
    NextToken();

    success &= StoreChild(last_node, ParseUnaryExpr(), 1);
  }

  SetHealthy(last_node, success);
  return last_node;
}

nodes::NodesVariant Parser::ParseUnaryExpr() {
  if (Lookup<lexer::TokenType::kUnLogicalNot>()) {
    return ParseLogNotExpr();
  }

  return ParsePrimaryExpr();
}

nodes::NodesVariant Parser::ParseLogNotExpr() {
  auto result = std::make_unique<nodes::LogNotExpr>();

  bool success = Consume<lexer::TokenType::kUnLogicalNot>() and
                 StoreCheckErr(result->Children()[0], ParsePrimaryExpr());

  result->Healthy() &= success;
  return result;
}

nodes::NodesVariant Parser::ParsePrimaryExpr() {
  if (Lookup<lexer::TokenType::kSyntLparent>()) {
    Consume<lexer::TokenType::kSyntLparent>();

    auto result = ParseExpression();

    bool success = IsHealthy(result);

    if (success) {
      success &= Consume<lexer::TokenType::kSyntRparent>();
    }

    if (!success) {
      success = SkipUntil<lexer::TokenType::kSyntRparent,
                          lexer::TokenType::kSyntSemicolon,
                          lexer::TokenType::kSyntRbrace>() and
                Consume<lexer::TokenType::kSyntRparent>();
    }

    SetHealthy(result, success);

    return result;
  }

  if (Lookup<lexer::TokenType::kIdToken>()) {
    return ParseLocatorOrCall();
  }

  return ParseLiteral();
}

nodes::NodesVariant Parser::ParseLocatorOrCall() {
  auto locator = ParseLocator();

  if (!Lookup<lexer::TokenType::kSyntLparent>()) {
    return locator;
  }

  auto result = std::make_unique<nodes::CallExpression>();

  bool success = StoreCheckErr(result->Children()[0], std::move(locator)) and
                 StoreCheckErr(result->Children()[1], ParseCallSuffix());

  result->Healthy() &= success;
  return result;
}

nodes::NodesVariant Parser::ParseCallSuffix() {
  auto result = std::make_unique<nodes::CallSuffix>();
  bool success = Consume<lexer::TokenType::kSyntLparent>() and
                 (Lookup<lexer::TokenType::kSyntRparent>()
                      ? true
                      : (PushCheckErr(result->Children(), ParseExpression()) or
                         SkipUntil<lexer::TokenType::kSyntRparent,
                                   lexer::TokenType::kSyntComma>()));

  while (success && Lookup<lexer::TokenType::kSyntComma>()) {
    success &= Consume<lexer::TokenType::kSyntComma>() and
               (PushCheckErr(result->Children(), ParseExpression()) or
                SkipUntil<lexer::TokenType::kSyntRparent,
                          lexer::TokenType::kSyntComma>());
  }

  success &= Consume<lexer::TokenType::kSyntRparent>();

  result->Healthy() &= success;
  return result;
}

nodes::NodesVariant Parser::ParseLiteral() {
  // clang-format off
  auto res = TokenToNode<
    TokenCase<lexer::TokenType::kIntLiteral, nodes::IntLiteral, ecs::IntValue>,
    TokenCase<lexer::TokenType::kFloatLiteral, nodes::FloatLiteral, ecs::FloatValue>,
    TokenCase<lexer::TokenType::kStringLiteral, nodes::StrLiteral, ecs::StrValue>
  >(cur_token_, "expected literal");
  // clang-format on

  if (IsHealthy(res)) {
    NextToken();
  }

  return res;
}

void Parser::NextToken() {
  if (next_token_index_ == input_.size()) {
    return;
  }

  cur_token_ = input_[next_token_index_++];
}

template <lexer::TokenType... tokens>
bool Parser::SkipUntil() {
  while (true) {
    if (Lookup<tokens...>()) {
      return true;
    }

    if (Lookup<lexer::TokenType::kEofToken>()) {
      return false;
    }

    NextToken();
  }
  return true;
}

// TODO error report (probably just print, no error tokens)
template <lexer::TokenType... tokens>
bool Parser::Consume() {
  if (Lookup<tokens...>()) {
    NextToken();
    return true;
  }

  return false;
}

template <lexer::TokenType... tokens>
bool Parser::Lookup() {
  return ((cur_token_.GetType() == tokens) || ...);
}

//===========================^=PARSER=^=============================================================

}  // namespace

nodes::NodesVariant Parse(std::vector<lexer::Token> input) {
  Parser parser(std::move(input));
  return parser.Parse();
}

}  // namespace parser

// NOLINTEND(misc-no-recursion)
