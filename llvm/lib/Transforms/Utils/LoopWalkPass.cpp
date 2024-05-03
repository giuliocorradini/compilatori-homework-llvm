#include "llvm/Transforms/Utils/LoopWalkPass.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

void printInstruction(Instruction &I){
    outs() << "Istruzione: " << I << "\n";
}

void LoopOnBB(BasicBlock &BB){
    for (auto &I : BB){
        printInstruction(I);
    }
}

PreservedAnalyses llvm::LoopWalkPass::run(Loop &L, LoopAnalysisManager &LAM, LoopStandardAnalysisResults &LAR, LPMUpdater &LU){
    outs() << "Questo loop ";
    if (L.isLoopSimplifyForm())
        outs() << "è in forma semplificata";
    else
        outs() << "non è in forma semplificata";
    outs() << "\n";
    for(Loop::block_iterator BI = L.block_begin(); BI != L.block_end(); ++BI){
        BasicBlock *B = *BI;
        outs() << "Sono il BB: " << *B << "\n";
        LoopOnBB(*B);
        outs() << "-------------------------\n";
    }
    return PreservedAnalyses::all();
}
