#include "emit_ir.hpp"

#include <ecs/ecs.hpp>
#include <ecs/used_components.hpp>
#include <parser/ast.hpp>
#include <semantics/symbols.hpp>
#include <semantics/types.hpp>

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <ostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/raw_os_ostream.h"
#include "llvm/Support/raw_ostream.h"

namespace ir {

namespace {

//===========================V=EMITTER=CLASS=V======================================================
class IrEmitter {
 public:
  explicit IrEmitter(auto& context, auto& module, auto& builder)
      : context_(context), module_(module), builder_(builder) {}

  bool HasError() const { return has_error_; }

  void Program(const parser::nodes::NodesVariant& program);

  void operator()(const auto* ptr);
  void operator()(const parser::nodes::IfStatement* if_stmt);
  void operator()(const parser::nodes::WhileStatement* while_stmt);
  void operator()(const parser::nodes::VariableDefinition* def);
  void operator()(const parser::nodes::ReturnStatement* ret);
  void operator()(const parser::nodes::Assignment* assignment);
  void operator()(const parser::nodes::CallStatement* call);

 private:
  class ExpressionVisitor {
   public:
    struct RetType {
      semantics::BasicTypes type;
      llvm::Value* value;
    };

    explicit ExpressionVisitor(IrEmitter& emitter) : emitter_(emitter) {}

    RetType CallImpl(auto* call);

    RetType operator()(const auto* ptr);
    RetType operator()(const parser::nodes::LogAnd* bin_op);
    RetType operator()(const parser::nodes::LogOr* bin_op);
    RetType operator()(const parser::nodes::BinEq* bin_op);
    RetType operator()(const parser::nodes::BinNeq* bin_op);
    RetType operator()(const parser::nodes::BinLt* bin_op);
    RetType operator()(const parser::nodes::BinLeq* bin_op);
    RetType operator()(const parser::nodes::BinGt* bin_op);
    RetType operator()(const parser::nodes::BinGeq* bin_op);
    RetType operator()(const parser::nodes::BinPlus* bin_op);
    RetType operator()(const parser::nodes::BinMinus* bin_op);
    RetType operator()(const parser::nodes::BinMul* bin_op);
    RetType operator()(const parser::nodes::BinDiv* bin_op);
    RetType operator()(const parser::nodes::BinMod* bin_op);
    RetType operator()(const parser::nodes::LogNotExpr* log_not);
    RetType operator()(const parser::nodes::CallExpression* call);
    RetType operator()(const parser::nodes::Locator* locator);
    RetType operator()(const parser::nodes::FloatLiteral* literal);
    RetType operator()(const parser::nodes::IntLiteral* literal);

   private:
    struct PromotedValues {
      semantics::BasicTypes type;
      llvm::Value* lhs;
      llvm::Value* rhs;
    };

    PromotedValues TypePromotion(const RetType& lhs, const RetType& rhs);
    PromotedValues LhsRhsPromotion(const parser::nodes::BinOp* bin_op);

    RetType DummyRet();
    IrEmitter& emitter_;
  };

  void FunctionProto(
      const std::unique_ptr<parser::nodes::FunctionDefinition>& func);
  void FunctionWithBody(
      const std::unique_ptr<parser::nodes::FunctionDefinition>& func);
  llvm::Value* DefaultRet(semantics::BasicTypes type);

  llvm::Type* Basic2LlvmType(semantics::BasicTypes type);
  llvm::Value* TypeConversion(semantics::BasicTypes from_t,
                              semantics::BasicTypes to_t,
                              llvm::Value* val);
  llvm::Value* Uint2Bool(llvm::Value* value);
  llvm::Value* Bool2Uint(llvm::Value* value);
  llvm::AllocaInst* CreateEntryBlockAlloca(
      const semantics::VariableSymbol* symbol);

  void Error(const std::string& msg) {
    std::cerr << msg << "\n";
    has_error_ = true;
  }

  std::unique_ptr<llvm::LLVMContext>& context_;
  std::unique_ptr<llvm::Module>& module_;
  std::unique_ptr<llvm::IRBuilder<>>& builder_;

