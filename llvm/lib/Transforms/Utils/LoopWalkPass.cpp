#include "llvm/Transforms/Utils/LoopWalkPass.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

void printInstruction(Instruction &I){
    outs() << "Istruzione: " << I << "\n";
}

void LoopOnBB(BasicBlock &BB){
    outs() << "Istruzioni che mi compongono \n";
    for (auto &I : BB){
        printInstruction(I);
    }
}

PreservedAnalyses llvm::LoopWalkPass::run(Loop &L, LoopAnalysisManager &LAM, LoopStandardAnalysisResults &LAR, LPMUpdater &LU){
    outs() << "Questo loop ";
    if (L.isLoopSimplifyForm())
        outs() << "è in forma semplificata \n";
    else {
        outs() << "non è in forma semplificata" << "\n";
        return PreservedAnalyses::all();
    }
    /* Dominator tree Non a lezione */
    DominatorTree &Tree = LAR.DT;
    outs() << "Stampa del dominator TREE:\n";
    Tree.print(outs());
    //Tree.viewGraph();
    for(Loop::block_iterator BI = L.block_begin(); BI != L.block_end(); ++BI){
        BasicBlock *B = *BI;
        outs() << "Sono il BB: " << B->getName() << "\n";
        LoopOnBB(*B);
        outs() << "-------------------------\n";
    }


    BasicBlock *Head = L.getHeader();
    Function *F = Head->getParent();

    for (auto Iterator = F->begin(); Iterator != F->end(); ++Iterator){
        outs() << *Iterator << "\n";
    }

    return PreservedAnalyses::all();
}
