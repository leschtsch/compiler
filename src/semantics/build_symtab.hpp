#pragma once

#include <parser/ast.hpp>

namespace semantics {

bool BuildSymtab(parser::nodes::NodesVariant& ast);

}  // namespace semantics
