#pragma once

#include <utils/type_tuple.hpp>

#include <variant>
#include <vector>

#include "tokens.hpp"

namespace parser {

struct AstNodev1 {
  using TokenVariant = utils::ConvertTo<tokens::AllTokensTuple, std::variant>;

  /// ErrorToken means error happened here and contains error info
  TokenVariant token;

  /// error happened somewhere down the tree, but we still know what this node should be
  bool has_active_error;
  std::vector<struct AstNodev1> children;
};

using AstNode = AstNodev1;

}  // namespace parser
