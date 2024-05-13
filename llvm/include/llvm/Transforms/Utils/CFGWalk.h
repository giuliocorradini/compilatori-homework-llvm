#pragma once

#include <llvm/IR/Constants.h>
#include "llvm/IR/PassManager.h"

namespace llvm {
    class CFGWalk : public PassInfoMixin<CFGWalk> {
    public:
        PreservedAnalyses run(Module &F, ModuleAnalysisManager &AM);
    };
}