  ExpressionVisitor expression_visitor_{*this};

  llvm::Function* last_func_ll_{nullptr};
  semantics::FunctionSymbol* last_func_symb_{nullptr};
  std::unordered_map<const semantics::VariableSymbol*, llvm::AllocaInst*>
      allocas_;
  std::unordered_map<std::string, llvm::Function*> function_protos_;

  bool has_error_{false};
};
//===========================^=EMITTER=CLASS=^======================================================

//===========================V=EXPRESSIONS=V========================================================
IrEmitter::ExpressionVisitor::RetType IrEmitter::ExpressionVisitor::CallImpl(
    auto* call) {
  const auto& locator =
      std::get<std::unique_ptr<parser::nodes::Locator>>(call->Func());
  const auto& call_suff =
      std::get<std::unique_ptr<parser::nodes::CallSuffix>>(call->Args());

  auto* symb = ecs::Get<ecs::SymbolUse>(*locator).Value();

  if (symb->Kind() != semantics::SymbolKind::kFunction) {
    emitter_.Error("callee " + symb->Name() + " is not a function");
    return DummyRet();
  }

  auto* func_symb = static_cast<semantics::FunctionSymbol*>(symb);

  if (func_symb->Args().size() != call_suff->Children().size()) {
    emitter_.Error("wrong number of args in call of " + func_symb->Name());
    return DummyRet();
  }

  llvm::Function* callee = emitter_.function_protos_[func_symb->Name()];
  std::vector<llvm::Value*> args;
  args.reserve(func_symb->Args().size());

  std::size_t idx = 0;
  for (const auto& arg : call_suff->Children()) {
    auto [type, val] = parser::nodes::VisitPtr(*this, arg);
    val = emitter_.TypeConversion(type, func_symb->Args()[idx], val);
    args.push_back(val);
    ++idx;
  }

  auto* val =
      emitter_.builder_->CreateCall(callee, args, "call_" + func_symb->Name());
  return {.type = func_symb->ReturnType(), .value = val};
}

IrEmitter::ExpressionVisitor::RetType IrEmitter::ExpressionVisitor::operator()(
    const auto* ptr) {
  emitter_.Error("unknown expression kind " + parser::nodes::GetName(ptr));
  return DummyRet();
}

IrEmitter::ExpressionVisitor::RetType IrEmitter::ExpressionVisitor::operator()(
    const parser::nodes::LogAnd* bin_op) {
  auto [lhst, lhs] = parser::nodes::VisitPtr(*this, bin_op->Lhs());
  lhs = emitter_.Uint2Bool(
      emitter_.TypeConversion(lhst, semantics::BasicTypes::kUint, lhs));

  auto [rhst, rhs] = parser::nodes::VisitPtr(*this, bin_op->Rhs());
  rhs = emitter_.Uint2Bool(
      emitter_.TypeConversion(rhst, semantics::BasicTypes::kUint, rhs));

  auto* val = emitter_.builder_->CreateLogicalAnd(lhs, rhs);
  return {.type = semantics::BasicTypes::kUint,
          .value = emitter_.Bool2Uint(val)};
}

IrEmitter::ExpressionVisitor::RetType IrEmitter::ExpressionVisitor::operator()(
    const parser::nodes::LogOr* bin_op) {
  auto [lhst, lhs] = parser::nodes::VisitPtr(*this, bin_op->Lhs());
  lhs = emitter_.Uint2Bool(
      emitter_.TypeConversion(lhst, semantics::BasicTypes::kUint, lhs));

  auto [rhst, rhs] = parser::nodes::VisitPtr(*this, bin_op->Rhs());
  rhs = emitter_.Uint2Bool(
      emitter_.TypeConversion(rhst, semantics::BasicTypes::kUint, rhs));

  auto* val = emitter_.builder_->CreateLogicalOr(lhs, rhs);
  return {.type = semantics::BasicTypes::kUint,
          .value = emitter_.Bool2Uint(val)};
}

IrEmitter::ExpressionVisitor::RetType IrEmitter::ExpressionVisitor::operator()(
    const parser::nodes::BinEq* bin_op) {
  auto [target_t, lhs, rhs] = LhsRhsPromotion(bin_op);
  llvm::Value* val = nullptr;

  if (semantics::IsFloatingPoint(target_t)) {
    val = emitter_.builder_->CreateFCmpOEQ(lhs, rhs);
  } else {
    val = emitter_.builder_->CreateICmpEQ(lhs, rhs);
  }

  return {.type = semantics::BasicTypes::kUint,
          .value = emitter_.Bool2Uint(val)};
}

IrEmitter::ExpressionVisitor::RetType IrEmitter::ExpressionVisitor::operator()(
    const parser::nodes::BinNeq* bin_op) {
  auto [target_t, lhs, rhs] = LhsRhsPromotion(bin_op);
  llvm::Value* val = nullptr;

  if (semantics::IsFloatingPoint(target_t)) {
    val = emitter_.builder_->CreateFCmpONE(lhs, rhs);
  } else {
    val = emitter_.builder_->CreateICmpNE(lhs, rhs);
  }

  return {.type = semantics::BasicTypes::kUint,
          .value = emitter_.Bool2Uint(val)};
}

IrEmitter::ExpressionVisitor::RetType IrEmitter::ExpressionVisitor::operator()(
    const parser::nodes::BinLt* bin_op) {
  auto [target_t, lhs, rhs] = LhsRhsPromotion(bin_op);
  llvm::Value* val = nullptr;

  if (semantics::IsFloatingPoint(target_t)) {
    val = emitter_.builder_->CreateFCmpOLT(lhs, rhs);
  } else if (semantics::IsUnsigned(target_t)) {
    val = emitter_.builder_->CreateICmpULT(lhs, rhs);
  } else {
    val = emitter_.builder_->CreateICmpSLT(lhs, rhs);
  }

  return {.type = semantics::BasicTypes::kUint,
          .value = emitter_.Bool2Uint(val)};
}

IrEmitter::ExpressionVisitor::RetType IrEmitter::ExpressionVisitor::operator()(
    const parser::nodes::BinLeq* bin_op) {
  auto [target_t, lhs, rhs] = LhsRhsPromotion(bin_op);
  llvm::Value* val = nullptr;

  if (semantics::IsFloatingPoint(target_t)) {
    val = emitter_.builder_->CreateFCmpOLE(lhs, rhs);
  } else if (semantics::IsUnsigned(target_t)) {
    val = emitter_.builder_->CreateICmpULE(lhs, rhs);
  } else {
    val = emitter_.builder_->CreateICmpSLE(lhs, rhs);
  }

  return {.type = semantics::BasicTypes::kUint,
          .value = emitter_.Bool2Uint(val)};
}

IrEmitter::ExpressionVisitor::RetType IrEmitter::ExpressionVisitor::operator()(
    const parser::nodes::BinGt* bin_op) {
  auto [target_t, lhs, rhs] = LhsRhsPromotion(bin_op);
  llvm::Value* val = nullptr;

  if (semantics::IsFloatingPoint(target_t)) {
    val = emitter_.builder_->CreateFCmpOGT(lhs, rhs);
  } else if (semantics::IsUnsigned(target_t)) {
    val = emitter_.builder_->CreateICmpUGT(lhs, rhs);
  } else {
    val = emitter_.builder_->CreateICmpSGT(lhs, rhs);
  }

  return {.type = semantics::BasicTypes::kUint,
          .value = emitter_.Bool2Uint(val)};
}

IrEmitter::ExpressionVisitor::RetType IrEmitter::ExpressionVisitor::operator()(
    const parser::nodes::BinGeq* bin_op) {
  auto [target_t, lhs, rhs] = LhsRhsPromotion(bin_op);
  llvm::Value* val = nullptr;

  if (semantics::IsFloatingPoint(target_t)) {
    val = emitter_.builder_->CreateFCmpOGE(lhs, rhs);
  } else if (semantics::IsUnsigned(target_t)) {
    val = emitter_.builder_->CreateICmpUGE(lhs, rhs);
  } else {
    val = emitter_.builder_->CreateICmpSGE(lhs, rhs);
  }

  return {.type = semantics::BasicTypes::kUint,
          .value = emitter_.Bool2Uint(val)};
}

IrEmitter::ExpressionVisitor::RetType IrEmitter::ExpressionVisitor::operator()(
    const parser::nodes::BinPlus* bin_op) {
  auto [target_t, lhs, rhs] = LhsRhsPromotion(bin_op);
  llvm::Value* val = nullptr;

  if (semantics::IsFloatingPoint(target_t)) {
    val = emitter_.builder_->CreateFAdd(lhs, rhs);
  } else if (semantics::IsUnsigned(target_t)) {
    val = emitter_.builder_->CreateAdd(lhs, rhs);
  } else {
    val = emitter_.builder_->CreateNSWAdd(lhs, rhs);
  }

  return {.type = semantics::BasicTypes::kUint, .value = val};
}

IrEmitter::ExpressionVisitor::RetType IrEmitter::ExpressionVisitor::operator()(
    const parser::nodes::BinMinus* bin_op) {
  auto [target_t, lhs, rhs] = LhsRhsPromotion(bin_op);
  llvm::Value* val = nullptr;

  if (semantics::IsFloatingPoint(target_t)) {
    val = emitter_.builder_->CreateFSub(lhs, rhs);
  } else if (semantics::IsUnsigned(target_t)) {
    val = emitter_.builder_->CreateSub(lhs, rhs);
  } else {
    val = emitter_.builder_->CreateNSWSub(lhs, rhs);
  }

  return {.type = semantics::BasicTypes::kUint, .value = val};
}

IrEmitter::ExpressionVisitor::RetType IrEmitter::ExpressionVisitor::operator()(
    const parser::nodes::BinMul* bin_op) {
  auto [target_t, lhs, rhs] = LhsRhsPromotion(bin_op);
  llvm::Value* val = nullptr;

  if (semantics::IsFloatingPoint(target_t)) {
    val = emitter_.builder_->CreateFMul(lhs, rhs);
  } else if (semantics::IsUnsigned(target_t)) {
    val = emitter_.builder_->CreateMul(lhs, rhs);
  } else {
    val = emitter_.builder_->CreateNSWMul(lhs, rhs);
  }

  return {.type = semantics::BasicTypes::kUint, .value = val};
}

IrEmitter::ExpressionVisitor::RetType IrEmitter::ExpressionVisitor::operator()(
    const parser::nodes::BinDiv* bin_op) {
  auto [target_t, lhs, rhs] = LhsRhsPromotion(bin_op);
  llvm::Value* val = nullptr;

  if (semantics::IsFloatingPoint(target_t)) {
    val = emitter_.builder_->CreateFDiv(lhs, rhs);
  } else if (semantics::IsUnsigned(target_t)) {
    val = emitter_.builder_->CreateUDiv(lhs, rhs);
  } else {
    val = emitter_.builder_->CreateSDiv(lhs, rhs);
  }

  return {.type = semantics::BasicTypes::kUint, .value = val};
}

IrEmitter::ExpressionVisitor::RetType IrEmitter::ExpressionVisitor::operator()(
    const parser::nodes::BinMod* bin_op) {
  auto [target_t, lhs, rhs] = LhsRhsPromotion(bin_op);
  llvm::Value* val = nullptr;

  if (semantics::IsFloatingPoint(target_t)) {
    emitter_.Error("can't take remainder of divison by float");
    return DummyRet();
  }
  if (semantics::IsUnsigned(target_t)) {
    val = emitter_.builder_->CreateURem(lhs, rhs);
  } else {
    val = emitter_.builder_->CreateSRem(lhs, rhs);
  }

  return {.type = semantics::BasicTypes::kUint, .value = val};
}

IrEmitter::ExpressionVisitor::RetType IrEmitter::ExpressionVisitor::operator()(
    const parser::nodes::LogNotExpr* log_not) {
  auto [type, val] = parser::nodes::VisitPtr(*this, log_not->Expr());
  val = emitter_.Uint2Bool(
      emitter_.TypeConversion(type, semantics::BasicTypes::kUint, val));

  val = emitter_.builder_->CreateNot(val);
  return {.type = semantics::BasicTypes::kUint,
          .value = emitter_.Bool2Uint(val)};
}

IrEmitter::ExpressionVisitor::RetType IrEmitter::ExpressionVisitor::operator()(
    const parser::nodes::CallExpression* call) {
  return CallImpl(call);
}

IrEmitter::ExpressionVisitor::RetType IrEmitter::ExpressionVisitor::operator()(
    const parser::nodes::Locator* locator) {

  auto* symb = ecs::Get<ecs::SymbolUse>(*locator).Value();
  if (symb->Kind() != semantics::SymbolKind::kVariable) {
    emitter_.Error("readed " + symb->Name() + " is not a variable");
    return DummyRet();
  }

  auto* var_symb = static_cast<semantics::VariableSymbol*>(symb);
  auto* alloca = emitter_.allocas_[var_symb];
  auto* val = emitter_.builder_->CreateLoad(
      emitter_.Basic2LlvmType(var_symb->Type()), alloca);

  return {.type = var_symb->Type(), .value = val};
}

IrEmitter::ExpressionVisitor::RetType IrEmitter::ExpressionVisitor::operator()(
    const parser::nodes::FloatLiteral* literal) {
  auto float_val = ecs::Get<ecs::FloatValue>(*literal).Value();
  auto* val = llvm::ConstantFP::get(
      emitter_.Basic2LlvmType(semantics::BasicTypes::kFloat),
      static_cast<double>(float_val));

  return {.type = semantics::BasicTypes::kFloat, .value = val};
}

IrEmitter::ExpressionVisitor::RetType IrEmitter::ExpressionVisitor::operator()(
    const parser::nodes::IntLiteral* literal) {
  auto int_val = ecs::Get<ecs::IntValue>(*literal).Value();
  auto* val = llvm::ConstantInt::get(
      emitter_.Basic2LlvmType(semantics::BasicTypes::kInt),
      static_cast<std::int64_t>(int_val));

  return {.type = semantics::BasicTypes::kInt, .value = val};
}

IrEmitter::ExpressionVisitor::PromotedValues
IrEmitter::ExpressionVisitor::TypePromotion(const RetType& lhs,
                                            const RetType& rhs) {

  auto [lhst, lhsv] = lhs;
  auto [rhst, rhsv] = rhs;
  auto target_t = semantics::CommonType(lhst, rhst);

  lhsv = emitter_.TypeConversion(lhst, target_t, lhsv);
  rhsv = emitter_.TypeConversion(rhst, target_t, rhsv);

  return {.type = target_t, .lhs = lhsv, .rhs = rhsv};
}

IrEmitter::ExpressionVisitor::PromotedValues
IrEmitter::ExpressionVisitor::LhsRhsPromotion(
    const parser::nodes::BinOp* bin_op) {
  return TypePromotion(parser::nodes::VisitPtr(*this, bin_op->Lhs()),
                       parser::nodes::VisitPtr(*this, bin_op->Rhs()));
}

IrEmitter::ExpressionVisitor::RetType IrEmitter::ExpressionVisitor::DummyRet() {
  auto* val = llvm::ConstantInt::get(
      emitter_.Basic2LlvmType(semantics::BasicTypes::kUint), 64);
  return {.type = semantics::BasicTypes::kUint, .value = val};
}

//===========================^=EXPRESSIONS=^========================================================

void IrEmitter::Program(const parser::nodes::NodesVariant& program) {
  const auto& prog_ptr =
      std::get<std::unique_ptr<parser::nodes::Program>>(program);

  for (const auto& child : prog_ptr->ChildrenSpan()) {
    const auto& child_ptr =
        std::get<std::unique_ptr<parser::nodes::FunctionDefinition>>(child);
    FunctionProto(child_ptr);
  }

  for (const auto& child : prog_ptr->ChildrenSpan()) {
    const auto& child_ptr =
        std::get<std::unique_ptr<parser::nodes::FunctionDefinition>>(child);
    FunctionWithBody(child_ptr);
  }
}

//===========================V=STATEMENTS=V=========================================================
void IrEmitter::operator()(const auto* ptr) {
  if (ptr == nullptr) {
    return;
  }

  for (auto& child : ptr->ChildrenSpan()) {
    parser::nodes::VisitPtr(*this, child);
  }
}

bool IsNullptr(const parser::nodes::NodesVariant& var) {
  return std::visit([](auto& ptr) { return ptr == nullptr; }, var);
}

void IrEmitter::operator()(const parser::nodes::IfStatement* if_stmt) {
  llvm::BasicBlock* then_bb =
      llvm::BasicBlock::Create(*context_, "then", last_func_ll_);
  llvm::BasicBlock* merge_bb =
      llvm::BasicBlock::Create(*context_, "ifcont", last_func_ll_);

  llvm::BasicBlock* else_bb = nullptr;
  const auto& else_var = if_stmt->Else();
  if (!IsNullptr(else_var)) {
    else_bb = llvm::BasicBlock::Create(*context_, "else", last_func_ll_);
  }

  auto [type, val] =
      parser::nodes::VisitPtr(expression_visitor_, if_stmt->Cond());
  val = TypeConversion(type, semantics::BasicTypes::kUint, val);
  val = Uint2Bool(val);
  builder_->CreateCondBr(
      val, then_bb, (else_bb != nullptr) ? else_bb : merge_bb);

  builder_->SetInsertPoint(then_bb);
  parser::nodes::VisitPtr(*this, if_stmt->Then());
  builder_->CreateBr(merge_bb);

  if (else_bb != nullptr) {
    builder_->SetInsertPoint(else_bb);
    parser::nodes::VisitPtr(*this, if_stmt->Else());
    builder_->CreateBr(merge_bb);
  }

  builder_->SetInsertPoint(merge_bb);
}

void IrEmitter::operator()(const parser::nodes::WhileStatement* while_stmt) {
  llvm::BasicBlock* cond_bb =
      llvm::BasicBlock::Create(*context_, "loopcond", last_func_ll_);
  llvm::BasicBlock* body_bb =
      llvm::BasicBlock::Create(*context_, "loopbody", last_func_ll_);
  llvm::BasicBlock* exit_bb =
      llvm::BasicBlock::Create(*context_, "loopexit", last_func_ll_);

  builder_->CreateBr(cond_bb);
  builder_->SetInsertPoint(cond_bb);

  auto [type, val] =
      parser::nodes::VisitPtr(expression_visitor_, while_stmt->Cond());
  val = TypeConversion(type, semantics::BasicTypes::kUint, val);
  val = Uint2Bool(val);

  builder_->CreateCondBr(val, body_bb, exit_bb);

  builder_->SetInsertPoint(body_bb);
  parser::nodes::VisitPtr(*this, while_stmt->Body());
  builder_->CreateBr(cond_bb);

  builder_->SetInsertPoint(exit_bb);
}

void IrEmitter::operator()(const parser::nodes::VariableDefinition* def) {
  const auto& var_symb = ecs::Get<ecs::VariableDef>(*def).Value();
  auto* alloca = CreateEntryBlockAlloca(var_symb.get());
  auto [type, val] = parser::nodes::VisitPtr(expression_visitor_, def->Value());
  val = TypeConversion(type, var_symb->Type(), val);

  builder_->CreateStore(val, alloca);
}

void IrEmitter::operator()(const parser::nodes::Assignment* assignment) {
  const auto& locator =
      std::get<std::unique_ptr<parser::nodes::Locator>>(assignment->Dest());

  auto* symb = ecs::Get<ecs::SymbolUse>(*locator).Value();
  if (symb->Kind() != semantics::SymbolKind::kVariable) {
    Error("assignee " + symb->Name() + " is not a variable");
    return;
  }
  auto* var_symb = static_cast<semantics::VariableSymbol*>(symb);

  auto* alloca = CreateEntryBlockAlloca(var_symb);
  auto [type, val] =
      parser::nodes::VisitPtr(expression_visitor_, assignment->Src());
  val = TypeConversion(type, var_symb->Type(), val);

  builder_->CreateStore(val, alloca);
}

void IrEmitter::operator()(const parser::nodes::ReturnStatement* ret) {
  auto [type, val] = parser::nodes::VisitPtr(expression_visitor_, ret->Value());
  val = TypeConversion(type, last_func_symb_->ReturnType(), val);

  builder_->CreateRet(val);
  auto* dead_bb = llvm::BasicBlock::Create(*context_, "dead", last_func_ll_);
  builder_->SetInsertPoint(dead_bb);
}

void IrEmitter::operator()(const parser::nodes::CallStatement* call) {
  expression_visitor_.CallImpl(call);
}

void IrEmitter::FunctionProto(
    const std::unique_ptr<parser::nodes::FunctionDefinition>& func) {

  const auto& func_symb = ecs::Get<ecs::FunctionDef>(*func).Value();

  std::vector<llvm::Type*> args;
  args.reserve(func_symb->Args().size());

  for (auto type : func_symb->Args()) {
    args.push_back(Basic2LlvmType(type));
  }

  auto* func_type = llvm::FunctionType::get(
      Basic2LlvmType(func_symb->ReturnType()), args, false);

  auto* function = llvm::Function::Create(func_type,
                                          llvm::Function::ExternalLinkage,
                                          func_symb->Name(),
                                          module_.get());

  function_protos_[func_symb->Name()] = function;
}

void IrEmitter::FunctionWithBody(
    const std::unique_ptr<parser::nodes::FunctionDefinition>& func) {
  const auto& func_symb = ecs::Get<ecs::FunctionDef>(*func).Value();

  last_func_ll_ = function_protos_[func_symb->Name()];
  last_func_symb_ = func_symb.get();

  llvm::BasicBlock* block =
      llvm::BasicBlock::Create(*context_, "entry", last_func_ll_);
  builder_->SetInsertPoint(block);

  const auto& params =
      std::get<std::unique_ptr<parser::nodes::ParameterList>>(func->Params());

  std::size_t idx = 0;
  for (auto& arg : last_func_ll_->args()) {
    const auto& param = params->Children()[idx];
    const auto& param_ptr =
        std::get<std::unique_ptr<parser::nodes::ParameterDecl>>(param);
    const auto& param_symb = ecs::Get<ecs::VariableDef>(*param_ptr).Value();

    auto* alloca = CreateEntryBlockAlloca(param_symb.get());
    builder_->CreateStore(&arg, alloca);

    ++idx;
  }

  parser::nodes::VisitPtr(*this, func->Body());

  builder_->CreateRet(DefaultRet(last_func_symb_->ReturnType()));
}
//===========================^=STATEMENTS=^=========================================================

//===========================V=HELPERS=V============================================================
llvm::Value* IrEmitter::DefaultRet(semantics::BasicTypes type) {
  switch (type) {
    case semantics::BasicTypes::kChar: {
      auto* typ = llvm::Type::getInt8Ty(*context_);
      return llvm::ConstantInt::get(typ, 0);
    }
    case semantics::BasicTypes::kInt:
    case semantics::BasicTypes::kUint: {
      auto* typ = llvm::Type::getInt64Ty(*context_);
      return llvm::ConstantInt::get(typ, 0);
    }
    case semantics::BasicTypes::kFloat: {
      auto* typ = llvm::Type::getDoubleTy(*context_);
      return llvm::ConstantFP::get(typ, 0.0);
    }

    default:
      break;
  }

  Error("unknown type");
  auto* typ = llvm::Type::getInt64Ty(*context_);
  return llvm::ConstantInt::get(typ, 0);
}

llvm::Type* IrEmitter::Basic2LlvmType(semantics::BasicTypes type) {
  switch (type) {
    case semantics::BasicTypes::kChar:
      return llvm::Type::getInt8Ty(*context_);
    case semantics::BasicTypes::kInt:
    case semantics::BasicTypes::kUint:
      return llvm::Type::getInt64Ty(*context_);
    case semantics::BasicTypes::kFloat:
      return llvm::Type::getDoubleTy(*context_);

    default:
      Error("unknown type");
      return llvm::Type::getInt64Ty(*context_);
  }
}

// NOLINTNEXTLINE: big freaking switch
llvm::Value* IrEmitter::TypeConversion(semantics::BasicTypes from_t,
                                       semantics::BasicTypes to_t,
                                       llvm::Value* val) {
  using semantics::BasicTypes;

  if (from_t == to_t) {
    return val;
  }

  // freaking giant switch
  switch (from_t) {
    case BasicTypes::kChar: {
      switch (to_t) {
        case BasicTypes::kInt:
        case BasicTypes::kUint:
          return builder_->CreateSExt(val, Basic2LlvmType(to_t));
        case BasicTypes::kFloat:
          return builder_->CreateSIToFP(val, Basic2LlvmType(to_t));
        default:
          break;
      }
      break;
    }
    case BasicTypes::kInt: {
      switch (to_t) {
        case BasicTypes::kChar:
          return builder_->CreateTrunc(val, Basic2LlvmType(to_t));
        case BasicTypes::kUint:
          return val;
        case BasicTypes::kFloat:
          return builder_->CreateSIToFP(val, Basic2LlvmType(to_t));
        default:
          break;
      }
      break;
    }
    case BasicTypes::kUint: {
      switch (to_t) {
        case BasicTypes::kChar:
          return builder_->CreateTrunc(val, Basic2LlvmType(to_t));
        case BasicTypes::kInt:
          return val;
        case BasicTypes::kFloat:
          return builder_->CreateUIToFP(val, Basic2LlvmType(to_t));
        default:
          break;
      }
      break;
    }
    case BasicTypes::kFloat: {
      switch (to_t) {
        case BasicTypes::kChar:
        case BasicTypes::kInt:
          return builder_->CreateFPToSI(val, Basic2LlvmType(to_t));
        case BasicTypes::kUint:
          return builder_->CreateFPToUI(val, Basic2LlvmType(to_t));
        default:
          break;
      }
      break;
    }
    default:
      break;
  }

  Error("can't convert type");
  return val;
}

llvm::Value* IrEmitter::Uint2Bool(llvm::Value* val) {
  return builder_->CreateICmpNE(
      val,
      llvm::ConstantInt::get(Basic2LlvmType(semantics::BasicTypes::kUint), 0));
}

llvm::Value* IrEmitter::Bool2Uint(llvm::Value* val) {
  return builder_->CreateZExt(val,
                              Basic2LlvmType(semantics::BasicTypes::kUint));
}

llvm::AllocaInst* IrEmitter::CreateEntryBlockAlloca(
    const semantics::VariableSymbol* symbol) {
  std::ostringstream oss;
  oss << symbol->Name() << "." << reinterpret_cast<std::uintptr_t>(symbol);
  llvm::IRBuilder<> tmp_builder(&last_func_ll_->getEntryBlock(),
                                last_func_ll_->getEntryBlock().begin());
  auto* alloca = tmp_builder.CreateAlloca(
      Basic2LlvmType(symbol->Type()), nullptr, oss.str());
  allocas_[symbol] = alloca;
  return alloca;
}
//===========================^=HELPERS=^============================================================

}  // namespace

bool EmitIr(std::ostream& ostream, const parser::nodes::NodesVariant& node) {
  auto context = std::make_unique<llvm::LLVMContext>();
  auto module = std::make_unique<llvm::Module>("main", *context);
  auto builder = std::make_unique<llvm::IRBuilder<>>(*context);

  IrEmitter emitter(context, module, builder);
  emitter.Program(node);

  bool res = emitter.HasError() && llvm::verifyModule(*module, &llvm::errs());

  auto raw_os_ostream = llvm::raw_os_ostream(ostream);
  module->print(raw_os_ostream, nullptr);

  return res;
}

}  // namespace ir
