#include "llvm/Transforms/Utils/MyLoopFuse.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/CodeGen/ValueTypes.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Support/raw_ostream.h"
#include <MacTypes.h>

using namespace llvm;

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

bool areLoopAdj(Loop *L1, Loop *L2){
    BasicBlock *HeadL2 = L2->getLoopPreheader();
    if (L2->isGuarded())    //Controllo se L2 è guarded
        HeadL2 = L2->getLoopGuardBranch()->getParent();
    BasicBlock *ExitBlock1 = L1->getExitBlock();
    if(ExitBlock1 and ExitBlock1 == HeadL2)
        return true;
    return false;
}

bool L1DominatesL2(Function &F, FunctionAnalysisManager &AM, Loop *L1, Loop *L2){
    DominatorTree &DT = AM.getResult<DominatorTreeAnalysis>(F);
    PostDominatorTree &PDT = AM.getResult<PostDominatorTreeAnalysis>(F);
    BasicBlock *H1 = L1->getHeader();
    BasicBlock *H2 = L2->getHeader();
    return DT.dominates(H1,H2) and PDT.dominates(H2, H1);

}


PreservedAnalyses MyLoopFusePass::run(Function &F, FunctionAnalysisManager &AM) {

    LoopInfo &LI = AM.getResult<LoopAnalysis>(F);

    Loop *OldLoop = nullptr;
    for (auto *L : LI.getLoopsInReverseSiblingPreorder()){
        if (OldLoop and OldLoop != L)
        if(areLoopAdj(L, OldLoop) && L1DominatesL2(F,AM, L, OldLoop)){
                outs()<<"Il loop("<<OldLoop<<") "<<" è adiacente a";
                outs()<<" loop("<<L<<")\n";
                outs() << "E lo domina pure \n";
            }
        OldLoop = L;
    }
    return PreservedAnalyses::all();
}
