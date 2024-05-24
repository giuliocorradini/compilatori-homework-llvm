#pragma once

#include "llvm/IR/PassManager.h"

namespace llvm {
    class MyLoopFusePass : public PassInfoMixin<MyLoopFusePass> {
    public:
        PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);
    };
}
