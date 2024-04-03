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
#include <map>
#include <optional>
using namespace std;

using namespace llvm;

namespace MultiInstructionOpt {
  optional<Instruction::BinaryOps> getReverseOpcode(BinaryOperator *I) {
    auto opcode = I->getOpcode();

    static map<Instruction::BinaryOps, Instruction::BinaryOps> reverse = {
      {Instruction::Add, Instruction::Sub},
      {Instruction::Sub, Instruction::Add},
      {Instruction::Mul, Instruction::SDiv},
      {Instruction::SDiv, Instruction::Mul},
      {Instruction::Shl, Instruction::LShr},
      {Instruction::LShr, Instruction::Shl},
      {Instruction::FNeg, Instruction::FNeg}
    };

    if (auto r = reverse.find(opcode); r != reverse.end())
      return make_optional(r->second);

    return nullopt;
  }

  /**
   * Gets the other operand, given an instruction and one of the operands
  */
  Value *getOtherOperand(Instruction &I, Value *o) {
    auto op = I.getOperand(0);
    
    return (op == o) ? I.getOperand(1) : op;
  }

  /**
   * Confronts operands to see if the opposite operation is performed.
  */
  bool isReverseOperation(BinaryOperator *A, BinaryOperator *B) {
    if (auto revOpcode = getReverseOpcode(A); revOpcode == B->getOpcode()) {
      ;
    }
  }

  bool optimizeOn(BasicBlock &B) {
    for (Instruction &I: B) {
      
      auto BinaryOp = dyn_cast<BinaryOperator>(&I);
      if (not BinaryOp) {
        // Not a binary operator, won't optimize here
        continue;
      }

      
      for (auto user: I.users()) {
        auto UserBinaryOp = dyn_cast<BinaryOperator>(&I);
        if (not UserBinaryOp)
          continue;

        if (isReverseOperation(BinaryOp, UserBinaryOp)) {
          UserBinaryOp->replaceAllUsesWith(BinaryOp);
        }
      }
      
    }

    return true;
  }
}

bool runOnBasicBlock(BasicBlock &B) {
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

