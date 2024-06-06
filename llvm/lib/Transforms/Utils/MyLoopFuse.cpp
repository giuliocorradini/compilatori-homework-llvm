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
    if (isa<SCEVCouldNotCompute>(TripCount1) || isa<SCEVCouldNotCompute>(TripCount2)){
        errs() << "Errore !\n";
        return false;
    }
    return SE.isKnownPredicate(ICmpInst::ICMP_EQ, TripCount1, TripCount2);
}


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

std::vector<BasicBlock*> getBodyBlocks(Loop * L){
    auto BodyBlock = L->getBlocksVector();

    BodyBlock.erase(remove(BodyBlock.begin(), BodyBlock.end(), L->getHeader()), BodyBlock.end());
    if(L->getLoopLatch())
        BodyBlock.erase(remove(BodyBlock.begin(), BodyBlock.end(), L->getLoopLatch()), BodyBlock.end());
    return BodyBlock;
}

Instruction* getPhiNodeFromLatch(Loop *L) {
  BasicBlock *Header = L->getHeader();
  if (!Header) return nullptr;

  BasicBlock *Latch = L->getLoopLatch();
  if (!Latch) return nullptr;

  for (Instruction &I : *Header) {
    if (PHINode *PN = dyn_cast<PHINode>(&I)) {
      for (unsigned i = 0; i < PN->getNumIncomingValues(); ++i) {
        if (PN->getIncomingBlock(i) == Latch) {
          return PN;
        }
      }
    }
  }
  return nullptr;
}
void fuseL1andL2(Function &F, FunctionAnalysisManager &AM, Loop *L1, Loop *L2){
    // DominatorTree &DT = AM.getResult<DominatorTreeAnalysis>(F);
    auto BodyBlock2 = getBodyBlocks(L2);
    auto BodyBlock1 = getBodyBlocks(L1);
    auto *ExitBlock2 = L2->getExitBlock();



    // controllo se l'istruzione terminatrice di L2 è una istruzione di branch, la faccio puntare al Latch
    if(BranchInst *BI = dyn_cast<BranchInst>(L2->getHeader()->getTerminator())){
            ReplaceInstWithInst(BI, BranchInst::Create(L2->getLoopLatch()));
    }
    // dal body1 punto all'inizio del body2
    if(BranchInst *BI = dyn_cast<BranchInst>(BodyBlock1.back()->getTerminator())){
            ReplaceInstWithInst(BI, BranchInst::Create(BodyBlock2.front()));
    }
    if(BranchInst *BI = dyn_cast<BranchInst>(BodyBlock2.back()->getTerminator())){
            ReplaceInstWithInst(BI, BranchInst::Create(L1->getLoopLatch()));
    }
    // setto la branch del h1 per puntare a exit di l2
    if(BranchInst *BI = dyn_cast<BranchInst>(L1->getHeader()->getTerminator())){
            BI->setSuccessor(1,ExitBlock2 );
    }

    // prendere dall'header del loop 2 la variabile di iterazione

    Instruction *PhiL1 = getPhiNodeFromLatch(L1);
    Instruction *PhiL2 = getPhiNodeFromLatch(L2);

    PhiL2->replaceAllUsesWith(PhiL1);

    for (auto bb : L1->blocks())
        bb->print(errs());
    for (auto bb : L2->blocks())
        bb->print(errs());

}

// todo controllare gli usi della var iterante

PreservedAnalyses MyLoopFusePass::run(Function &F, FunctionAnalysisManager &AM) {
    LoopInfo &LI = AM.getResult<LoopAnalysis>(F);
    Loop *OldLoop = nullptr;

    bool isLoopChanged = true;
    while (isLoopChanged){
        isLoopChanged = false;
        for (auto *L : reverse(LI)){
            if (OldLoop and OldLoop != L)
            if(areLoopAdj(OldLoop, L) and L1DominatesL2(F,AM, OldLoop, L) and iterateSameTimes(F, AM, OldLoop,L) and
            isAnyInstructionNegativeDep(F, AM, OldLoop,L)){
                    isLoopChanged = true;
                    fuseL1andL2(F, AM, OldLoop,L);
                    errs() << "Fusi i due LOOP\n";
                    break;
                }
            OldLoop = L;
        }

    }

    return PreservedAnalyses::all();
}
