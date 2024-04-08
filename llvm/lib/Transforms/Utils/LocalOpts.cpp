//===-- LocalOpts.cpp - Example Transformations --------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/Transforms/Utils/LocalOpts.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instructions.h"
#include <map>
#include <optional>
#include <string>
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
    auto otherOperand =
        getOtherOperand(B, A); //< get the other operand of B which is not A
    if (otherOperand == A->getOperand(0) or otherOperand == A->getOperand(1))
      return true;
  }

  return false;
}

bool optimizeOn(BasicBlock &B) {
  map<Instruction const *, Instruction *> replaceMapping;

  for (Instruction &I : B) {

    auto BinaryOp = dyn_cast<BinaryOperator>(&I);
    if (not BinaryOp) {
      // Not a binary operator, won't optimize here
      continue;
    }

    for (auto userIt : I.users()) {
      BinaryOperator *UserBinaryOp = dyn_cast<BinaryOperator>(userIt);
      if (not UserBinaryOp)
        continue;

      if (isReverseOperation(BinaryOp, UserBinaryOp)) {
        replaceMapping[UserBinaryOp] = BinaryOp;
      }
    }
  }

  for (Instruction &I : B) {
    if (replaceMapping.find(&I) != replaceMapping.end())
      I.replaceAllUsesWith(replaceMapping[&I]);
  }

  return replaceMapping.empty() ? false : true;
}
} // namespace MultiInstructionOpt

namespace StrenghtReduction {
int isPowerOf2OrAdj(const APInt &value) {
  if (value.isPowerOf2())
    return 0;
  else if ((value + 1).isPowerOf2())
    return 1;
  else if ((value - 1).isPowerOf2())
    return -1;
  return -2;
}

void strenghtReduction(Instruction &inst) {
  if (inst.getOpcode() == BinaryOperator::Mul) {
    // prendo l'argomento 1 e 2
    Value *fac1 = inst.getOperand(0);
    Value *fac2 = inst.getOperand(1);
    if (ConstantInt *C = dyn_cast<ConstantInt>(fac1)) {
      // Se il primo è una costante ed è potenza del 2
      int distance = isPowerOf2OrAdj(C->getValue());
      if (distance != -2) {
        const APInt &modifiedValue = C->getValue() + distance;
        // Creo la costante di shift
        Constant *shift =
            ConstantInt::get(C->getType(), modifiedValue.logBase2());
        // Creo una nuova istruzione e la genero dopo la moltiplicazione
        Instruction *shiftInst =
            BinaryOperator::Create(Instruction::Shl, fac2, shift);
        shiftInst->insertAfter(&inst);
        if (distance == -1) {
          Instruction *secondOp =
              BinaryOperator::Create(Instruction::Sub, shiftInst, fac2);
          secondOp->insertAfter(shiftInst);
          inst.replaceAllUsesWith(secondOp);
        } else if (distance == 1) {
          Instruction *secondOp =
              BinaryOperator::Create(Instruction::Add, shiftInst, fac2);
          secondOp->insertAfter(shiftInst);
          inst.replaceAllUsesWith(secondOp);
        } else
          inst.replaceAllUsesWith(shiftInst);
      }
    } else if (ConstantInt *C = dyn_cast<ConstantInt>(fac2)) {
      int distance = isPowerOf2OrAdj(C->getValue());
      if (distance != -2) {
        const APInt &modifiedValue = C->getValue() + distance;
        // Creo la costante di shift
        Constant *shift =
            ConstantInt::get(C->getType(), modifiedValue.logBase2());
        // Creo una nuova istruzione e la genero dopo la moltiplicazione
        Instruction *shiftInst =
            BinaryOperator::Create(Instruction::Shl, fac1, shift);
        shiftInst->insertAfter(&inst);
        if (distance == -1) {
          Instruction *secondOp =
              BinaryOperator::Create(Instruction::Sub, shiftInst, fac1);
          secondOp->insertAfter(shiftInst);
          inst.replaceAllUsesWith(secondOp);
        } else if (distance == 1) {
          Instruction *secondOp =
              BinaryOperator::Create(Instruction::Add, shiftInst, fac1);
          secondOp->insertAfter(shiftInst);
          inst.replaceAllUsesWith(secondOp);
        } else
          inst.replaceAllUsesWith(shiftInst);
      }
    }
  }
  // Se è una divisione
  if (inst.getOpcode() == BinaryOperator::SDiv) {
    // prendo l'argomento 1 e 2
    Value *fac1 = inst.getOperand(0);
    Value *fac2 = inst.getOperand(1);
    if (ConstantInt *C = dyn_cast<ConstantInt>(fac2)) {
      if (C->getValue().isPowerOf2()) {
        // Creo la costante di shift
        Constant *shift =
            ConstantInt::get(C->getType(), C->getValue().logBase2());
        // Creo una nuova istruzione e la genero dopo la divisione
        Instruction *shiftInst =
            BinaryOperator::Create(Instruction::AShr, fac1, shift);
        shiftInst->insertAfter(&inst);
        inst.replaceAllUsesWith(shiftInst);
      }
    }
  }
}

bool optimizeOn(BasicBlock &B) {
  for (Instruction &I : B) {
    strenghtReduction(I);
  }

  return true;
}
} // namespace StrenghtReduction

