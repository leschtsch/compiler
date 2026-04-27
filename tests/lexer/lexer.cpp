#include <ecs/ecs.hpp>
#include <ecs/used_components.hpp>
#include <lexer/lexer.hpp>
#include <lexer/tokens.hpp>

#include <cmath>
#include <gtest/gtest.h>

TEST(Empty, Empty) {
  std::string input;
  lexer::Lexer cur_lexer{input};
  EXPECT_EQ(lexer::TokenType::kEofToken, cur_lexer.NextToken().GetType());
}

TEST(Empty, Spaces) {
  std::string input = " \t\n ";
  lexer::Lexer cur_lexer{input};
  EXPECT_EQ(lexer::TokenType::kEofToken, cur_lexer.NextToken().GetType());
}

TEST(Comments, Ok) {
  std::string input = "//abob/*abiba\n /*a//b/*o*/b//a*/";
  lexer::Lexer cur_lexer{input};
  EXPECT_EQ(lexer::TokenType::kEofToken, cur_lexer.NextToken().GetType());
}

TEST(Comments, Error) {
  std::string input = "/* /* */";
  lexer::Lexer cur_lexer{input};
  EXPECT_EQ(lexer::TokenType::kErrorToken, cur_lexer.NextToken().GetType());
}

TEST(Strings, Value) {
  std::string input = R"("\a\b\e\f\n\r\t\v\\\"ab \xff")";
  lexer::Lexer cur_lexer{input};
  auto tok = cur_lexer.NextToken();
  EXPECT_EQ(lexer::TokenType::kStringLiteral, tok.GetType());
  EXPECT_EQ(ecs::Get<ecs::StrValue>(tok).Value(),
            "\a\b\e\f\n\r\t\v\\\"ab \xff");
}

TEST(Strings, Errors) {
  std::string input = R"("\xgg \y)";
  lexer::Lexer cur_lexer{input};
  EXPECT_EQ(lexer::TokenType::kErrorToken, cur_lexer.NextToken().GetType());
  EXPECT_EQ(lexer::TokenType::kErrorToken, cur_lexer.NextToken().GetType());
  EXPECT_EQ(lexer::TokenType::kErrorToken, cur_lexer.NextToken().GetType());
}

TEST(Strings, Errors2) {
  std::string input = R"("\)";
  lexer::Lexer cur_lexer{input};
  EXPECT_EQ(lexer::TokenType::kErrorToken, cur_lexer.NextToken().GetType());
}

TEST(Strings, Errors3) {
  std::string input = R"("\x1)";
  lexer::Lexer cur_lexer{input};
  EXPECT_EQ(lexer::TokenType::kErrorToken, cur_lexer.NextToken().GetType());
}

TEST(Strings, WithComments) {
  std::string input = R"("ab//o/*b*/a")";
  lexer::Lexer cur_lexer{input};
  auto tok = cur_lexer.NextToken();
  EXPECT_EQ(lexer::TokenType::kStringLiteral, tok.GetType());
  EXPECT_EQ(ecs::Get<ecs::StrValue>(tok).Value(), "ab//o/*b*/a");
  EXPECT_EQ(lexer::TokenType::kEofToken, cur_lexer.NextToken().GetType());
}

TEST(IdsKws, Id) {
  std::string input = "variable";
  lexer::Lexer cur_lexer{input};
  auto tok = cur_lexer.NextToken();
  EXPECT_EQ(lexer::TokenType::kIdToken, tok.GetType());
  EXPECT_EQ(ecs::Get<ecs::IdName>(tok).Value(), "variable");
  EXPECT_EQ(lexer::TokenType::kEofToken, cur_lexer.NextToken().GetType());
}

TEST(IdsKws, Kw) {
  std::string input = "for";
  lexer::Lexer cur_lexer{input};
  EXPECT_EQ(lexer::TokenType::kKwfor, cur_lexer.NextToken().GetType());
  EXPECT_EQ(lexer::TokenType::kEofToken, cur_lexer.NextToken().GetType());
}

TEST(IdsKws, Misc) {
  std::string input = "for /* aboba*/ variable";
  lexer::Lexer cur_lexer{input};

  EXPECT_EQ(lexer::TokenType::kKwfor, cur_lexer.NextToken().GetType());
  EXPECT_EQ(lexer::TokenType::kIdToken, cur_lexer.NextToken().GetType());
  EXPECT_EQ(lexer::TokenType::kEofToken, cur_lexer.NextToken().GetType());
}

TEST(OtherTokens, test1) {
  std::string input = "int z = x + -y;";
  lexer::Lexer cur_lexer{input};

  auto tok = cur_lexer.NextToken();
  EXPECT_EQ(lexer::TokenType::kKwint, tok.GetType());

  tok = cur_lexer.NextToken();
  EXPECT_EQ(lexer::TokenType::kIdToken, tok.GetType());
  EXPECT_EQ(ecs::Get<ecs::IdName>(tok).Value(), "z");

  tok = cur_lexer.NextToken();
  EXPECT_EQ(lexer::TokenType::kBinAssign, tok.GetType());

  tok = cur_lexer.NextToken();
  EXPECT_EQ(lexer::TokenType::kIdToken, tok.GetType());
  EXPECT_EQ(ecs::Get<ecs::IdName>(tok).Value(), "x");

  tok = cur_lexer.NextToken();
  EXPECT_EQ(lexer::TokenType::kBinPlus, tok.GetType());

  tok = cur_lexer.NextToken();
  EXPECT_EQ(lexer::TokenType::kBinMinus, tok.GetType());

  tok = cur_lexer.NextToken();
  EXPECT_EQ(lexer::TokenType::kIdToken, tok.GetType());
  EXPECT_EQ(ecs::Get<ecs::IdName>(tok).Value(), "y");

  tok = cur_lexer.NextToken();
  EXPECT_EQ(lexer::TokenType::kSyntSemicolon, tok.GetType());

  tok = cur_lexer.NextToken();
  EXPECT_EQ(lexer::TokenType::kEofToken, tok.GetType());
}

