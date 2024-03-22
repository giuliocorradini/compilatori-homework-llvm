#pragma once

#include "llvm/IR/PassManager.h"

namespace llvm {
    class TestPass : public PassInfoMixin<TestPass> {
    protected:
        void analyzeFunction(Function &F);
    public:
        PreservedAnalyses run(Module &F, ModuleAnalysisManager &AM);
    };
}