namespace AlgebraicIdentityOpt {
bool algebraicIdentity(llvm::Instruction &I) {
  Value *Op1 = I.getOperand(0);
  Value *Op2 = I.getOperand(1);
  if (BinaryOperator::Mul == I.getOpcode()) {
    // if first operand is constant and 1 and second operand is not a constant,
    // or viceversa...
    if (ConstantInt *C = dyn_cast<ConstantInt>(Op1);
        C && C->getValue().isOne() && not dyn_cast<ConstantInt>(Op2)) {
      I.replaceAllUsesWith(Op2);
    } else if (ConstantInt *C = dyn_cast<ConstantInt>(Op2);
               C && C->getValue().isOne() && not dyn_cast<ConstantInt>(Op1)) {
      I.replaceAllUsesWith(Op1);
    } else {
      return false;
    }
    return true;
  } else if (BinaryOperator::Add == I.getOpcode()) {
    // if first operand is constant and 0 and second operand is not a constant,
    // or viceversa...
    if (ConstantInt *C = dyn_cast<ConstantInt>(Op1);
        C && C->getValue().isZero() && not dyn_cast<ConstantInt>(Op2)) {
      I.replaceAllUsesWith(Op2);
    } else if (ConstantInt *C = dyn_cast<ConstantInt>(Op2);
               C && C->getValue().isZero() && not dyn_cast<ConstantInt>(Op1)) {
      I.replaceAllUsesWith(Op1);
    } else {
      return false;
    }
    return true;
  }
  return false;
}

bool optimizeOn(BasicBlock &B) {
  bool optimizedSomething = false;

  for (auto &InstIter : B) {
    // algebraic identity verification and optimization
    optimizedSomething |= algebraicIdentity(InstIter);
  }

  return optimizedSomething;
}
} // namespace AlgebraicIdentityOpt

bool runOnBasicBlock(BasicBlock &B) {
  bool isOptimized = false;

  // Run strenght reduction and algebraic optimization here
  isOptimized = MultiInstructionOpt::optimizeOn(B);
  isOptimized |= StrenghtReduction::optimizeOn(B);
  isOptimized |= AlgebraicIdentityOpt::optimizeOn(B);

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

PreservedAnalyses LocalOpts::run(Module &M, ModuleAnalysisManager &AM) {
  for (auto Fiter = M.begin(); Fiter != M.end(); ++Fiter)
    if (runOnFunction(*Fiter))
      return PreservedAnalyses::none();

  return PreservedAnalyses::all();
}
