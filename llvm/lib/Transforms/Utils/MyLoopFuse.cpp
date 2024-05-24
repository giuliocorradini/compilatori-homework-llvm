#include "llvm/Transforms/Utils/MyLoopFuse.h"

using namespace llvm;

PreservedAnalyses MyLoopFuse::run(Module &M, ModuleAnalysisManager &AM) {
    return PreservedAnalyses::all();
}
