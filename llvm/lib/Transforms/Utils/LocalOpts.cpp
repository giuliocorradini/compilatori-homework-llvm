//===-- LocalOpts.cpp - Example Transformations --------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/Transforms/Utils/LocalOpts.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstrTypes.h"
// L'include seguente va in LocalOpts.h
#include <llvm/IR/Constants.h>

using namespace llvm;


bool algebraicIdentity(llvm::Instruction &I){
    Value *Op1=I.getOperand(0);
    Value *Op2=I.getOperand(1);
    if(BinaryOperator::Mul == I.getOpcode()){
      //if first operand is constant and 1 and second operand is not a constant, or viceversa...
      if(ConstantInt *C = dyn_cast<ConstantInt>(Op1); C && C->getValue().isOne() && not dyn_cast<ConstantInt>(Op2)){ 
        I.replaceAllUsesWith(Op2);
      } else if(ConstantInt *C = dyn_cast<ConstantInt>(Op2); C && C->getValue().isOne() && not dyn_cast<ConstantInt>(Op1)) {
        I.replaceAllUsesWith(Op1);
      } else {
        return false;
      }
      return true;
    }
    else if(BinaryOperator::Add == I.getOpcode()){
      //if first operand is constant and 0 and second operand is not a constant, or viceversa...
      if(ConstantInt *C = dyn_cast<ConstantInt>(Op1); C && C->getValue().isZero() && not dyn_cast<ConstantInt>(Op2)){ 
        I.replaceAllUsesWith(Op2);
      } else if(ConstantInt *C = dyn_cast<ConstantInt>(Op2); C && C->getValue().isZero() && not dyn_cast<ConstantInt>(Op1)) {
        I.replaceAllUsesWith(Op1);
      } else {
        return false;
      }
      return true;
    }
    return false;
}

bool runOnBasicBlock(BasicBlock &B) {

  /**
    
    // Preleviamo le prime due istruzioni del BB
    Instruction &Inst1st = *B.begin(), &Inst2nd = *(++B.begin());

    // L'indirizzo della prima istruzione deve essere uguale a quello del 
    // primo operando della seconda istruzione (per costruzione dell'esempio)
    assert(&Inst1st == Inst2nd.getOperand(0));

    // Stampa la prima istruzione
    outs() << "PRIMA ISTRUZIONE: " << Inst1st << "\n";
    // Stampa la prima istruzione come operando
    outs() << "COME OPERANDO: ";
    Inst1st.printAsOperand(outs(), false);
    outs() << "\n";

    // User-->Use-->Value
    outs() << "I MIEI OPERANDI SONO:\n";
    for (auto *Iter = Inst1st.op_begin(); Iter != Inst1st.op_end(); ++Iter) {
      Value *Operand = *Iter;

      if (Argument *Arg = dyn_cast<Argument>(Operand)) {
        outs() << "\t" << *Arg << ": SONO L'ARGOMENTO N. " << Arg->getArgNo() 
	       <<" DELLA FUNZIONE " << Arg->getParent()->getName()
               << "\n";
      }
      if (ConstantInt *C = dyn_cast<ConstantInt>(Operand)) {
        outs() << "\t" << *C << ": SONO UNA COSTANTE INTERA DI VALORE " << C->getValue()
               << "\n";
      }
    }

    outs() << "LA LISTA DEI MIEI USERS:\n";
    for (auto Iter = Inst1st.user_begin(); Iter != Inst1st.user_end(); ++Iter) {
      outs() << "\t" << *(dyn_cast<Instruction>(*Iter)) << "\n";
    }

    outs() << "E DEI MIEI USI (CHE E' LA STESSA):\n";
    for (auto Iter = Inst1st.use_begin(); Iter != Inst1st.use_end(); ++Iter) {
      outs() << "\t" << *(dyn_cast<Instruction>(Iter->getUser())) << "\n";
    }

    // Manipolazione delle istruzioni
    Instruction *NewInst = BinaryOperator::Create(
        Instruction::Add, Inst1st.getOperand(0), Inst1st.getOperand(0));

    NewInst->insertAfter(&Inst1st);
    // Si possono aggiornare le singole references separatamente?
    // Controlla la documentazione e prova a rispondere.
    Inst1st.replaceAllUsesWith(NewInst);
    */
   std::vector<Instruction*> toBeErasedVec;
   for(auto &InstIter : B){
    //algebraic identity verification and optimization, with elimination of unused instruction
    if(algebraicIdentity(InstIter)){
      toBeErasedVec.push_back(&InstIter);
    }
    if(BinaryOperator::Mul == InstIter.getOpcode()){
      Value *Op1=InstIter.getOperand(0);
      Value *Op2=InstIter.getOperand(1);
      if (ConstantInt *C = dyn_cast<ConstantInt>(Op1)) {
        if(C->getValue().isPowerOf2()){
          Constant *shiftValue= ConstantInt::get(C->getType(),C->getValue().logBase2());
          Instruction *NewShiftInst= BinaryOperator::Create(Instruction::Shl, Op2, shiftValue);
          NewShiftInst->insertAfter(&InstIter);
          InstIter.replaceAllUsesWith(NewShiftInst);
        }
      }
      else if (ConstantInt *C = dyn_cast<ConstantInt>(Op2)) {
        if(C->getValue().isPowerOf2()){
          Constant *shiftValue= ConstantInt::get(C->getType(),C->getValue().logBase2());
          Instruction *NewShiftInst= BinaryOperator::Create(Instruction::Shl, Op1, shiftValue);
          NewShiftInst->insertAfter(&InstIter);
          InstIter.replaceAllUsesWith(NewShiftInst);
          toBeErasedVec.push_back(&InstIter);
        }
      }      
      }
    }
    // for(auto &Inst:toBeErasedVec){
    //   Inst->eraseFromParent();
    // }

    return true;
  }


bool runOnFunction(Function &F) {
  bool Transformed = false;

  for (auto Iter = F.begin(); Iter != F.end(); ++Iter) {
    if (runOnBasicBlock(*Iter)) {
      Transformed = true;
    }
  }

  return Transformed;
}


PreservedAnalyses LocalOpts::run(Module &M,
                                      ModuleAnalysisManager &AM) {
  for (auto Fiter = M.begin(); Fiter != M.end(); ++Fiter)
    if (runOnFunction(*Fiter))
      return PreservedAnalyses::none();
  
  return PreservedAnalyses::all();
}

