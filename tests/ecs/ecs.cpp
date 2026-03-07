#include "ecs/ecs.hpp"

#include <exception>
#include <gtest/gtest.h>

TEST(Ecs, CRUD) {
  struct C1 : public ecs::BaseComponent<int> {};
  struct C2 : public ecs::BaseComponent<int> {};

  ecs::Entity entity1 = ecs::CreateEntity();
  ecs::Entity entity2 = ecs::CreateEntity();
  EXPECT_NE(entity1.id, entity2.id);

  EXPECT_FALSE(ecs::Get<C1>(entity1).HasValue());
  EXPECT_FALSE(ecs::Get<C1>(entity2).HasValue());
  EXPECT_FALSE(ecs::Get<C2>(entity1).HasValue());
  EXPECT_FALSE(ecs::Get<C2>(entity2).HasValue());

  ecs::Set<C1>(entity1, 1);
  ecs::Set<C2>(entity2, 2);
  EXPECT_TRUE(ecs::Get<C1>(entity1).HasValue());
  EXPECT_EQ(ecs::Get<C1>(entity1).Value(), 1);
  EXPECT_FALSE(ecs::Get<C1>(entity2).HasValue());
  EXPECT_FALSE(ecs::Get<C2>(entity1).HasValue());
  EXPECT_TRUE(ecs::Get<C2>(entity2).HasValue());
  EXPECT_EQ(ecs::Get<C2>(entity2).Value(), 2);

  ecs::Get<C1>(entity1).Value() = 3;
  EXPECT_TRUE(ecs::Get<C1>(entity1).HasValue());
  EXPECT_EQ(ecs::Get<C1>(entity1).Value(), 3);
  EXPECT_FALSE(ecs::Get<C1>(entity2).HasValue());
  EXPECT_FALSE(ecs::Get<C2>(entity1).HasValue());
  EXPECT_TRUE(ecs::Get<C2>(entity2).HasValue());
  EXPECT_EQ(ecs::Get<C2>(entity2).Value(), 2);

  ecs::Drop<C2>(entity2);
  EXPECT_TRUE(ecs::Get<C1>(entity1).HasValue());
  EXPECT_EQ(ecs::Get<C1>(entity1).Value(), 3);
  EXPECT_FALSE(ecs::Get<C1>(entity2).HasValue());
  EXPECT_FALSE(ecs::Get<C2>(entity1).HasValue());
  EXPECT_FALSE(ecs::Get<C2>(entity2).HasValue());

  ecs::Drop<C1>(entity1);
  EXPECT_FALSE(ecs::Get<C1>(entity1).HasValue());
  EXPECT_FALSE(ecs::Get<C1>(entity2).HasValue());
  EXPECT_FALSE(ecs::Get<C2>(entity1).HasValue());
  EXPECT_FALSE(ecs::Get<C2>(entity2).HasValue());
}

struct Counted {
  Counted() { ++default_constr; }
  ~Counted() { ++destructor; }

  Counted(const Counted& /* other */) { ++copy_constr; }
  Counted& operator=(const Counted& /* other */) {
    ++copy_assign;
    return *this;
  }

  Counted(Counted&& /* other */) noexcept { ++move_constr; }
  Counted& operator=(Counted&& /* other */) noexcept {
    ++move_assign;
    return *this;
  }

  static std::size_t default_constr;
  static std::size_t destructor;
  static std::size_t copy_constr;
  static std::size_t copy_assign;
  static std::size_t move_constr;
  static std::size_t move_assign;
};

std::size_t Counted::default_constr = 0;
std::size_t Counted::copy_constr = 0;
std::size_t Counted::copy_assign = 0;
std::size_t Counted::move_constr = 0;
std::size_t Counted::move_assign = 0;
std::size_t Counted::destructor = 0;

TEST(Ecs, MoveSemantics) {
  struct C1 : public ecs::BaseComponent<Counted> {};
  ecs::Entity entity1 = ecs::CreateEntity();
  ecs::Entity entity2 = ecs::CreateEntity();

  Counted val1;
  Counted val2;

  EXPECT_EQ(Counted::copy_constr, 0);
  ecs::Set<C1>(entity1, val1);
  EXPECT_EQ(Counted::copy_constr, 1);
  EXPECT_EQ(Counted::move_constr, 0);
  ecs::Set<C1>(entity2, std::move(val1));
  EXPECT_EQ(Counted::move_constr, 1);

  EXPECT_EQ(Counted::copy_assign, 0);
  ecs::Set<C1>(entity1, val2);
  EXPECT_EQ(Counted::copy_assign, 1);
  EXPECT_EQ(Counted::move_assign, 0);
  ecs::Set<C1>(entity1, std::move(val2));
  EXPECT_EQ(Counted::move_assign, 1);

  EXPECT_EQ(Counted::copy_assign, 1);
  val1 = ecs::Get<C1>(entity1).Value();
  EXPECT_EQ(Counted::copy_assign, 2);
  EXPECT_EQ(Counted::move_assign, 1);
  val2 = std::move(ecs::Get<C1>(entity1).Value());
  EXPECT_EQ(Counted::move_assign, 2);
}

int main() {
  testing::InitGoogleTest();
  return RUN_ALL_TESTS();
}
