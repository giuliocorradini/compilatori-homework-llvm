#pragma once

#include <llvm/IR/Constants.h>
#include "llvm/IR/PassManager.h"

namespace llvm {
    class LocalOpts : public PassInfoMixin<LocalOpts> {
    public:
        PreservedAnalyses run(Module &F, ModuleAnalysisManager &AM);
    };
}