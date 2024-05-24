#pragma once

#include "llvm/IR/PassManager.h"

namespace llvm {
    class MyLoopFuse : public PassInfoMixin<MyLoopFuse> {
    public:
        PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);
    };
}
