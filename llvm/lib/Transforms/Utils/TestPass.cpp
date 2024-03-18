#include "llvm/Transforms/Utils/TestPass.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Instructions.h"

namespace llvm {
    PreservedAnalyses TestPass::run(Function &F, FunctionAnalysisManager &AM) {
        errs() << "Questa funzione si chiama " << F.getName() << "\n";
        errs() << "Il numero di argomenti è " << F.arg_end() - F.arg_begin() << "\n";
        
        int bbnum = 0;
        int calls = 0;
        int instructions = 0;

        for (auto const &bb: F) {
            bbnum++;
            
            for (auto &ins: bb) {
                instructions++;

                if (auto *CI = dyn_cast<CallInst>(&ins))
                    calls++;
            }
        }

        errs() << "Numero di chiamate a funzione " << calls << "\n";
        errs() << "Numero di basic blocks " << bbnum << "\n";
        errs() << "Numero di istruzioni " << instructions << "\n";

        return PreservedAnalyses::all();
    }
}