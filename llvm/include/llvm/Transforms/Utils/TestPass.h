#pragma once

#include "llvm/IR/PassManager.h"

namespace llvm {
    class TestPass : public PassInfoMixin<TestPass> {
    public:
        PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);
    };
}