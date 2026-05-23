#pragma once

#include <parser/ast.hpp>

#include <memory>

#include "llvm/IR/Module.h"

namespace ir {

std::unique_ptr<llvm::Module> EmitIrImpl(
    const parser::nodes::NodesVariant& node);

}  // namespace ir
