#include "llvm/Transforms/Utils/MyLoopFuse.h"
#include "llvm/Analysis/DependenceAnalysis.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/CodeGen/ValueTypes.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Support/InstructionCost.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include <__algorithm/remove.h>

using namespace llvm;


/**
 * Check if 2 given loops are adjacent.
 * Formally, 2 loops are adjacent if there is no other BB between them in the Control Flow Graph.
*/
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
/**
 * Check if the first loop dominates the second, and viceversa if the second loop post-dominates the first.
 * Necessary condition to determine whether two loops are control flow equivalent and, as such,
 * it's guaranteed that if the first loopexecutes, also the second will.
*/
bool L1DominatesL2(Function &F, FunctionAnalysisManager &AM, Loop *L1, Loop *L2){
    DominatorTree &DT = AM.getResult<DominatorTreeAnalysis>(F);
    PostDominatorTree &PDT = AM.getResult<PostDominatorTreeAnalysis>(F);
    BasicBlock *H1 = L1->getHeader();
    BasicBlock *H2 = L2->getHeader();
    return DT.dominates(H1,H2) and PDT.dominates(H2, H1);

}

/** Check whether 2 given loops have identical trip count (they iterate the same number of times).
 * For that, scalar evolution analysis is required: it detects changes
 * in the value of scalar variables present in the loop.
*/
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
    if (isa<SCEVCouldNotCompute>(TripCount1) || isa<SCEVCouldNotCompute>(TripCount2)){
        errs() << "Errore !\n";
        return false;
    }
    return SE.isKnownPredicate(ICmpInst::ICMP_EQ, TripCount1, TripCount2);
}

/**
 * WIP: still not found a way to implement negative dependency check between two instructions
 * This function is basically always returning true.
*/
bool isAnyInstructionNegativeDep(Function &F, FunctionAnalysisManager &AM, Loop *L1, Loop *L2){
    DependenceInfo &DI = AM.getResult<DependenceAnalysis>(F);
    for (auto *BBL1 : L1->getBlocks()){
        for(auto &I1 : *BBL1){
            for (auto *BBL2 : L2->getBlocks()){
                for(auto &I2 : *BBL2){
                    auto DepResult = DI.depends(&I2, &I1, true);

                    if (DepResult and DepResult->isDirectionNegative()) {/*and isBlockingDependence(DepResult.get())){
                        errs() <<"resultato ddi is Negative: "<<DepResult->isDirectionNegative() << "\n";
                        errs() << "l'istruzione " << I2.getNameOrAsOperand() << " dipende da " << I1.getNameOrAsOperand() << "\n";

                        errs() << "Dipendenza SRC: " << DepResult->getSrc()->getNameOrAsOperand() << " DEST:" << DepResult->getDst()->getNameOrAsOperand() << "\n";
                        DepResult->dump(errs());*/
                        return false;
                        // return if
                    }
                }
            }
        }
    }
    return true;
}

/**
 * Extract blocks composing the body of a loop from the entire loop itself,
 * by discarding latch, header and preheader and keeping the leftovers.
*/
std::vector<BasicBlock*> getBodyBlocks(Loop * L){

    //get all loop blocks and place them in an array
    auto BodyBlock = L->getBlocksVector(); //preheader is already left out

    //remove header from list of BBs
    BodyBlock.erase(remove(BodyBlock.begin(), BodyBlock.end(), L->getHeader()), BodyBlock.end());

    //remove latch from list of BBs
    if(L->getLoopLatch())
        BodyBlock.erase(remove(BodyBlock.begin(), BodyBlock.end(), L->getLoopLatch()), BodyBlock.end());
    return BodyBlock;
}

/**
 * This function acts as a double-check in getting the correct phi node (induction variable for the loop)
 * from a loop. It compares the phi node obtained from loop header to phi node gotten from the loop latch.
 * They should always be the same.
 * This function acts like llvm::Loop->getCanonicalInductionVariable().
 * This is granted to work ONLY WITH FOR LOOPS.
*/
Instruction* getPhiNodeFromLatch(Loop *L) {
  BasicBlock *Header = L->getHeader();
  if (!Header) return nullptr;

  BasicBlock *Latch = L->getLoopLatch();
  if (!Latch) return nullptr;

  for (Instruction &I : *Header) {
    if (PHINode *PN = dyn_cast<PHINode>(&I)) {
      for (unsigned int i = 0; i < PN->getNumIncomingValues(); ++i) {
        if (PN->getIncomingBlock(i) == Latch) {
          return PN;
        }
      }
    }
  }
  return nullptr;
}


/**
 * Here is where fusion between two loops happen.
*/
void fuseL1andL2(Function &F, FunctionAnalysisManager &AM, Loop *L1, Loop *L2){
    LoopInfo &LI = AM.getResult<LoopAnalysis>(F);
    auto BodyBlock2 = getBodyBlocks(L2);
    auto BodyBlock1 = getBodyBlocks(L1);
    auto *ExitBlock2 = L2->getExitBlock();


    // check if terminator instruction of L2 is a branch, in that case make that point to L2's latch
    if(BranchInst *BI = dyn_cast<BranchInst>(L2->getHeader()->getTerminator())){
        ReplaceInstWithInst(BI, BranchInst::Create(L2->getLoopLatch()));
    }
    // body1 -> body2
    if(BranchInst *BI = dyn_cast<BranchInst>(BodyBlock1.back()->getTerminator())){
        //BI->setSuccessor(0, BodyBlock2.front());
        ReplaceInstWithInst(BI, BranchInst::Create(BodyBlock2.front()));
    }
    // body2 (now merged with body1) -> latch1
    if(BranchInst *BI = dyn_cast<BranchInst>(BodyBlock2.back()->getTerminator())){
        //BI->setSuccessor(0, L1->getLoopLatch());
        ReplaceInstWithInst(BI, BranchInst::Create(L1->getLoopLatch()));
    }
    // branch_header1 -> exit_L2 instead of -> exit_L1 (now dead code)
    if(BranchInst *BI = dyn_cast<BranchInst>(L1->getHeader()->getTerminator())){
            BI->setSuccessor(1,ExitBlock2 );
    }

    //replacing uses of induction variable in body2 (now linked to body1) with the one from body1

    Instruction *PhiL1 = getPhiNodeFromLatch(L1);
    Instruction *PhiL2 = getPhiNodeFromLatch(L2);

    PhiL2->replaceAllUsesWith(PhiL1);

    LI.erase(L2);
    for (BasicBlock *BB : BodyBlock2)
        L1->addBasicBlockToLoop(BB, LI);

}


PreservedAnalyses MyLoopFusePass::run(Function &F, FunctionAnalysisManager &AM) {
    LoopInfo &LI = AM.getResult<LoopAnalysis>(F);
    Loop *OldLoop = nullptr;
    bool isLoopChanged = true;
    while (isLoopChanged){
        isLoopChanged = false;
        for (auto *L : reverse(LI)){ //iterate on all CFG loops
            if (OldLoop and OldLoop != L){
                //if loops are adjacent, L1 dominates L2 and they iterate the same number of times...
                //negative dependency is still a dummy check, actually not working
                if(areLoopAdj(OldLoop, L) and L1DominatesL2(F,AM, OldLoop, L) and
                    iterateSameTimes(F, AM, OldLoop, L) and isAnyInstructionNegativeDep(F, AM, OldLoop,L)){
                        isLoopChanged = true;
                        fuseL1andL2(F, AM, OldLoop, L);
                        errs() << "Fusi i due LOOP\n";
                        break;
                    }
            }
            OldLoop = L;
        }
    }

    return PreservedAnalyses::all();
}
