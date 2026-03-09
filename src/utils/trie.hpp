#pragma once

/**
 * @file trie.hpp
 */

#include <cstdint>
#include <cstdio>
#include <format>
#include <optional>
#include <ostream>
#include <unordered_map>

namespace utils {

/**
 * @brief Simple ad hoc trie
 *
 * It probably lacks functionality and uses some
 * bad solutions that work for our special cases.
 */
template <typename Alphabet, typename Val>
class Trie {
 public:
  class Node {
    friend class Trie<Alphabet, Val>;

   public:
    [[nodiscard]] auto HasValue() const noexcept -> bool {
      return value_.has_value();
    }

    auto Value() const -> const Val& { return value_.value(); }
    auto Value() -> Val& { return value_.value(); }

   private:
    std::unordered_map<Alphabet, Node> links_;
    std::optional<Val> value_;
  };

  Trie() = default;

  auto NextNode(Alphabet chr, Node* node = nullptr) -> Node* {
    if (node == nullptr) {
      node = &root_;
    }

    auto found = node->links_.find(chr);
    if (found == node->links_.end()) {
      return nullptr;
    }

    return &found->second;
  }

  template <typename Container>
  auto Find(const Container& cont, Node* node = nullptr) -> Node* {
    if (node == nullptr) {
      node = &root_;
    }

    for (Alphabet chr : cont) {
      if (node == nullptr) {
        break;
      }

      node = NextNode(chr, node);
    }

    return node;
  }

  template <typename Container, typename U>
  void Insert(const Container& cont, U&& val, Node* node = nullptr) {
    if (node == nullptr) {
      node = &root_;
    }

    for (Alphabet chr : cont) {
      node = &node->links_[chr];
    }

    node->value_ = std::forward<U>(val);
  }

  void Dump(std::ostream& ostream) {
    ostream << "digraph trie {\n";
    DumpImpl(ostream, &root_);
    ostream << "}\n";
  }

 private:
  static void DumpImpl(std::ostream& ostream, Node* node) {
    for (auto& [chr, child] : node->links_) {
      if (chr == '\0') {
        continue;
      }

      ostream << std::format("{} -> {} [label=\"{}\"];\n",
                             reinterpret_cast<std::uintptr_t>(node),
                             reinterpret_cast<std::uintptr_t>(&child),
                             chr);
      DumpImpl(ostream, &child);
    }
  }

  Node root_;
};

}  // namespace utils
