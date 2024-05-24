#include "llvm/Transforms/Utils/MyLoopFuse.h"
#include "llvm/IR/PassManager.h"

using namespace llvm;

PreservedAnalyses MyLoopFuse::run(Function &F, FunctionAnalysisManager &AM) {
    return PreservedAnalyses::all();
}
