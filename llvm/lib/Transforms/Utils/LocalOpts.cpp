//===-- LocalOpts.cpp - Example Transformations --------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// This module performs local optimizations on the code: algebraic identity, strenght
// reduction and multi-instruction optimization.
//
// Each optimization is implemented in its exclusive namespace, and defines a function
// with signtature `bool optimizeOn(BasicBlock &B)` that operates on a basic block,
// and returns true if an optimization was performed.
//
// The entry point of this module is LocalOpts::run, under no namespace.
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

/**
 * Implements multi-instruction optimizations, id est: if the value computed by an instruction
 * is a usee of an instruction that performs the opposite using the same operand, then the second
 * instruction can be avoided and its uses can be replaced with the operand that appears in the first
 * instruction, but not in the second.
 * 
 * Let's give an example:
 * 
 * %1 = add 2, %0
 * %2 = sub %1, 2
 * 
 * The constant 2 appears in both the first and second instructions, and sub is the opposite of add,
 * therefore we can replace all the uses of %2 with %0.
 */
namespace MultiInstructionOpt {

/**
 * Gets the reverse opcode for a binary operation. A binary operation is identified by
 * an opcode (add, sub, mul...), this function defines an association between opposite
 * opcodes (add-sub, mul-div etc.).
 * 
 * @returns the revserse opcode a std::optional. A wrapper for a value that may or may
 * not be present.
 */
optional<Instruction::BinaryOps> getReverseOpcode(BinaryOperator const *I) {
  auto opcode = I->getOpcode();

  /**
   * This map instantly returns the reverse code for an operation.
   */
  static map<Instruction::BinaryOps, Instruction::BinaryOps> reverse = {
      {Instruction::Add, Instruction::Sub},
      {Instruction::Sub, Instruction::Add},
      /*{Instruction::Mul, Instruction::SDiv},
      {Instruction::SDiv, Instruction::Mul},
      {Instruction::Shl, Instruction::LShr},
      {Instruction::LShr, Instruction::Shl},*/
  };

  /**
   * Given the opcode of the BinaryOperator I, gets the reverse from the map.
   */
  if (auto r = reverse.find(opcode); r != reverse.end())
    return make_optional(r->second);

  return nullopt; //< An optional with no value inside
}

/**
 * Gets the other operand, given an instruction and one of the operands
 * 
 * @param I, an instruction
 * @param o, an operand of I
 * @returns the other operand of I that is not o
 */
Value *getOtherOperand(Instruction const *I, Value const *o) {
  auto op = I->getOperand(0);

  return (op == o) ? I->getOperand(1) : op;
}

/**
 * Confronts operands to see if the opposite operation is performed.
 *
 * @param A Pointer to the instruction to optimize, casted to BinaryOperator
 * @param B Pointer to the user, i.e. the instruction that uses A as argument
 */
optional<Value *> isReverseOperation(BinaryOperator const *A, BinaryOperator const *B) {
  auto revOpcode = getReverseOpcode(A);

  // Get the reverse opcode for A, if it's the same opcode of B, then the operations
  // might be opposite.
  if (revOpcode.has_value() and revOpcode.value() == B->getOpcode()) {
    // A is an operand of B, get the other operand of B and check if it appears in A.
    auto otherOperand =
        getOtherOperand(B, A); //< get the other operand of B which is not A
    if (otherOperand == A->getOperand(0))
      return make_optional(A->getOperand(1));
    else if (otherOperand == A->getOperand(1))
      return make_optional(A->getOperand(0));
  }

  return nullopt;
}

/**
 * Multi instruction optimizations are performed on binary operators onyly.
 * The entry point (this function) scans all the instructions in a basic block and
 * when it finds a binary operators looks for its users.
 * 
 * We are looking for binary operations that are users of other binary operations.
 * isReverseOperation takes a pair of binary operations ("usee", "user") and returns a
 * pointer to the value that can be reversed and replace the use of the seconds operation
 * with itself.
 * 
 * "usee" -> BinaryOp
 * "user" -> UserBinaryOp
 * 
 * The pointer is saved in initOp. If nullptr is returned, "user" is not the reverse operation
 * of "usee".
 * 
 * InitOp is inserted in replaceMapping, a key-value map that stores "user" and the value to
 * replace "user"'s uses with (initOp).
 */
bool optimizeOn(BasicBlock &B) {
  map<Instruction const *, Value *> replaceMapping;

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

      if (auto initOp = isReverseOperation(BinaryOp, UserBinaryOp); initOp) {
        replaceMapping[UserBinaryOp] = initOp.value();
      }
    }
  }

  /**
   * Now replace all identified usee with the values.
   */
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
    if (ConstantInt *C = dyn_cast<ConstantInt>(fac1); C and not C->isZero()) {
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
    } else if (ConstantInt *C = dyn_cast<ConstantInt>(fac2); C and not C->isZero()) {
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

/**
 * When I is an operation between two constant, that could be picked up
 * by algebraic identity, we don't do nothing. It's optimized by constant folding
 * and constant propagation.
 */
bool algebraicIdentity(llvm::Instruction &I) {
  if (BinaryOperator::Mul == I.getOpcode()) {
    Value *Op1 = I.getOperand(0);
    Value *Op2 = I.getOperand(1);
    // if first operand is constant and 1 and second operand is not a constant,
    // or viceversa...
    if (ConstantInt *C = dyn_cast<ConstantInt>(Op1);
        C && C->getValue().isOne() && not isa<ConstantInt>(Op2)) {
      I.replaceAllUsesWith(Op2);
    } else if (ConstantInt *C = dyn_cast<ConstantInt>(Op2);
        C && C->getValue().isOne() && not isa<ConstantInt>(Op1)) {
      I.replaceAllUsesWith(Op1);
    } else {
      return false;
    }
    return true;
  } else if (BinaryOperator::Add == I.getOpcode()) {
    Value *Op1 = I.getOperand(0);
    Value *Op2 = I.getOperand(1);
    // if first operand is constant and 0 and second operand is not a constant,
    // or viceversa...
    if (ConstantInt *C = dyn_cast<ConstantInt>(Op1);
        C && C->getValue().isZero() && not isa<ConstantInt>(Op2)) {
      I.replaceAllUsesWith(Op2);
    } else if (ConstantInt *C = dyn_cast<ConstantInt>(Op2);
        C && C->getValue().isZero() && not isa<ConstantInt>(Op1)) {
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

/**
 * Runs the actual optimizations on a basic block.
 */
bool runOnBasicBlock(BasicBlock &B) {
  bool isOptimized = false;

  // Run strenght reduction and algebraic optimization here
  isOptimized |= AlgebraicIdentityOpt::optimizeOn(B);
  isOptimized |= StrenghtReduction::optimizeOn(B);
  isOptimized |= MultiInstructionOpt::optimizeOn(B);

  return isOptimized;
}

/**
 * For each basic block in the given function F, performs the optimizations.
 * 
 * Returns true if at least a basic block was optimized.
 */
bool runOnFunction(Function &F) {
  bool Transformed = false;

  for (auto Iter = F.begin(); Iter != F.end(); ++Iter) {
    if (runOnBasicBlock(*Iter)) {
      Transformed = true;
    }
  }

  return Transformed;
}

/**
 * For each function in the module (LocalOpts is a ModulePass), run the optimizations
 */
PreservedAnalyses LocalOpts::run(Module &M, ModuleAnalysisManager &AM) {
  for (auto Fiter = M.begin(); Fiter != M.end(); ++Fiter)
    runOnFunction(*Fiter);

  return PreservedAnalyses::all();
}
