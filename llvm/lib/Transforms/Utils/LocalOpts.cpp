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
  optional<Instruction::BinaryOps> getReverseOpcode(BinaryOperator const *I) {
    auto opcode = I->getOpcode();

    static map<Instruction::BinaryOps, Instruction::BinaryOps> reverse = {
      {Instruction::Add, Instruction::Sub},
      {Instruction::Sub, Instruction::Add},
      {Instruction::Mul, Instruction::SDiv},
      {Instruction::SDiv, Instruction::Mul},
      {Instruction::Shl, Instruction::LShr},
      {Instruction::LShr, Instruction::Shl},
    };

    if (auto r = reverse.find(opcode); r != reverse.end())
      return make_optional(r->second);

    return nullopt;
  }

  /**
   * Gets the other operand, given an instruction and one of the operands
  */
  Value *getOtherOperand(Instruction const *I, Value const *o) {
    auto op = I->getOperand(0);
    
    return (op == o) ? I->getOperand(1) : op;
  }

  /**
   * Confronts operands to see if the opposite operation is performed.
   * 
   * @param A Pointer to instruction to optimized, casted to BinaryOperator
   * @param B Pointer to the user, i.e. the instruction that uses A as argument
  */
  bool isReverseOperation(BinaryOperator const *A, BinaryOperator const *B) {
    auto revOpcode = getReverseOpcode(A);
    if (revOpcode.has_value() and revOpcode.value() == B->getOpcode()) {
      auto otherOperand = getOtherOperand(B, A);  //< get the other operand of B which is not A
      if (otherOperand == A->getOperand(0) or otherOperand == A->getOperand(1))
        return true;
    }

    return false;
  }

  bool optimizeOn(BasicBlock &B) {
    map<Instruction const *, Instruction *> replaceMapping;

    for (Instruction &I: B) {
      
      auto BinaryOp = dyn_cast<BinaryOperator>(&I);
      if (not BinaryOp) {
        // Not a binary operator, won't optimize here
        continue;
      }

      
      for (auto user: I.users()) {
        Instruction const *ubop = dyn_cast<const Instruction>(&user);
        if (not ubop)
          continue;

        BinaryOperator const *UserBinaryOp = dyn_cast<const BinaryOperator>(ubop);  //Why not BinaryOperator directly? Doesn't work
        if (not UserBinaryOp)
          continue;

        if (isReverseOperation(BinaryOp, UserBinaryOp)) {
          replaceMapping[UserBinaryOp] = BinaryOp;
        }
      }
      
    }

    for (Instruction &I: B) {
      if (replaceMapping.find(&I) != replaceMapping.end())
        I.replaceAllUsesWith(replaceMapping[&I]);
    }

    return replaceMapping.empty() ? false : true;
  }
}

bool runOnBasicBlock(BasicBlock &B) {
  bool isOptimized = false;

  // Run strenght reduction and algebraic optimization here
  isOptimized = MultiInstructionOpt::optimizeOn(B);

  return isOptimized;
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

