#pragma once

#include "llvm/IR/Instructions.h"

namespace llvm {
    void walk_UD(Instruction &I) {
        outs() << "UD chain:\n";
        for (auto usePtr = I.user_begin(); usePtr != I.user_end(); usePtr++) {
            usePtr->printAsOperand(outs());
            outs() << "\n";
        }
    }

    void walk_DU(Instruction &I) {
        outs() << "DU chain:\n";
        for (auto usePtr = I.use_begin(); usePtr != I.use_end(); usePtr++) {
            Instruction *inst = dyn_cast<Instruction>(&*usePtr);
            inst->printAsOperand(outs());
            outs() << "\n";
            outs() << "E sono usato da";
            usePtr->getUser()->print(outs());
            outs() << "\n";
        }
    }
}