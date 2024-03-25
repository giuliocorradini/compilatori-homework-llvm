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
#include <string>
#include <vector>
using namespace std;

using namespace llvm;

/**
 * Strength reduction of multiplication by power of 2s.
 * Converts Muls into left shifts.
*/
bool runOnBasicBlock(BasicBlock &B) {
  vector<Instruction *> toBeErased;

  // For each instruction of 
  for (auto &i: B) {
    BinaryOperator *mul = dyn_cast<BinaryOperator>(&i);
    if (not mul or mul->getOpcode() != BinaryOperator::Mul)
      continue;

    ConstantInt *C;
    Value *Factor;

    C = dyn_cast<ConstantInt>(mul->getOperand(0)); // Get first operand and try casting to constant
    Factor = mul->getOperand(1);

    if (not C or not C->getValue().isPowerOf2()) {
      C = dyn_cast<ConstantInt>(mul->getOperand(1));

      if (not C or not C->getValue().isPowerOf2()) {
        outs() << *mul << ": no operand is a constant integer nor power of 2\n";
        continue;
      }

      Factor = mul->getOperand(0);
    }

    outs() << "Converting " << *mul << " to left shift\n";
    Constant *shiftConst = ConstantInt::get(C->getType(), C->getValue().exactLogBase2());
    Instruction *new_shift = BinaryOperator::Create(
      BinaryOperator::Shl, Factor, shiftConst
    );

    new_shift->insertAfter(mul);
    mul->replaceAllUsesWith(new_shift);

    toBeErased.push_back(mul);
  }

  for (Instruction *i: toBeErased)
    i->eraseFromParent();

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

