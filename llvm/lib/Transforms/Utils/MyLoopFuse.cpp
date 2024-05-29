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

bool iterateSameTimes(Function &F, FunctionAnalysisManager &AM, Loop *L1, Loop *L2){
    ScalarEvolution &SE = AM.getResult<ScalarEvolutionAnalysis>(F);
    const SCEV *TripCount1 = SE.getBackedgeTakenCount(L1);
    const SCEV *TripCount2 = SE.getBackedgeTakenCount(L2);
    errs() << "Numero di iterazione di L1: ";
    TripCount1->print(errs());
    errs() << "\nNumero di iterazione di L2: ";
    TripCount2->print(errs());
    errs() <<"\n";
    if (isa<SCEVCouldNotCompute>(TripCount1) || isa<SCEVCouldNotCompute>(TripCount2))
        return false;
    return SE.isKnownPredicate(ICmpInst::ICMP_EQ, TripCount1, TripCount2);
}


PreservedAnalyses MyLoopFusePass::run(Function &F, FunctionAnalysisManager &AM) {
    LoopInfo &LI = AM.getResult<LoopAnalysis>(F);
    Loop *OldLoop = nullptr;
    for (auto *L : reverse(LI)){
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
