#include "llvm/Transforms/Utils/MyLoopFuse.h"
#include "llvm/Analysis/DependenceAnalysis.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/CodeGen/ValueTypes.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/InstructionCost.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include <__algorithm/remove.h>
#include <string>

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
 * This is granted to work ONLY WITH CANONICAL LOOPS.
*/
Instruction* getPhiNodeFromLatch(Loop *L) {
  PHINode *Phi = L->getCanonicalInductionVariable();
  if (Phi)
    return Phi;

  BasicBlock *Header = L->getHeader();
  if (!Header){
      return nullptr;
  }
  Header->print(errs());

  BasicBlock *Latch = L->getLoopLatch();
  if (!Latch){
      return nullptr;
  }
  Latch->print(errs());

  // TODO: da sistemare, non prende la variabile che nel latch fa la somma, ma una che semplicemente arriva dal latch
  for (Instruction &I : *Header) {
      if (PHINode *PN = dyn_cast<PHINode>(&I)) {
          for (unsigned int i = 0; i < PN->getNumIncomingValues(); ++i) {
              if (PN->getIncomingBlock(i) == Latch) {
                  // Controllo se il PHINode è incrementato nel Latch
                  Value *IncomingValue = PN->getIncomingValue(i);
                  if (Instruction *ADDI = dyn_cast<Instruction>(IncomingValue)){
                      if( ADDI->getOpcode() == Instruction::Add){ // Controllo che la variabile di incremento provenga dal latch e abbia come operando la phinode davvero
                          if ((ADDI->getOperand(0) == PN or ADDI->getOperand(1) == PN) and ADDI->getParent() == Latch){
                              errs() << "Variabile di incremento: " << PN->getNameOrAsOperand() << "\n";
                              return PN;
                          }
                      }
                  }
              }
          }
      }
  }
  return nullptr;

}

bool isForLoop(Loop* L){
    return true;
}

void moveUsefulInstr(Loop *L1, Loop *L2, Instruction *PhiL2){
    for (Instruction &I : *L2->getHeader()) {
        if (PHINode *PN = dyn_cast<PHINode>(&I)) {
            errs() << "Phinode nell'header: " << PN->getNameOrAsOperand() << "\n";
            for (unsigned int i = 0; i < PN->getNumIncomingValues(); ++i) {
                if (PN->getIncomingBlock(i) == L2->getLoopLatch() and PN != PhiL2) { //se non è il phinode che contiene la variabile di iterazione

                    PHINode *NewPN = PHINode::Create(PN->getType(), PN->getNumIncomingValues(), "moved"+PN->getName() , &*L1->getHeader()->getFirstInsertionPt());
                    for (unsigned int j = 0; j < PN->getNumIncomingValues(); ++j) {
                        BasicBlock *IncomingBlock = PN->getIncomingBlock(j);
                        Value *IncomingValue = PN->getIncomingValue(j);
                        if (IncomingBlock == L2->getLoopPredecessor() or IncomingBlock == L2->getLoopPreheader()){
                            IncomingBlock = L1->getLoopPredecessor();
                            errs()<<"LOOP PREDECESSOR: "<< L1->getLoopPredecessor()->getNameOrAsOperand() <<"\n";
                        }
                        else if (IncomingBlock == L2->getLoopLatch()) {
                            IncomingBlock = L1->getLoopLatch();  // Sostituisci il latch di L2 con il latch di L1
                        }

                        NewPN->addIncoming(IncomingValue, IncomingBlock); // il valore non è cambiato, cambio i blocchi predecessori
                        //se il predBlock era il latch del loop2 allora diventa il latch del loop1
                        // se il predBlock era il predecessore del loop2 allora diventa il predecessore del loop1
                    }
                    L1->getHeader()->print(errs());
                    PN->replaceAllUsesWith(NewPN);

                }
            }
        }
    }
}

/**
 * Here is where fusion between two loops happen.
*/
void fuseL1andL2(Function &F, FunctionAnalysisManager &AM, Loop *L1, Loop *L2){
    LoopInfo &LI = AM.getResult<LoopAnalysis>(F);

    Instruction *PhiL1 = getPhiNodeFromLatch(L1);
    Instruction *PhiL2 = getPhiNodeFromLatch(L2);

    if (!PhiL1 or !PhiL2)
        return;

    // DO WIHLE -> head non presente / corrisponde al primo blocco del body
    // il latch fa la conditional branch -> head fa una unconditional branch
    // SOLUZIONE: Dal loop 2 tolgo l'incremento della phi e anche la phi stessa dopodichè posso mergare
    // COME CONTROLLO CHE SIA FOR O WHILE ->

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



    PhiL2->replaceAllUsesWith(PhiL1);

    //TODO: Muovere tutte le phi che sono presenti nell'header tranne quella di incremento, potrebbero servire
    moveUsefulInstr(L1, L2, PhiL2);

    //TODO : gestire loop innestati, questo non funziona perchè fanno parte già del loop innestato
    LI.erase(L2);
    for (BasicBlock *BB : BodyBlock2)
        L1->addBasicBlockToLoop(BB, LI);
}

//TODO : gestire loop innestati

PreservedAnalyses MyLoopFusePass::run(Function &F, FunctionAnalysisManager &AM) {
    LoopInfo &LI = AM.getResult<LoopAnalysis>(F);
    Loop *OldLoop = nullptr;
    bool isLoopChanged;
    do{
        isLoopChanged = false;
        for (auto *L : reverse(LI)){ //iterate on all CFG loops
            L->print(errs());
            if (OldLoop and OldLoop != L){
                //if loops are adjacent, L1 dominates L2 and they iterate the same number of times...
                //negative dependency is still a dummy check, actually not working
                if(areLoopAdj(OldLoop, L) and L1DominatesL2(F,AM, OldLoop, L) and
                    iterateSameTimes(F, AM, OldLoop, L) and isAnyInstructionNegativeDep(F, AM, OldLoop,L)){
                        errs()<<"inizio fusione dei loop\n";
                        isLoopChanged = true;
                        fuseL1andL2(F, AM, OldLoop, L);
                        errs() << "Fusi i due LOOP\n";
                        break;
                    }
            }
            OldLoop = L;
        }
    }while(isLoopChanged);

    return PreservedAnalyses::all();
}
