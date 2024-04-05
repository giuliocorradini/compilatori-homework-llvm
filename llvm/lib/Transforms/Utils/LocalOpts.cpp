//===-- LocalOpts.cpp - Example Transformations --------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This pass implements algebraic reduction, strength reduction and
// multi-instruction optimizations.
//
//===----------------------------------------------------------------------===//

#include "llvm/Transforms/Utils/LocalOpts.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstrTypes.h"
#include <string>
#include <vector>
using namespace std;

using namespace llvm;

int isPowerOf2OrAdj(const APInt& value){
	if (value.isPowerOf2())
		return 0;
	else if ((value + 1).isPowerOf2())
		return 1;
	else if ( (value - 1).isPowerOf2())
		return -1;
	return -2;
}

void strenghtReduction(Instruction &inst, std::vector<Instruction *> &eraseList){
	if (inst.getOpcode() == BinaryOperator::Mul){
		// prendo l'argomento 1 e 2
		Value *fac1 = inst.getOperand(0);
		Value *fac2 =  inst.getOperand(1);
		if (ConstantInt *C = dyn_cast<ConstantInt>(fac1)) {
			// Se il primo è una costante ed è potenza del 2
			int distance = isPowerOf2OrAdj(C->getValue());
			if(distance != -2){
				const APInt& modifiedValue = C->getValue() + distance;
				// Creo la costante di shift 
				Constant *shift = ConstantInt::get(C->getType(), modifiedValue.logBase2());
				// Creo una nuova istruzione e la genero dopo la moltiplicazione
				Instruction* shiftInst = BinaryOperator::Create(Instruction::Shl, fac2, shift);
				shiftInst->insertAfter(&inst);
				if (distance == -1){
					Instruction* secondOp = BinaryOperator::Create(Instruction::Sub, shiftInst, fac2);
					secondOp->insertAfter(shiftInst);
					inst.replaceAllUsesWith(secondOp);
				}
				else if (distance == 1){
					Instruction* secondOp = BinaryOperator::Create(Instruction::Add, shiftInst, fac2);
					secondOp->insertAfter(shiftInst);
					inst.replaceAllUsesWith(secondOp);
				}
				else
					inst.replaceAllUsesWith(shiftInst);
				eraseList.push_back(&inst);
			}
		}
		else if(ConstantInt *C = dyn_cast<ConstantInt>(fac2)){
			int distance = isPowerOf2OrAdj(C->getValue());
			if(distance != -2){
				const APInt& modifiedValue = C->getValue() + distance;
				// Creo la costante di shift 
				Constant *shift = ConstantInt::get(C->getType(), modifiedValue.logBase2());
				// Creo una nuova istruzione e la genero dopo la moltiplicazione
				Instruction* shiftInst = BinaryOperator::Create(Instruction::Shl, fac1, shift);
				shiftInst->insertAfter(&inst);
				if (distance == -1){
					Instruction* secondOp = BinaryOperator::Create(Instruction::Sub, shiftInst, fac1);
					secondOp->insertAfter(shiftInst);
					inst.replaceAllUsesWith(secondOp);
				}
				else if (distance == 1){
					Instruction* secondOp = BinaryOperator::Create(Instruction::Add, shiftInst, fac1);
					secondOp->insertAfter(shiftInst);
					inst.replaceAllUsesWith(secondOp);
				}
				else
					inst.replaceAllUsesWith(shiftInst);
				eraseList.push_back(&inst);
			}
		}
	}
	//Se è una divisione
	if (inst.getOpcode() == BinaryOperator::SDiv){
		// prendo l'argomento 1 e 2
		Value *fac1 = inst.getOperand(0);
		Value *fac2 =  inst.getOperand(1);
		if(ConstantInt *C = dyn_cast<ConstantInt>(fac2)){
			if(C->getValue().isPowerOf2()){
				// Creo la costante di shift 
				Constant *shift = ConstantInt::get(C->getType(), C->getValue().logBase2());
				// Creo una nuova istruzione e la genero dopo la divisione
				Instruction* shiftInst = BinaryOperator::Create(Instruction::AShr, fac1, shift);
				shiftInst->insertAfter(&inst);
				inst.replaceAllUsesWith(shiftInst);
				eraseList.push_back(&inst);
			}
		}
	}
}

bool runOnBasicBlock(BasicBlock &B) {
	std::vector<Instruction *> eraseList;
	for (auto &inst : B){
		strenghtReduction(inst, eraseList);
	}
	for (auto &inst : eraseList){
		inst->eraseFromParent();
	}
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


