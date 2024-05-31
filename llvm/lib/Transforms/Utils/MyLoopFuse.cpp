#include "llvm/Transforms/Utils/MyLoopFuse.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/CodeGen/ValueTypes.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Analysis/ScalarEvolution.h"


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
    errs() << "Dominanza: " << DT.dominates(H1, H2) << " reverse: " << PDT.dominates(H2, H1) << "\n";

    return DT.dominates(H1,H2) and PDT.dominates(H2, H1);
}

bool iterateSameTimes(Function &F, FunctionAnalysisManager &AM, Loop *L1, Loop *L2) {
    ScalarEvolution &SE = AM.getResult<ScalarEvolutionAnalysis>(F);

    const SCEV *L1_execs = SE.getBackedgeTakenCount(L1);
    const SCEV *L2_execs = SE.getBackedgeTakenCount(L2);

    if (isa<SCEVCouldNotCompute>(L1_execs) or isa<SCEVCouldNotCompute>(L2_execs))
        return false;

    errs() << "L1 trip count ";
    L1_execs->print(errs());
    
    errs() << "L2 trip count ";
    L2_execs->print(errs());

    
    return SE.isKnownPredicate(ICmpInst::ICMP_EQ, L1_execs, L2_execs);
}


PreservedAnalyses MyLoopFusePass::run(Function &F, FunctionAnalysisManager &AM) {

    errs() << "Welcome to loop fusion\n";

    LoopInfo &LI = AM.getResult<LoopAnalysis>(F);

    Loop *OldLoop = nullptr;
    for (auto *L : reverse(LI)){
        errs() << "Analizzo " << L->getLoopPreheader()->getNameOrAsOperand() << "\n";
        if (OldLoop and OldLoop != L) {
            if(areLoopAdj(OldLoop, L) && L1DominatesL2(F,AM, OldLoop, L) && iterateSameTimes(F, AM, OldLoop, L)){
                errs()<<"Il loop("<<OldLoop<<") "<<" è adiacente a";
                errs()<<" loop("<<L<<")\n";
                errs() << "E lo domina pure \n";
                errs() << "Hanno lo stesso number di iterazioni\n";
            }
        }
        OldLoop = L;
    }
    return PreservedAnalyses::all();
}
