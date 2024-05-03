#include "llvm/Analysis/LoopAnalysisManager.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Transforms/Scalar/LoopPassManager.h"
#include <llvm/IR/PassManager.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Constants.h>

namespace llvm {
	class LoopWalkPass : public PassInfoMixin<LoopWalkPass> {
		public:
			PreservedAnalyses run(Loop &L, LoopAnalysisManager &LAM, LoopStandardAnalysisResults &LAR, LPMUpdater &LU);
	};
}
