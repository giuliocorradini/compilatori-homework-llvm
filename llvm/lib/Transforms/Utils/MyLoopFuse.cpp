#include "llvm/Transforms/Utils/MyLoopFuse.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/CodeGen/ValueTypes.h"
#include "llvm/IR/BasicBlock.h"
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
    /*
    outs()<< "L1 Exit: " << ExitBlock1<< "\n";
    ExitBlock1->print(outs());
    outs()<< "L2 Head: " << HeadL2<< "\n";
    HeadL2->print(outs());
    outs()<< "L2 real Head: " << L2->getHeader() << "\n";
    L2->getHeader()->print(outs());*/
    if(ExitBlock1 and ExitBlock1 == HeadL2)
        return true;
    return false;
}


PreservedAnalyses MyLoopFusePass::run(Function &F, FunctionAnalysisManager &AM) {

    LoopInfo &LI = AM.getResult<LoopAnalysis>(F);
    Loop *OldLoop = nullptr;
    for (auto *L : LI){
        /*outs() << "L[i] = " << L <<"\n";
        L->print(outs());
        outs() << "LOLD = " << OldLoop << "\n";
        if (OldLoop) OldLoop->print(outs());*/
        if (OldLoop and OldLoop != L)
            if(areLoopAdj(L, OldLoop)){
                outs()<<"Il loop("<<OldLoop<<") "<<" è adiacente a";
                outs()<<" loop("<<L<<")\n";
            }
        OldLoop = L;
    }
    return PreservedAnalyses::all();
}
