#pragma once

#include <ecs/ecs.hpp>

#include <array>
#include <cstddef>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace parser::nodes {

//===========================V=FORwARDS=V===========================================================
class BaseNode;

template <std::size_t n_children>
class NaryNode;

class ArbitraryNode;

// NOLINTNEXTLINE
#define NODE(Node, Parent) class Node;

#include <language_data/grammar_nodes.dat>

#undef NODE

#define NODE(Node, Parent) std::unique_ptr<Node>,
//===========================^=FORwARDS=^===========================================================

//===========================V=VARIANT=V============================================================
using NodesVariant = std::variant<

#include <language_data/grammar_nodes.dat>

    std::unique_ptr<BaseNode>,
    std::unique_ptr<NaryNode<1>>,
    std::unique_ptr<NaryNode<2>>,
    std::unique_ptr<NaryNode<3>>,
    std::unique_ptr<NaryNode<4>>,
    std::unique_ptr<ArbitraryNode>>;

#undef NODE
//===========================^=VARIANT=^============================================================

//===========================V=VISITING=V===========================================================
template <typename Visitor>
concept NodeVisitor =
    requires(Visitor visitor, NodesVariant& node) { visitor.operator()(node); };

template <typename Visitor, typename... Variants>
auto VisitPtr(Visitor&& visitor, Variants&... variants) {
  return std::visit(
      [&visitor](auto&... ptrs) -> auto {
        return std::invoke(std::forward<Visitor>(visitor), ptrs.get()...);
      },
      variants...);
}

template <typename Visitor>
class PtrVisitorWrapper : public Visitor {
 public:
  auto operator()(auto&... ptrs) & {
    return std::invoke(static_cast<Visitor>(*this), ptrs.get()...);
  }

  auto operator()(auto&... ptrs) const& {
    return std::invoke(static_cast<const Visitor>(*this), ptrs.get()...);
  }

  auto operator()(auto&... ptrs) && {
    return std::invoke(static_cast<Visitor&&>(*this), ptrs.get()...);
  }

  auto operator()(auto&... ptrs) const&& {
    return std::invoke(static_cast<const Visitor&&>(*this), ptrs.get()...);
  }
};
//===========================^=VISITING=^===========================================================

//===========================V=DEFINITIONS=V========================================================
class BaseNode : public ecs::Entity {
 public:
  // NOLINTBEGIN(readability-convert-member-functions-to-static): unified
  // interface with descendants
  [[nodiscard]] std::span<NodesVariant> ChildrenSpan() { return {}; }
  [[nodiscard]] std::span<const NodesVariant> ChildrenSpan() const { return {}; }
  // NOLINTEND(readability-convert-member-functions-to-static)

  bool& Healthy() { return healthy_; }
  [[nodiscard]] bool Healthy() const { return healthy_; }

 private:
  bool healthy_{true};
};

template <std::size_t n_children>
class NaryNode : public BaseNode {
 public:
  [[nodiscard]] std::span<NodesVariant> ChildrenSpan() { return children_; }
  [[nodiscard]] std::span<const NodesVariant> ChildrenSpan() const {
    return children_;
  }

  [[nodiscard]] std::array<NodesVariant, n_children>& Children() {
    return children_;
  }
  [[nodiscard]] const std::array<NodesVariant, n_children>& Children() const {
    return children_;
  }

 private:
  std::array<NodesVariant, n_children> children_;
};

class ArbitraryNode : public BaseNode {
 public:
  [[nodiscard]] std::span<NodesVariant> ChildrenSpan() {
    return children_;
  }
  [[nodiscard]] std::span<const NodesVariant> ChildrenSpan() const {
    return children_;
  }

  [[nodiscard]] std::vector<NodesVariant>& Children() { return children_; }
  [[nodiscard]] const std::vector<NodesVariant>& Children() const {
    return children_;
  }

 private:
  std::vector<NodesVariant> children_;
};

// NOLINTBEGIN
#define NODE(Node, Parent) \
  class Node : public Parent {};
// NOLINTEND

#include <language_data/grammar_nodes.dat>

#undef NODE
//===========================^=DEFINITIONS=^========================================================

//===========================V=NAMES=V==============================================================

[[nodiscard]] inline std::string GetName(const BaseNode* /* node */) {
  return "BaseNode";
}

template <std::size_t n_children>
[[nodiscard]] inline std::string GetName(
    const NaryNode<n_children>* /* node */) {
  return "NaryNode<" + std::to_string(n_children) + ">";
}

[[nodiscard]] inline std::string GetName(const ArbitraryNode* /* node */) {
  return "ArbitraryNode";
}

#define NODE(Node, Parent)                                           \
  [[nodiscard]] inline std::string GetName(const Node* /* node */) { \
    return #Node;                                                    \
  }

#include <language_data/grammar_nodes.dat>

#undef NODE

//===========================^=NAMES=^==============================================================

}  // namespace parser::nodes
