#include <ecs/ecs.hpp>
#include <ecs/used_components.hpp>
#include <lexer/lexer.hpp>
#include <lexer/tokens.hpp>

#include <cmath>
#include <gtest/gtest.h>
#include <variant>

#include "gtest/gtest.h"

TEST(Empty, Empty) {
  std::string input;
  lexer::Lexer cur_lexer{input};
  EXPECT_TRUE(
      std::holds_alternative<lexer::tokens::EofToken>(cur_lexer.NextToken()));
}

TEST(Empty, Spaces) {
  std::string input = " \t\n ";
  lexer::Lexer cur_lexer{input};
  EXPECT_TRUE(
      std::holds_alternative<lexer::tokens::EofToken>(cur_lexer.NextToken()));
}

TEST(Comments, Ok) {
  std::string input = "//abob/*abiba\n /*a//b/*o*/b//a*/";
  lexer::Lexer cur_lexer{input};
  EXPECT_TRUE(
      std::holds_alternative<lexer::tokens::EofToken>(cur_lexer.NextToken()));
}

TEST(Comments, Error) {
  std::string input = "/* /* */";
  lexer::Lexer cur_lexer{input};
  EXPECT_TRUE(
      std::holds_alternative<lexer::tokens::ErrorToken>(cur_lexer.NextToken()));
}

TEST(Strings, Value) {
  std::string input = R"("\a\b\e\f\n\r\t\v\\\"ab \xff")";
  lexer::Lexer cur_lexer{input};
  auto tok = cur_lexer.NextToken();
  EXPECT_TRUE(std::holds_alternative<lexer::tokens::String>(tok));
  ecs::Entity entity = std::get<lexer::tokens::String>(tok);
  EXPECT_EQ(ecs::Get<lexer::StrValue>(entity).Value(),
            "\a\b\e\f\n\r\t\v\\\"ab \xff");
}

TEST(Strings, Errors) {
  std::string input = R"("\xgg \y)";
  lexer::Lexer cur_lexer{input};
  EXPECT_TRUE(
      std::holds_alternative<lexer::tokens::ErrorToken>(cur_lexer.NextToken()));
  EXPECT_TRUE(
      std::holds_alternative<lexer::tokens::ErrorToken>(cur_lexer.NextToken()));
  EXPECT_TRUE(
      std::holds_alternative<lexer::tokens::ErrorToken>(cur_lexer.NextToken()));
}

TEST(Strings, Errors2) {
  std::string input = R"("\)";
  lexer::Lexer cur_lexer{input};
  EXPECT_TRUE(
      std::holds_alternative<lexer::tokens::ErrorToken>(cur_lexer.NextToken()));
}

TEST(Strings, Errors3) {
  std::string input = R"("\x1)";
  lexer::Lexer cur_lexer{input};
  EXPECT_TRUE(
      std::holds_alternative<lexer::tokens::ErrorToken>(cur_lexer.NextToken()));
}

TEST(Strings, WithComments) {
  std::string input = R"("ab//o/*b*/a")";
  lexer::Lexer cur_lexer{input};
  auto tok = cur_lexer.NextToken();
  EXPECT_TRUE(std::holds_alternative<lexer::tokens::String>(tok));
  ecs::Entity entity = std::get<lexer::tokens::String>(tok);
  EXPECT_EQ(ecs::Get<lexer::StrValue>(entity).Value(), "ab//o/*b*/a");
  EXPECT_TRUE(
      std::holds_alternative<lexer::tokens::EofToken>(cur_lexer.NextToken()));
}

TEST(IdsKws, Id) {
  std::string input = "variable";
  lexer::Lexer cur_lexer{input};
  auto tok = cur_lexer.NextToken();
  EXPECT_TRUE(std::holds_alternative<lexer::tokens::IdToken>(tok));
  ecs::Entity entity = std::get<lexer::tokens::IdToken>(tok);
  EXPECT_EQ(ecs::Get<lexer::IdName>(entity).Value(), "variable");
  EXPECT_TRUE(
      std::holds_alternative<lexer::tokens::EofToken>(cur_lexer.NextToken()));
}

