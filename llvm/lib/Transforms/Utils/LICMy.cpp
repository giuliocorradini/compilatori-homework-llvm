#include "llvm/Transforms/Utils/LICMy.h"
#include "llvm/IR/Dominators.h"
#include <vector>

using namespace llvm;
using namespace std;

bool isLoopInvariant(Instruction &Inst, Loop &L);
bool isLoopInvariant(Value *Operand, Loop &L);

void moveToPreHeader(Instruction *Inst);

void markInstruction(Instruction *Inst) {
    LLVMContext &C = Inst->getContext();
    MDNode *MDnode = MDNode::get(C, MDString::get(C, "true"));
    Inst->setMetadata("isLoopInvariant", MDnode);
}

bool isMarkedLoopInvariant(Instruction *Inst){
    MDNode *MDnode = Inst->getMetadata("isLoopInvariant");
    return (MDnode) ? true : false;
}

void isLoopInvariant(Instruction &I, Loop &L, set<Instruction *> &loop_invariants){
    if (not I.isBinaryOp() and not I.isUnaryOp())
        return;
    
    auto op1 = I.getOperand(0);
    bool op1_valid = false;
    Instruction *op1_rd = dyn_cast<Instruction>(op1);
    Argument *arg1 = dyn_cast<Argument>(op1);

    if (Constant *c = dyn_cast<Constant>(op1); c or arg1 or (op1_rd and not L.contains(op1_rd)) or loop_invariants.find(op1_rd) != loop_invariants.end())
        op1_valid = true;
    
    if (I.isUnaryOp()) {
        if (op1_valid) {
            loop_invariants.insert(&I);
            errs() << I << " is loop invariant\n";
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

/**
 * Check if BasicBlock A dominates all the exit blocks of the Loop
*/
bool dominatesAllExits(Instruction *I, SmallVectorImpl<BasicBlock *> &ExitBlocks, DominatorTree &DT) {
    /*
    for (BasicBlock *BB : ExitBlocks) {
        if (!DT.dominates(A, BB)) {
            return false;
        }
    }
    return true;
    */
    return all_of(ExitBlocks.begin(), ExitBlocks.end(), [&](BasicBlock *exit) {return DT.dominates(I, exit);});
}

bool isDeadAfterLoop(Instruction &Inst, Loop &L){
    for (auto *User : Inst.users()){
        if (Instruction *I = dyn_cast<Instruction>(User); I && !L.contains(I))
            return false;
    }
    return true;
}

void loopOnBB(BasicBlock &BB, Loop &L, SmallVector<BasicBlock *, 10> &exits, set<Instruction *> &Mov, DominatorTree &DT){
    for (auto &I : BB)
        isLoopInvariant(I, L, Mov);
    
    for (auto &I: Mov) {
        if (dominatesAllExits(I, exits, DT) || isDeadAfterLoop(*I,L) ){// oppure la roba degli usi
            outs() << "è anche candidato per la move\n";
            Mov.insert(I);
        }
    }

}

void moveToPreHeader(Instruction *Inst, BasicBlock *PreHeader){
    for (User::op_iterator Operand = Inst->op_begin(); Operand != Inst->op_end(); ++Operand) {
        Value *OperandValue = *Operand;
        if (dyn_cast<Constant>(OperandValue) || dyn_cast<Argument>(OperandValue))
            continue;
        moveToPreHeader(dyn_cast<Instruction>(OperandValue), PreHeader);
    }
    Inst->removeFromParent();
    Inst->insertInto(PreHeader, --PreHeader->end());
    //BasicBlock::iterator InsertPoint = --PreHeader->end();
    //Inst->insertBefore(&*InsertPoint);
    outs() << "inserita l'istruzione " << Inst->getNameOrAsOperand() << " nel blocco: " << PreHeader->getNameOrAsOperand() << "\n";
}


/**
 * Load e store non vengono mai marcate come loop invariant, quindi dobbiamo fare un passo ulteriore di
 * ottimizzazione con LLVM opt: mem2reg.
*/
PreservedAnalyses LICMyPass::run(Loop &L, LoopAnalysisManager &LAM, LoopStandardAnalysisResults &LAR, LPMUpdater &LU){
    if (not L.isLoopSimplifyForm()) {
        errs() << "Loop in not in simplify form. Use LoopInstSimplify to produce a canonical form.\n";
        return PreservedAnalyses::all();
    }

    errs() << "Loop " << L.getName() << "\n";

    set<Instruction *> MovableInst; //< Candidate for LICM
    SmallVector<BasicBlock*, 10> ExitBlocks;
    L.getExitBlocks(ExitBlocks);
    DominatorTree &DT = LAR.DT;

    for (auto BB: L.getBlocks()) {
        loopOnBB(*BB, L, ExitBlocks, MovableInst, DT);
    }
    

    BasicBlock *PreHeader = L.getLoopPreheader();
    errs() << "Number of movable instruction " << MovableInst.size() << "\n";
    for (Instruction *Inst : MovableInst){
        moveToPreHeader(Inst, PreHeader);
    }

    errs() << "Exits blocks\n";

    for (auto eb: ExitBlocks)
        errs() << eb->getNameOrAsOperand() << "\n";

    errs() << "Candidates:\n";

    for (auto cand: MovableInst) {
        errs() << cand->getNameOrAsOperand() << "\n";
    }

    errs() << "\n";

    return PreservedAnalyses::all();
}
