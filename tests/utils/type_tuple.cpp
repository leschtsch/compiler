#include <utils/type_tuple.hpp>

#include <gtest/gtest.h>
#include <sys/stat.h>
#include <tuple>
#include <type_traits>
#include <variant>

// clang-format off

TEST(TypeTuple, Concat) {
  static_assert(
    std::is_same_v<
      utils::TypeTuple<>
    , utils::Concat<
        utils::TypeTuple<>
      , utils::TypeTuple<>
      >
    >
  );
  static_assert(
    std::is_same_v<
      utils::TypeTuple<int, char, bool, float>
    , utils::Concat<
        utils::TypeTuple<>
      , utils::TypeTuple<int, char, bool, float>
      >
    >
  );
  static_assert(
    std::is_same_v<
      utils::TypeTuple<int, char, bool, float>
    , utils::Concat<
        utils::TypeTuple<int, char, bool, float>
      , utils::TypeTuple<>
      >
    >
  );
  static_assert(
    std::is_same_v<
      utils::TypeTuple<int, char, bool, float>
    , utils::Concat<
        utils::TypeTuple<int, char>
      , utils::TypeTuple<bool, float>
      >
    >
  );

  static_assert(
    std::is_same_v<
      utils::TypeTuple<>
    , utils::Concat<>
    >
  );
  static_assert(
    std::is_same_v<
      utils::TypeTuple<int, char>
    , utils::Concat<
        utils::TypeTuple<int, char>
      >
    >
  );

  static_assert(
    std::is_same_v<
      utils::TypeTuple<int, char, bool, float>
    , utils::Concat<
        utils::TypeTuple<int>
      , utils::TypeTuple<char, bool>
      , utils::TypeTuple<float>
      >
    >
  );
}

TEST(TypeTuple, ConvertTo) {
  static_assert(
    std::is_same_v<
      std::tuple<int, char, bool>
    , utils::ConvertTo<
        utils::TypeTuple<int, char, bool>
      , std::tuple  
      >
    >
  );
  static_assert(
    std::is_same_v<
      std::variant<int, char, bool>
    , utils::ConvertTo<
        utils::TypeTuple<int, char, bool>
      , std::variant
      >
    >
  );
}

// clang-format on

int main() {
  testing::InitGoogleTest();
  return RUN_ALL_TESTS();
}
