#pragma once

#include <parser/ast.hpp>

#include <ostream>

namespace misc {

void DumpAst(std::ostream& ostream, const parser::nodes::NodesVariant& node);

}  // namespace misc
