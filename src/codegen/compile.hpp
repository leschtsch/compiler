#pragma once

#include <string>
#include "llvm/IR/Module.h"
#include <memory>

namespace ir {

bool CompileImpl(const std::string& filename, std::unique_ptr<llvm::Module>& module);

}  // namespace ir
