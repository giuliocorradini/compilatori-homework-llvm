#include "llvm/Transforms/Utils/LICMy.h"
#include <vector>

using namespace llvm;
using namespace std;

/**
 * Load e store non vengono mai marcate come loop invariant, quindi dobbiamo fare un passo ulteriore di
 * ottimizzazione con LLVM opt: mem2reg.
*/

PreservedAnalyses LICMyPass::run(Loop &L, LoopAnalysisManager &LAM, LoopStandardAnalysisResults &LAR, LPMUpdater &LU) {
    if (not L.isLoopSimplifyForm()) {
        errs() << "Loop is not in simplify form\n";
        return PreservedAnalyses::none();
    }

    set<Instruction *> loop_invariants = {};

    //  Reaching definitions for each loop block
    for (auto BB: L.getBlocks()) {
        errs() << "Block: " << BB->getNameOrAsOperand() << "\n";
        for (auto &I: *BB) {
            if (not I.isBinaryOp() and not I.isUnaryOp())
                continue;
            
            errs() << I << "\n";
            auto op1 = I.getOperand(0);
            bool op1_valid = false;
            Instruction *op1_rd = dyn_cast<Instruction>(op1);
            Argument *arg1 = dyn_cast<Argument>(op1);

            if (Constant *c = dyn_cast<Constant>(op1); c or arg1 or (op1_rd and not L.contains(op1_rd)) or loop_invariants.find(op1_rd) != loop_invariants.end())
                op1_valid = true;
            
            if (I.isUnaryOp()) {
                if (op1_valid) {
                    loop_invariants.insert(&I);
                }
            } else {
                auto op2 = I.getOperand(1);
                bool op2_valid = false;
                Instruction *op2_rd = dyn_cast<Instruction>(op2);
                Argument *arg2 = dyn_cast<Argument>(op2);

                if (Constant *c = dyn_cast<Constant>(op2); c or arg2 or (op2_rd and not L.contains(op2_rd)) or loop_invariants.find(op2_rd) != loop_invariants.end())
                    op2_valid = true;

                if (op1_valid and op2_valid) {
                    errs() << I << " is loop invariant\n";
                    loop_invariants.insert(&I);
                }
            }
        }
    }
    
    errs() << "Candidate loop invariant:\n";
    for (auto li: loop_invariants) {
        errs() << li->getNameOrAsOperand() << "\n";
    }

    errs() << "The preheader: " << L.getLoopPreheader()->getNameOrAsOperand() << "\n";
    errs() << "The header: " << L.getHeader()->getNameOrAsOperand() << "\n";

    errs() << "Basic blocks:\n";

    for (auto BB: L.getBlocks()) {
        errs() << BB->getNameOrAsOperand() << "\n";
    }

    if (L.getExitBlock())
        errs() << "The exit block is: " << L.getExitBlock()->getNameOrAsOperand() << "\n";
    else
        errs() << "Multiple exit blocks\n";


    // Code motion candidates

    errs() << "\n\n";

    return PreservedAnalyses::all();
}