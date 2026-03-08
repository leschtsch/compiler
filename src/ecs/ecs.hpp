/**
 * @file ecs.hpp
 *
 * This is implementation of Entity-Component system.
 * You may think of this as of simple  in-memory database.
 */

#pragma once

#include <concepts>
#include <cstdint>
#include <optional>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace ecs {

namespace details {

using IdT = std::uint64_t;

inline IdT GetID() {
  static IdT next_id = 0;
  return ++next_id;
};

template <typename T>
concept BareType =
    !std::is_reference_v<T> && !std::is_const_v<T> && !std::is_volatile_v<T>;

}  // namespace details

struct Entity {
  details::IdT id{details::GetID()};
};

template <details::BareType Type>
struct Component {};

namespace details {

template <typename T>
constexpr T GetTypeHelper(Component<T>);

template <typename T>
using GetTypeT = decltype(GetTypeHelper(std::declval<T>()));
}  // namespace details

template <typename T>
concept IsComponent = std::derived_from<T, Component<details::GetTypeT<T>>>;

namespace details {

template <typename T>
struct OptionalReference {
 public:
  explicit OptionalReference(T* val) : data_(val) {}
  ~OptionalReference() = default;

  // non-copyable
  OptionalReference(const OptionalReference& other) = delete;
  OptionalReference& operator=(const OptionalReference& other) = delete;

  // non-movable
  OptionalReference(OptionalReference&& other) = delete;
  OptionalReference& operator=(OptionalReference&& other) = delete;

  T* operator->() noexcept { return data_; }
  const T* operator->() const noexcept { return data_; }

  [[nodiscard]] T& operator*() noexcept { *data_; }
  [[nodiscard]] const T& operator*() const noexcept { *data_; }

  [[nodiscard]] bool HasValue() const noexcept { return data_ != nullptr; }

  [[nodiscard]] T& Value() {
    if (!HasValue()) {
      throw std::bad_optional_access{};
    }

    return (*data_);
  }

  [[nodiscard]] const T& Value() const {
    if (!HasValue()) {
      throw std::bad_optional_access{};
    }

    return (*data_);
  }

 private:
  T* data_;
};

template <typename T, typename C>
concept MatchesComponent =
    IsComponent<C> &&
    std::same_as<GetTypeT<C>, std::remove_reference_t<std::remove_cv_t<T>>>;

template <IsComponent C>
class EcsHelper {
 private:
  using Type = GetTypeT<C>;

 public:
  template <MatchesComponent<C> T>
  static void Set(const Entity& entity, T&& component) {
    storage.insert_or_assign(entity.id, std::forward<T>(component));
  }

  static void Drop(const Entity& entity) { storage.erase(entity.id); }

  static auto Get(const Entity& entity) -> OptionalReference<Type> {
    auto found = storage.find(entity.id);

    if (found == storage.end()) {
      return OptionalReference<Type>(nullptr);
    }

    return OptionalReference<Type>(&found->second);
  }

 private:
  static std::unordered_map<IdT, Type> storage;
};
}  // namespace details

template <IsComponent C, details::MatchesComponent<C> T>
void Set(const Entity& entity, T&& component) {
  details::EcsHelper<C>::Set(entity, std::forward<T>(component));
}

template <IsComponent C>
void Drop(const Entity& entity) {
  details::EcsHelper<C>::Drop(entity);
}

template <IsComponent C>
auto Get(const Entity& entity) {
  return details::EcsHelper<C>::Get(entity);
}

template <IsComponent C>
std::unordered_map<details::IdT, typename details::EcsHelper<C>::Type>
    details::EcsHelper<C>::storage = {};

}  // namespace ecs
