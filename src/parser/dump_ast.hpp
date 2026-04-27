#pragma once

#include <ostream>

#include "ast.hpp"

namespace parserv2 {

void DumpAst(std::ostream& ostream, const nodes::NodesVariant& node);

}  // namespace parserv2
