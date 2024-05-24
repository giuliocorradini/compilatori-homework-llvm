#include "llvm/Transforms/Utils/MyLoopFuse.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/PassManager.h"

using namespace llvm;

PreservedAnalyses MyLoopFusePass::run(Function &F, FunctionAnalysisManager &AM) {

    LoopInfo &LI = AM.getResult<LoopAnalysis>(F);
    for (auto *L : LI){
            outs() << L << "\n";
    }
    return PreservedAnalyses::all();
}
