#include "llvm/Transforms/Utils/MyLoopFuse.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/CodeGen/ValueTypes.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/PassManager.h"

using namespace llvm;

/*not used, maybe remove? */
llvm::BasicBlock* getFirstSuccessor(llvm::BasicBlock *BB) {
    if (!BB->getTerminator()) {
        return nullptr;
    }
    llvm::Instruction *Terminator = BB->getTerminator();
    if (Terminator->getNumSuccessors() == 0) {
        return nullptr;
    }
    return Terminator->getSuccessor(0);
}
/*Check if 2 given loops are adjacent.
  Formally, 2 loops are adjacent if there is no other BB between them in the Control Flow Graph.*/
bool areLoopAdj(Loop *L1, Loop *L2){
    BasicBlock *HeadL2 = L2->getLoopPreheader();
    //check if second loop has a guard, in that case loops are adj if the exitBB of L1 and the guard are adj
    if (L2->isGuarded())
        HeadL2 = L2->getLoopGuardBranch()->getParent();
    BasicBlock *ExitBlock1 = L1->getExitBlock();
    if(ExitBlock1 and ExitBlock1 == HeadL2)
        return true;
    return false;
}
/*Check if the first loop dominates the second, and viceversa if the second loop post-dominates the first.
  Necessary condition to determine whether two loops are control flow equivalent and, as such,
  it's guaranteed that if the first loopexecutes, also the second will.*/
bool L1DominatesL2(Function &F, FunctionAnalysisManager &AM, Loop *L1, Loop *L2){
    DominatorTree &DT = AM.getResult<DominatorTreeAnalysis>(F);
    PostDominatorTree &PDT = AM.getResult<PostDominatorTreeAnalysis>(F);
    BasicBlock *H1 = L1->getHeader();
    BasicBlock *H2 = L2->getHeader();
    return DT.dominates(H1,H2) and PDT.dominates(H2, H1);

}

/*Check whether 2 given loops have identical trip count (they iterate the same number of times).
  For that, scalar evolution analysis is required: it detects changes
  in the value of scalar variables present in the loop.*/
bool iterateSameTimes(Function &F, FunctionAnalysisManager &AM, Loop *L1, Loop *L2){
    ScalarEvolution &SE = AM.getResult<ScalarEvolutionAnalysis>(F);
    const SCEV *TripCount1 = SE.getBackedgeTakenCount(L1);
    const SCEV *TripCount2 = SE.getBackedgeTakenCount(L2);
    errs() << "Numero di iterazione di L1: ";
    TripCount1->print(errs());
    errs() << "\nNumero di iterazione di L2: ";
    TripCount2->print(errs());
    errs() <<"\n";

    //returning false if at least one of the loops has a non predictable trip count: loop fusion will be discarded
    if (isa<SCEVCouldNotCompute>(TripCount1) || isa<SCEVCouldNotCompute>(TripCount2))
        return false;
    return SE.isKnownPredicate(ICmpInst::ICMP_EQ, TripCount1, TripCount2);
}


PreservedAnalyses MyLoopFusePass::run(Function &F, FunctionAnalysisManager &AM) {
    LoopInfo &LI = AM.getResult<LoopAnalysis>(F);
    Loop *OldLoop = nullptr;
    for (auto *L : reverse(LI)){ //actually scrolls through all available loops in CFG-like order
        if (OldLoop and OldLoop != L)
        if(areLoopAdj(OldLoop, L) and L1DominatesL2(F,AM, OldLoop, L) and iterateSameTimes(F, AM, OldLoop,L)){
                errs()<<"Il loop("<<OldLoop<<") "<<" è adiacente a"<<" loop("<<L<<")\n";
                errs() << "E lo domina pure \n";
                errs() << "Hanno lo stesso numero di iterazioni \n";
            }
        OldLoop = L;
    }
    return PreservedAnalyses::all();
}