TEST(IdsKws, Kw) {
  std::string input = "for";
  lexer::Lexer cur_lexer{input};
  EXPECT_TRUE(std::holds_alternative<lexer::tokens::Keyword_for>(
      cur_lexer.NextToken()));
  EXPECT_TRUE(
      std::holds_alternative<lexer::tokens::EofToken>(cur_lexer.NextToken()));
}

TEST(IdsKws, Misc) {
  std::string input = "for /* aboba*/ variable";
  lexer::Lexer cur_lexer{input};

  EXPECT_TRUE(std::holds_alternative<lexer::tokens::Keyword_for>(
      cur_lexer.NextToken()));
  EXPECT_TRUE(
      std::holds_alternative<lexer::tokens::IdToken>(cur_lexer.NextToken()));
  EXPECT_TRUE(
      std::holds_alternative<lexer::tokens::EofToken>(cur_lexer.NextToken()));
}

TEST(OtherTokens, test1) {
  std::string input = "int z = x + -y;";
  lexer::Lexer cur_lexer{input};

  auto tok = cur_lexer.NextToken();
  EXPECT_TRUE(std::holds_alternative<lexer::tokens::Keyword_int>(tok));

  tok = cur_lexer.NextToken();
  EXPECT_TRUE(std::holds_alternative<lexer::tokens::IdToken>(tok));
  ecs::Entity entity = std::get<lexer::tokens::IdToken>(tok);
  EXPECT_EQ(ecs::Get<lexer::IdName>(entity).Value(), "z");

  tok = cur_lexer.NextToken();
  EXPECT_TRUE(std::holds_alternative<lexer::tokens::BinAssign>(tok));

  tok = cur_lexer.NextToken();
  EXPECT_TRUE(std::holds_alternative<lexer::tokens::IdToken>(tok));
  entity = std::get<lexer::tokens::IdToken>(tok);
  EXPECT_EQ(ecs::Get<lexer::IdName>(entity).Value(), "x");

  tok = cur_lexer.NextToken();
  EXPECT_TRUE(std::holds_alternative<lexer::tokens::BinPlus>(tok));

  tok = cur_lexer.NextToken();
  EXPECT_TRUE(std::holds_alternative<lexer::tokens::BinMinus>(tok));

  tok = cur_lexer.NextToken();
  EXPECT_TRUE(std::holds_alternative<lexer::tokens::IdToken>(tok));
  entity = std::get<lexer::tokens::IdToken>(tok);
  EXPECT_EQ(ecs::Get<lexer::IdName>(entity).Value(), "y");

  tok = cur_lexer.NextToken();
  EXPECT_TRUE(std::holds_alternative<lexer::tokens::SyntaxSemicolon>(tok));

  tok = cur_lexer.NextToken();
  EXPECT_TRUE(std::holds_alternative<lexer::tokens::EofToken>(tok));
}

TEST(OtherTokens, Long) {
  std::string input = ">>=!=";
  lexer::Lexer cur_lexer{input};

  auto tok = cur_lexer.NextToken();
  EXPECT_TRUE(std::holds_alternative<lexer::tokens::BinRshiftAssign>(tok));
  tok = cur_lexer.NextToken();
  EXPECT_TRUE(std::holds_alternative<lexer::tokens::BinNeq>(tok));
  tok = cur_lexer.NextToken();
  EXPECT_TRUE(std::holds_alternative<lexer::tokens::EofToken>(tok));
}

TEST(Nums, Int) {
  std::string input = " 123 ";
  lexer::Lexer cur_lexer{input};

  auto tok = cur_lexer.NextToken();
  EXPECT_TRUE(std::holds_alternative<lexer::tokens::Int>(tok));
  auto entity = std::get<lexer::tokens::Int>(tok);
  EXPECT_EQ(ecs::Get<lexer::IntValue>(entity).Value(), 123);
  tok = cur_lexer.NextToken();
  EXPECT_TRUE(std::holds_alternative<lexer::tokens::EofToken>(tok));
}

TEST(Nums, Float) {
  std::string input = " 0x1.2p-3 ";
  lexer::Lexer cur_lexer{input};

  auto tok = cur_lexer.NextToken();
  EXPECT_TRUE(std::holds_alternative<lexer::tokens::Float>(tok));

  auto entity = std::get<lexer::tokens::Float>(tok);
  EXPECT_TRUE(std::abs(ecs::Get<lexer::FloatValue>(entity).Value() - 0x1.2p-3) <
              1e-10);

  tok = cur_lexer.NextToken();
  EXPECT_TRUE(std::holds_alternative<lexer::tokens::EofToken>(tok));
}

