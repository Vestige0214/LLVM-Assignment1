#include <cstdio>

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CallSite.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"
#include "ProfilingInstrumentationPass.h"


using namespace llvm;
using cgprofiler::ProfilingInstrumentationPass;

#define DEBUG_TYPE = "ProfilingInstrumentationPass";

namespace cgprofiler {

char ProfilingInstrumentationPass::ID = 0;

}


// Returns a map (Function* -> uint64_t).
static DenseMap<Function*, uint64_t>
computeFunctionIDs(llvm::ArrayRef<Function*> functions) {
  DenseMap<Function*, uint64_t> idMap;

  size_t nextID = 0;
  for (auto f : functions) {
    idMap[f] = nextID;
    ++nextID;
  }

  return idMap;
}

// Returns a set of all internal (defined) functions.
static DenseSet<Function*>
computeInternal(llvm::ArrayRef<Function*> functions) {
  DenseSet<Function*> internal;

  for (auto f : functions) {
    if (!f->isDeclaration()) {
      internal.insert(f);
    }
  }

  return internal;
}


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


// Create the CCOUNT(functionInfo) table used by the runtime library.
static void
createFunctionTable(Module& m, uint64_t numFunctions) {
  auto& context = m.getContext();

  // Create the component types of the table
  auto* int64Ty    = Type::getInt64Ty(context);
  auto* stringTy   = Type::getInt8PtrTy(context);
  Type* fieldTys[] = {stringTy, int64Ty, int64Ty};
  auto* structTy   = StructType::get(context, fieldTys, false);
  auto* tableTy    = ArrayType::get(structTy, numFunctions);
  auto* zero       = ConstantInt::get(int64Ty, 0, false);

  // Compute and store an externally visible array of function information.
  std::vector<Constant*> values;
  std::transform(
      m.begin(),
      m.end(),
      std::back_inserter(values),
      [&m, zero, structTy](auto& f) {
        Constant* structFields[] = {createConstantString(m, f.getName()), zero};
        return ConstantStruct::get(structTy, structFields);
      });
  auto* functionTable = ConstantArray::get(tableTy, values);
  new GlobalVariable(m,
                     tableTy,
                     false,
                     GlobalValue::ExternalLinkage,
                     functionTable,
                     "LLVMAnalysis_functionInfo");
}

bool
ProfilingInstrumentationPass::runOnModule(llvm::Module& m) {
  auto& context = m.getContext();

  // First identify the functions we wish to track
  std::vector<Function*> toCount;
  for (auto& f : m) {
    toCount.push_back(&f);
  }

  ids = computeFunctionIDs(toCount);
  internal = computeInternal(toCount);
  auto const numFunctions = toCount.size();

  // Store the number of functions into an externally visible variable.
  auto* int64Ty = Type::getInt64Ty(context);
  auto* numFunctionsGlobal = ConstantInt::get(int64Ty, numFunctions, false);
  new GlobalVariable(m,
                     int64Ty,
                     true,
                     GlobalValue::ExternalLinkage,
                     numFunctionsGlobal,
                     "LLVMAnalysis_numberOfFunctions");

  createFunctionTable(m, numFunctions);

  // Install the result printing function so that it prints out the counts after
  // the entire program is finished executing.
  auto* voidTy  = Type::getVoidTy(context);
  auto* printer = m.getOrInsertFunction("LLVMAnalysis_printNumOfRuns", voidTy, nullptr);
  appendToGlobalDtors(m, llvm::cast<Function>(printer), 0);



  

    // Declare the counter function
  auto* helperTy = FunctionType::get(voidTy, int64Ty, false);
  auto* counter  = m.getOrInsertFunction("LLVMAnalysis_functionCalled", helperTy);

  //Declare the set Line Number function
  llvm::Type* paramRef[] = {Type::getInt64Ty(context), Type::getInt64Ty(context)};
  auto* lineNumHelperTy = FunctionType::get(voidTy, paramRef, false);
  auto* lineNumFunc = m.getOrInsertFunction("LLVMAnalysis_setLineNum", lineNumHelperTy);


  for (auto f : toCount) {
    // We only want to instrument internally defined functions.
    if (f->isDeclaration()) {
      continue;
    }

    // Count each internal function as it executes.
    handleCalledFunction(*f, counter);

    // Count each external function as it is called.
    for (auto& bb : *f) {
      for (auto& i : bb) {
        handleInstruction(CallSite(&i), counter, lineNumFunc);
      }
    }
  }

  return true;
}


void
ProfilingInstrumentationPass::handleCalledFunction(Function& f, Value* counter) {
  //Count is added to first insertion point
  IRBuilder<> builder(&*f.getEntryBlock().getFirstInsertionPt());
  builder.CreateCall(counter, builder.getInt64(ids[&f]));
}


void
ProfilingInstrumentationPass::handleInstruction(CallSite cs, Value* counter, Value* lineNumFunc) {
  // auto& context = cs->getContext();
  // Check whether the instruction is actually a call
  if (!cs.getInstruction()) {
    return;
  }
  uint64_t lineNum = 0;

  if (DILocation *Loc = cs.getInstruction()->getDebugLoc()){
    lineNum = Loc->getLine();
    // StringRef fileName = Loc.getFilename();
  }

  // Check whether the called function is directly invoked
  auto called = dyn_cast<Function>(cs.getCalledValue()->stripPointerCasts());
  if (!called) {
    return;
  }

  IRBuilder<> builder2(cs.getInstruction());
  llvm::Value* paramRef[] = {builder2.getInt64(ids[called]), builder2.getInt64(lineNum)};
  builder2.CreateCall(lineNumFunc, paramRef);

  // Check if the function is internal or blacklisted.
  if (internal.count(called) || !ids.count(called)) {
    // Internal functions are counted upon the entry of each function body.
    // Blacklisted functions are not counted. Neither should proceed.
    return;
  }

  // External functions are counted at their invocation sites.
  IRBuilder<> builder(cs.getInstruction());
  builder.CreateCall(counter, builder.getInt64(ids[called]));
}
