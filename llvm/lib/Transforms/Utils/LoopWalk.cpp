#include "llvm/Transforms/Utils/LoopWalk.h"


PreservedAnalyses LoopWalkPass::run(Loop &L, LoopAnalysisManager &LAM, LoopStandardAnalysisResults &LAR, LPMUpdater &LU) {
    errs() << "This loop is " << (L.isLoopSimplifyForm() ? "" : "not ") << "in simplify form\n";

    errs() << "The preheader: " << L.getLoopPreheader()->getNameOrAsOperand() << "\n";
    errs() << "The header: " << L.getHeader()->getNameOrAsOperand() << "\n";

    errs() << "Basic blocks:\n";

    for (auto BB: L.getBlocks()) {
        errs() << BB->getNameOrAsOperand() << "\n";
    }

    if (L.getExitBlock())
        errs() << "The exit block is: " << L.getExitBlock()->getNameOrAsOperand() << "\n";
    else
        errs() << "Multiple exit blocks\n";

    return PreservedAnalyses::all();
}