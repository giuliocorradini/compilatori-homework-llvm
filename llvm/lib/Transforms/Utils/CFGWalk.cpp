#include "llvm/Transforms/Utils/CFGWalk.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instructions.h"
#include <map>
#include <optional>
#include <string>
using namespace std;

using namespace llvm;

PreservedAnalyses CFGWalk::run(Module &M, ModuleAnalysisManager &AM) {
  return PreservedAnalyses::all();
}
