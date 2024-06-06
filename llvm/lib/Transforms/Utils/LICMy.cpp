#include "llvm/Transforms/Utils/LICMy.h"
#include "llvm/IR/Dominators.h"
#include <vector>

using namespace llvm;
using namespace std;

/** To mark an instruction as Loop Invariant, we need to check:
 * - if the instr. is either unary or binary
 * -
*/
static void addIfLoopInvariant(Instruction &I, Loop &L, set<Instruction *> &loop_invariants){
    if (not I.isBinaryOp() and not I.isUnaryOp())
        return;

    auto op1 = I.getOperand(0);
    bool op1_valid = false;

    Instruction *op1_rd = dyn_cast<Instruction>(op1);
    Argument *arg1 = dyn_cast<Argument>(op1);

    /**
     * Check if the operand is a constant, an argument,
     * or an instruction and as such its definition is outside of the loop,
     * or if

    if (Constant *c = dyn_cast<Constant>(op1); c or arg1 or (op1_rd and not L.contains(op1_rd)) or loop_invariants.find(op1_rd) != loop_invariants.end())
        op1_valid = true;

    if (I.isUnaryOp()) {
        if (op1_valid) {
            loop_invariants.insert(&I);
            errs() << I << " is loop invariant\n";
        }
    } else { //if binary instruction, also check second operand
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
static bool dominatesAllExits(Instruction *I, SmallVectorImpl<BasicBlock *> &ExitBlocks, DominatorTree &DT) {
    return all_of(ExitBlocks.begin(), ExitBlocks.end(), [&](BasicBlock *exit) {return DT.dominates(I, exit);});
}

/**
 * A value is dead after loop iff it has no users outside the loop. Equivalent to:
 * all of its users are inside the loop.
*/
static bool isDeadAfterLoop(Instruction &Inst, Loop &L){
    return none_of(Inst.user_begin(), Inst.user_end(), [&](Value *UserAsValue) {
        Instruction *User = dyn_cast<Instruction>(UserAsValue);
        return User and not L.contains(User);   //not L.contains check if User is outside the loop
    });
}

/**
 * Given a BasicBlock and a set of loop invariant instructions, populates the set with loop invariants
 * for this basic block.
*/
static void findLoopInvariants(BasicBlock &BB, Loop &L, set<Instruction *> &LI){
    for (auto &I : BB)
        addIfLoopInvariant(I, L, LI);
}

/**
 * Given a set of LoopInvariants Instructions, return a set of Movable instruction (LICM candidates).
*/
static set<Instruction *> filterMovable(Loop &L, SmallVector<BasicBlock *, 10> &exits, set<Instruction *> &LoopInvariants, DominatorTree &DT){
    set<Instruction *> Movable;

    for (auto &I: LoopInvariants) {
        if (dominatesAllExits(I, exits, DT) || isDeadAfterLoop(*I,L) ) {
            errs() << I->getNameOrAsOperand() << " is candidate for move\n";
            Movable.insert(I);
        }
    }

    return Movable;
}

/**
 * Recursive implementation of DFS to move instruction to preheader in the correct order.
 * Usees of an instruction need to be moved BEFORE the instruction itself to preserve correct functionality.
*/
static void moveToPreHeader(Instruction *Inst, BasicBlock *PreHeader){
    for (User::op_iterator Operand = Inst->op_begin(); Operand != Inst->op_end(); ++Operand) {
        Value *OperandValue = *Operand;

        //skipping recursive visit of constants and function arguments
        if (dyn_cast<Constant>(OperandValue) || dyn_cast<Argument>(OperandValue))
            continue;

        moveToPreHeader(dyn_cast<Instruction>(OperandValue), PreHeader);
    }

    //remove from loop and move to the bottom of preheader BB
    Inst->removeFromParent();
    Inst->insertInto(PreHeader, --PreHeader->end());

    errs() << "Moved " << Inst->getNameOrAsOperand() << " into basic block " << PreHeader->getNameOrAsOperand() << "\n";
}


/**
 * Load e store are never marked as loop invariants, this makes every instruction virtually loop VARIANT. To enable
 * this optimization the user must add a mem2reg pass to its pipeline.
*/
PreservedAnalyses LICMyPass::run(Loop &L, LoopAnalysisManager &LAM, LoopStandardAnalysisResults &LAR, LPMUpdater &LU){
    if (not L.isLoopSimplifyForm()) {
        errs() << "Loop in not in simplify form. Use LoopInstSimplify to produce a canonical form.\n";
        return PreservedAnalyses::all();
    }

    errs() << "Loop " << L.getName() << "\n"; //this is a LoopPass so it is called by pass manager on every loop

    set<Instruction *> LoopInvariants; //< Candidates for LICM
    SmallVector<BasicBlock*, 10> ExitBlocks; //fallbacks to std::vector once the dimension increases to a certain extent
    L.getExitBlocks(ExitBlocks); //n.b this is different from getExitingBlocks
    DominatorTree &DT = LAR.DT;

    for (auto BB: L.getBlocks()) { //on every block of the loop...
        findLoopInvariants(*BB, L, LoopInvariants);
    }

    //obtain set of candidates for LICM
    set<Instruction *> MovableInst = filterMovable(L, ExitBlocks, LoopInvariants, DT);

    /**
     * We need a preheader to which insert the moved instructions into.
     * All loops expressed in Loop Simplify Form have a preheader.
     * The LoopSimplify pass is automatically called by pass manager when scheduling a LoopPass.
    */
    BasicBlock *PreHeader = L.getLoopPreheader();

    errs() << "Number of movable instructions " << MovableInst.size() << "\n";
    for (Instruction *Inst : MovableInst){
        moveToPreHeader(Inst, PreHeader); //moving all loop-motion candidates to preheader
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
