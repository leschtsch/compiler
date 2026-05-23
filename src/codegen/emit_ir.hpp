#pragma once

#include <parser/ast.hpp>

#include <memory>

#include "llvm/IR/Module.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"

namespace ir {

struct EmitIrRet {
  std::unique_ptr<llvm::LLVMContext> ctx;
  std::unique_ptr<llvm::Module>mod;
  std::unique_ptr<llvm::IRBuilder<>> builder;
};

EmitIrRet EmitIrImpl(
    const parser::nodes::NodesVariant& node);

}  // namespace ir
