
#include "llvm/IR/Module.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"

#include "ProfilingInstrumentationPass.h"


using namespace cgprofiler;


namespace cgprofiler {

char ProfilingInstrumentationPass::ID = 0;

} // namespace cgprofiler


static llvm::Constant*
createConstantString(llvm::Module& m, llvm::StringRef str) {
  auto& context = m.getContext();

  auto* name    = llvm::ConstantDataArray::getString(context, str, true);
  auto* int8Ty  = llvm::Type::getInt8Ty(context);
  auto* arrayTy = llvm::ArrayType::get(int8Ty, str.size() + 1);
  auto* asStr   = new llvm::GlobalVariable(
      m, arrayTy, true, llvm::GlobalValue::PrivateLinkage, name);

  auto* zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0);
  llvm::Value* indices[] = {zero, zero};
  return llvm::ConstantExpr::getInBoundsGetElementPtr(arrayTy, asStr, indices);
}

void
handleIntruction(llvm::CallSite cs){
  if (!cs.getInstruction()){
    return;
  }
  auto called = cs.getCalledValue()->stripPointerCasts();
  auto calledFunction = llvm::dyn_cast<llvm::Function>(called);
  if (!calledFunction){
    return;
  }
}

bool
ProfilingInstrumentationPass::runOnModule(llvm::Module& m) {
  for (auto &f : m){
    for (auto &b : f){
      for (auto &i : b){
        handleIntruction(&i);
      }
    }
  }
  return true;
}



