#pragma once

#include <utils/type_tuple.hpp>

#include <variant>
#include <vector>

#include "tokens.hpp"

namespace parser {

struct AstNode {
  using TokenVariant = utils::ConvertTo<tokens::AllTokensTuple, std::variant>;

  /// ErrorToken means error happened here and contains error info
  TokenVariant token;

  /// error happened somewhere down the tree, but we still know what this node should be
  bool has_error;
  std::vector<struct AstNode> children;
};

}  // namespace parser