TEST(Program, Something) {
  std::string input = R"(
    int main() {
      puts("hello world");
      printf("%d", 1);
    }
  )";
  lexer::Lexer cur_lexer{input};

  auto tok = cur_lexer.NextToken();
  EXPECT_TRUE(std::holds_alternative<lexer::tokens::Keyword_int>(tok));
  tok = cur_lexer.NextToken();
  EXPECT_TRUE(std::holds_alternative<lexer::tokens::IdToken>(tok));
  auto entity = static_cast<ecs::Entity>(std::get<lexer::tokens::IdToken>(tok));
  EXPECT_EQ(ecs::Get<lexer::IdName>(entity).Value(), "main");
  tok = cur_lexer.NextToken();
  EXPECT_TRUE(std::holds_alternative<lexer::tokens::SyntaxLparent>(tok));
  tok = cur_lexer.NextToken();
  EXPECT_TRUE(std::holds_alternative<lexer::tokens::SyntaxRparent>(tok));
  tok = cur_lexer.NextToken();
  EXPECT_TRUE(std::holds_alternative<lexer::tokens::SyntaxLbrace>(tok));

  tok = cur_lexer.NextToken();
  EXPECT_TRUE(std::holds_alternative<lexer::tokens::IdToken>(tok));
  entity = std::get<lexer::tokens::IdToken>(tok);
  EXPECT_EQ(ecs::Get<lexer::IdName>(entity).Value(), "puts");
  tok = cur_lexer.NextToken();
  EXPECT_TRUE(std::holds_alternative<lexer::tokens::SyntaxLparent>(tok));
  tok = cur_lexer.NextToken();
  EXPECT_TRUE(std::holds_alternative<lexer::tokens::String>(tok));
  entity = std::get<lexer::tokens::String>(tok);
  EXPECT_EQ(ecs::Get<lexer::StrValue>(entity).Value(), "hello world");
  tok = cur_lexer.NextToken();
  EXPECT_TRUE(std::holds_alternative<lexer::tokens::SyntaxRparent>(tok));
  tok = cur_lexer.NextToken();
  EXPECT_TRUE(std::holds_alternative<lexer::tokens::SyntaxSemicolon>(tok));

  tok = cur_lexer.NextToken();
  EXPECT_TRUE(std::holds_alternative<lexer::tokens::IdToken>(tok));
  entity = std::get<lexer::tokens::IdToken>(tok);
  EXPECT_EQ(ecs::Get<lexer::IdName>(entity).Value(), "printf");
  tok = cur_lexer.NextToken();
  EXPECT_TRUE(std::holds_alternative<lexer::tokens::SyntaxLparent>(tok));
  tok = cur_lexer.NextToken();
  EXPECT_TRUE(std::holds_alternative<lexer::tokens::String>(tok));
  entity = std::get<lexer::tokens::String>(tok);
  EXPECT_EQ(ecs::Get<lexer::StrValue>(entity).Value(), "%d");
  tok = cur_lexer.NextToken();
  EXPECT_TRUE(std::holds_alternative<lexer::tokens::SyntaxComma>(tok));
  tok = cur_lexer.NextToken();
  EXPECT_TRUE(std::holds_alternative<lexer::tokens::Int>(tok));
  entity = std::get<lexer::tokens::Int>(tok);
  EXPECT_EQ(ecs::Get<lexer::IntValue>(entity).Value(), 1);
  tok = cur_lexer.NextToken();
  EXPECT_TRUE(std::holds_alternative<lexer::tokens::SyntaxRparent>(tok));
  tok = cur_lexer.NextToken();
  EXPECT_TRUE(std::holds_alternative<lexer::tokens::SyntaxSemicolon>(tok));

  tok = cur_lexer.NextToken();
  EXPECT_TRUE(std::holds_alternative<lexer::tokens::SyntaxRbrace>(tok));
  tok = cur_lexer.NextToken();
  EXPECT_TRUE(std::holds_alternative<lexer::tokens::EofToken>(tok));
}

auto main() -> int {
  testing::InitGoogleTest();
  return RUN_ALL_TESTS();
}
