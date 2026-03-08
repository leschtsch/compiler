#include <ecs/ecs.hpp>
#include <ecs/used_components.hpp>
#include <lexer/lexer.hpp>
#include <lexer/tokens.hpp>

#include <gtest/gtest.h>
#include <variant>

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
  ASSERT_TRUE(std::holds_alternative<lexer::tokens::String>(tok));
  ecs::Entity entity = std::get<lexer::tokens::String>(tok);
  ASSERT_EQ(ecs::Get<lexer::StrValue>(entity).Value(), "\a\b\e\f\n\r\t\v\\\"ab \xff");
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
  ASSERT_TRUE(std::holds_alternative<lexer::tokens::String>(tok));
  ecs::Entity entity = std::get<lexer::tokens::String>(tok);
  ASSERT_EQ(ecs::Get<lexer::StrValue>(entity).Value(), "ab//o/*b*/a");
  EXPECT_TRUE(
      std::holds_alternative<lexer::tokens::EofToken>(cur_lexer.NextToken()));
}

int main() {
  testing::InitGoogleTest();
  return RUN_ALL_TESTS();
}
