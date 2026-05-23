#include "compile.hpp"

#include <iostream>
#include <memory>

#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/IR/PassManager.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/StandardInstrumentations.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/TargetParser/Host.h"

namespace ir {

namespace {

void Optimize(std::unique_ptr<llvm::Module>& module) {
  llvm::LoopAnalysisManager lam;
  llvm::FunctionAnalysisManager fam;
  llvm::CGSCCAnalysisManager cgam;
  llvm::ModuleAnalysisManager mam;

  llvm::PassBuilder pass_builder;
  pass_builder.registerModuleAnalyses(mam);
  pass_builder.registerCGSCCAnalyses(cgam);
  pass_builder.registerFunctionAnalyses(fam);
  pass_builder.registerLoopAnalyses(lam);
  pass_builder.crossRegisterProxies(lam, fam, cgam, mam);

  llvm::ModulePassManager mpm =
      pass_builder.buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O2);

  mpm.run(*module, mam);
}

}  // namespace

bool CompileImpl(const std::string& filename,
                 std::unique_ptr<llvm::Module>& module) {
  Optimize(module);

  llvm::InitializeAllTargetInfos();
  llvm::InitializeAllTargets();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmParsers();
  llvm::InitializeAllAsmPrinters();

  auto target_triple = llvm::sys::getDefaultTargetTriple();
  module->setTargetTriple(llvm::Triple(target_triple));

  std::string error;
  const auto* target =
      llvm::TargetRegistry::lookupTarget(module->getTargetTriple(), error);

  if (target == nullptr) {
    std::cerr << error << "\n";
    return false;
  }

  llvm::TargetOptions opt;
  auto* target_machine = target->createTargetMachine(
      llvm::Triple(target_triple), "generic", "", opt, llvm::Reloc::PIC_);

  module->setDataLayout(target_machine->createDataLayout());

  llvm::legacy::PassManager pass;
  auto filetype = llvm::CodeGenFileType::ObjectFile;

  std::error_code err;
  llvm::raw_fd_ostream dest(filename, err, llvm::sys::fs::OF_None);

  if (err) {
    std::cerr << "Could not open file: " << err.message();
    return false;
  }

  if (target_machine->addPassesToEmitFile(pass, dest, nullptr, filetype)) {
    std::cerr << "TargetMachine can't emit a file of this type";
    return false;
  }

  pass.run(*module);
  dest.flush();

  delete target_machine;
  return true;
}

}  // namespace ir
