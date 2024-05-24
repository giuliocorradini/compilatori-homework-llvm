#pragma once

#include "llvm/IR/PassManager.h"

namespace llvm {
    class MyLoopFuse : public PassInfoMixin<MyLoopFuse> {
    public:
        PreservedAnalyses run(Module &F, ModuleAnalysisManager &AM);
    };
}
