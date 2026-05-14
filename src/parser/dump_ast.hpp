#pragma once

#include <ostream>

#include "ast.hpp"

namespace parser {

void DumpAst(std::ostream& ostream, const nodes::NodesVariant& node);

}  // namespace parser
