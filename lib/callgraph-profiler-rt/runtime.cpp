
#include <cstdint>
#include <cstdio>
#include "llvm/ADT/StringRef.h"


extern "C" {


// This macro allows us to prefix strings so that they are less likely to
// conflict with existing symbol names in the examined programs.
// e.g. CCOUNT(entry) yields CaLlCoUnTeR_entry
#define CCOUNT(X) LLVMAnalysis_##X

// The count of the number of functions is stored in a global variable inside
// the instrumented module.
extern uint64_t CCOUNT(numberOfFunctions);

// An array of information for each function ID is stored within the
// instrumented module.
extern struct {
  char* name;
  uint64_t count;
  uint64_t lineNum;
} CCOUNT(functionInfo)[];


void
CCOUNT(functionCalled)(uint64_t id) {
  ++CCOUNT(functionInfo)[id].count;
}

void
CCOUNT(setLineNum)(uint64_t id, uint64_t lineNum) {
  CCOUNT(functionInfo)[id].lineNum = lineNum;
}

// void 
// CCOUNT(setFileName)(llvm::StringRef fileName) {

// }

void
CCOUNT(printNumOfRuns)() {
  for (size_t id = 0; id < CCOUNT(numberOfFunctions); ++id) {
    auto& info = CCOUNT(functionInfo)[id];
    printf("Name: %s LineNumber: %lu Count: %lu\n", info.name, info.lineNum, info.count);
  }
}
}
