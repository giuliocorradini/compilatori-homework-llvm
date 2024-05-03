#pragma once

#include "llvm/IR/PassManager.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/LoopAnalysisManager.h"
#include "llvm/Transforms/Scalar/LoopPassManager.h"

using namespace llvm;

class LoopWalkPass : public PassInfoMixin<LoopWalkPass> {
public:
  PreservedAnalyses run(Loop &L, LoopAnalysisManager &LAM, LoopStandardAnalysisResults &LAR, LPMUpdater &LU);
};
