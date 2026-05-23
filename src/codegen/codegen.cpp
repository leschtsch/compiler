#include "codegen.hpp"

#include <codegen/compile.hpp>
#include <codegen/emit_ir.hpp>

#include <ostream>

#include "llvm/IR/Module.h"
#include "llvm/Support/raw_os_ostream.h"

namespace ir {

bool EmitIr(std::ostream& ostream, const parser::nodes::NodesVariant& node) {
  auto [ctx, module, builder] = EmitIrImpl(node);
  if (!module) {
    return false;
  }

  auto raw_os_ostream = llvm::raw_os_ostream(ostream);
  module->print(raw_os_ostream, nullptr);

  return true;
}

bool Compile(const std::string& filename,
             const parser::nodes::NodesVariant& node) {
  auto [ctx, module, builder] = EmitIrImpl(node);
  return CompileImpl(filename, module);
}

}  // namespace ir
