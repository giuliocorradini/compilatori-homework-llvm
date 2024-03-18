#include "llvm/Transforms/Utils/TestPass.h"
#include "llvm/IR/PassManager.h"

namespace llvm {
    PreservedAnalyses TestPass::run(Function &F, FunctionAnalysisManager &AM) {
        errs() << "Questa funzione si chiama " << F.getName() << "\n";
        errs() << "Il numero di argomenti è " << F.arg_end() - F.arg_begin() << "\n";
        return PreservedAnalyses::all();
    }
}