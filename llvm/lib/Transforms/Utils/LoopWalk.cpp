#include "llvm/Transforms/Utils/LoopWalk.h"


PreservedAnalyses LoopWalkPass::run(Loop &L, LoopAnalysisManager &LAM, LoopStandardAnalysisResults &LAR, LPMUpdater &LU) {
    errs() << "This loop is " << (L.isLoopSimplifyForm() ? "" : "not ") << "in simplify form\n";

    errs() << "The preheader: " << L.getLoopPreheader()->getName() << "\n";
    errs() << "The header: " << L.getHeader()->getName() << "\n";

    errs() << "Basic blocks:\n";

    for (auto BB: L.getBlocks()) {
        errs() << BB->getName() << "\n";
    }

    errs() << "The exit block is: " << L.getExitBlock()->getName() << "\n";

    return PreservedAnalyses::all();
}