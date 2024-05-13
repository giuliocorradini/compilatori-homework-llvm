#include "llvm/IR/Argument.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Metadata.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/MyLoopInvariantCodeMotion.h"
#include <string>
#include <vector>

using namespace llvm;

bool isLoopInvariant(Instruction &Inst, Loop &L);
bool isLoopInvariant(Value *Operand, Loop &L);

void markInstruction(Instruction *Inst) {
    LLVMContext &C = Inst->getContext();
    MDNode *MDnode = MDNode::get(C, MDString::get(C, "true"));
    Inst->setMetadata("isLoopInvariant", MDnode);
}

bool isMarkedLoopInvarian(Instruction *Inst){
    MDNode *MDnode = Inst->getMetadata("isLoopInvariant");
    return (MDnode) ? true : false;
}

bool isLoopInvariant(Instruction &Inst, Loop &L){
    if (isMarkedLoopInvarian(&Inst))
        return true;
    if(!L.contains(&Inst)) { //Controllo se l'istruzione ha reaching def fuori dal loop
        markInstruction(&Inst);
        return true;
    }
    if(Inst.getOpcode() == Instruction::PHI ){
        return false;
    }
    for (User::op_iterator Operand = Inst.op_begin(); Operand != Inst.op_end(); ++Operand) {
        Value *OperandValue = *Operand;
        if (!isLoopInvariant(OperandValue,L))
            return false;
    }
    return true;
}

bool isLoopInvariant(Value *Operand, Loop &L){
    if (Instruction* OperandInst = dyn_cast<Instruction>(Operand)) {
        if (isLoopInvariant(*OperandInst, L))
            return true;
    }
    else if(dyn_cast<Constant>(Operand) || dyn_cast<Argument>(Operand))
        return true;
    return false;
}

bool doesDominateAllBlocks(BasicBlock *A, SmallVectorImpl<BasicBlock *> &ExitBlocks, DominatorTree &DT) {
    for (BasicBlock *BB : ExitBlocks) {
        if (!DT.dominates(A, BB)) {
            return false;
        }
    }
    return true;
}

void loopOnBB(BasicBlock &BB, Loop &L, bool IsDominator){
    for (auto &I : BB)
        if(isLoopInvariant(I, L)){
            outs() << "l'istruzione" << I << " è loop Invariant\n";
            //COntrollo la dominanza del blocco di appartenenza
            if (IsDominator)
                outs() << "è anche candidato per la move\n";
        }
}

PreservedAnalyses llvm::LICMyPass::run(Loop &L, LoopAnalysisManager &LAM, LoopStandardAnalysisResults &LAR, LPMUpdater &LU){
    if (!L.isLoopSimplifyForm()){
        return PreservedAnalyses::all();
    }
    SmallVector<BasicBlock*, 10> ExitBlocks;
    L.getExitBlocks(ExitBlocks);
    DominatorTree &DT = LAR.DT;
    for(Loop::block_iterator BI = L.block_begin(); BI != L.block_end(); ++BI){
        BasicBlock *B = *BI;
        bool IsDom = doesDominateAllBlocks(B, ExitBlocks, DT);
        loopOnBB(*B, L, IsDom);
    }



    return PreservedAnalyses::all();
}