TEST(OtherTokens, Long) {
  std::string input = ">>=!=";
  lexer::Lexer cur_lexer{input};

  auto tok = cur_lexer.NextToken();
  EXPECT_EQ(lexer::TokenType::kBinRshiftAssign, tok.GetType());
  tok = cur_lexer.NextToken();
  EXPECT_EQ(lexer::TokenType::kBinNeq, tok.GetType());
  tok = cur_lexer.NextToken();
  EXPECT_EQ(lexer::TokenType::kEofToken, tok.GetType());
}

TEST(Nums, Int) {
  std::string input = " 123 ";
  lexer::Lexer cur_lexer{input};

  auto tok = cur_lexer.NextToken();
  EXPECT_EQ(lexer::TokenType::kIntLiteral, tok.GetType());
  EXPECT_EQ(ecs::Get<ecs::IntValue>(tok).Value(), 123);
  tok = cur_lexer.NextToken();
  EXPECT_EQ(lexer::TokenType::kEofToken, tok.GetType());
}

TEST(Nums, Float) {
  std::string input = " 0x1.2p-3 ";
  lexer::Lexer cur_lexer{input};

  auto tok = cur_lexer.NextToken();
  EXPECT_EQ(lexer::TokenType::kFloatLiteral, tok.GetType());

  EXPECT_TRUE(std::abs(ecs::Get<ecs::FloatValue>(tok).Value() - 0x1.2p-3) <
              1e-10);

  tok = cur_lexer.NextToken();
  EXPECT_EQ(lexer::TokenType::kEofToken, tok.GetType());
}

TEST(Nums, StartsWithDot) {
  std::string input = " .1 ";
  lexer::Lexer cur_lexer{input};

  auto tok = cur_lexer.NextToken();
  EXPECT_EQ(lexer::TokenType::kFloatLiteral, tok.GetType());

  EXPECT_TRUE(std::abs(ecs::Get<ecs::FloatValue>(tok).Value() - .1) < 1e-10);

  tok = cur_lexer.NextToken();
  EXPECT_EQ(lexer::TokenType::kEofToken, tok.GetType());
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
  EXPECT_EQ(lexer::TokenType::kKwint, tok.GetType());
  tok = cur_lexer.NextToken();
  EXPECT_EQ(lexer::TokenType::kIdToken, tok.GetType());
  EXPECT_EQ(ecs::Get<ecs::IdName>(tok).Value(), "main");
  tok = cur_lexer.NextToken();
  EXPECT_EQ(lexer::TokenType::kSyntLparent, tok.GetType());
  tok = cur_lexer.NextToken();
  EXPECT_EQ(lexer::TokenType::kSyntRparent, tok.GetType());
  tok = cur_lexer.NextToken();
  EXPECT_EQ(lexer::TokenType::kSyntLbrace, tok.GetType());

  tok = cur_lexer.NextToken();
  EXPECT_EQ(lexer::TokenType::kIdToken, tok.GetType());
  EXPECT_EQ(ecs::Get<ecs::IdName>(tok).Value(), "puts");
  tok = cur_lexer.NextToken();
  EXPECT_EQ(lexer::TokenType::kSyntLparent, tok.GetType());
  tok = cur_lexer.NextToken();
  EXPECT_EQ(lexer::TokenType::kStringLiteral, tok.GetType());
  EXPECT_EQ(ecs::Get<ecs::StrValue>(tok).Value(), "hello world");
  tok = cur_lexer.NextToken();
  EXPECT_EQ(lexer::TokenType::kSyntRparent, tok.GetType());
  tok = cur_lexer.NextToken();
  EXPECT_EQ(lexer::TokenType::kSyntSemicolon, tok.GetType());

  tok = cur_lexer.NextToken();
  EXPECT_EQ(lexer::TokenType::kIdToken, tok.GetType());
  EXPECT_EQ(ecs::Get<ecs::IdName>(tok).Value(), "printf");
  tok = cur_lexer.NextToken();
  EXPECT_EQ(lexer::TokenType::kSyntLparent, tok.GetType());
  tok = cur_lexer.NextToken();
  EXPECT_EQ(lexer::TokenType::kStringLiteral, tok.GetType());
  EXPECT_EQ(ecs::Get<ecs::StrValue>(tok).Value(), "%d");
  tok = cur_lexer.NextToken();
  EXPECT_EQ(lexer::TokenType::kSyntComma, tok.GetType());
  tok = cur_lexer.NextToken();
  EXPECT_EQ(lexer::TokenType::kIntLiteral, tok.GetType());
  EXPECT_EQ(ecs::Get<ecs::IntValue>(tok).Value(), 1);
  tok = cur_lexer.NextToken();
  EXPECT_EQ(lexer::TokenType::kSyntRparent, tok.GetType());
  tok = cur_lexer.NextToken();
  EXPECT_EQ(lexer::TokenType::kSyntSemicolon, tok.GetType());

  tok = cur_lexer.NextToken();
  EXPECT_EQ(lexer::TokenType::kSyntRbrace, tok.GetType());
  tok = cur_lexer.NextToken();
  EXPECT_EQ(lexer::TokenType::kEofToken, tok.GetType());
}

auto main() -> int {
  testing::InitGoogleTest();
  return RUN_ALL_TESTS();
}
