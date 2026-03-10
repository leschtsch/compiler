#pragma once

#include <utils/type_tuple.hpp>

#include <variant>
#include <vector>

#include "tokens.hpp"

namespace parser {

struct AstNode {
  using TokenVariant = utils::ConvertTo<tokens::AllTokensTuple, std::variant>;

  TokenVariant token;
  std::vector<struct AstNode> children;
};

}  // namespace parser
