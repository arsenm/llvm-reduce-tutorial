//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

using namespace llvm;

namespace {
class BuggyPass : public PassInfoMixin<BuggyPass> {
public:
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);
};

} // anonymous namespace

PreservedAnalyses BuggyPass::run(Function &F, FunctionAnalysisManager &AM) {
  bool Changed = false;

  for (BasicBlock &BB : F) {
    for (Instruction &I : BB) {
      Changed = true;
    }
  }

  return Changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
}

static llvm::PassPluginLibraryInfo getBuggyPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "BuggyPlugin", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerVectorizerStartEPCallback(
                [](llvm::FunctionPassManager &PM, OptimizationLevel Level) {
                  PM.addPass(BuggyPass());
                });
            PB.registerPipelineParsingCallback(
                [](StringRef Name, llvm::FunctionPassManager &PM,
                   ArrayRef<llvm::PassBuilder::PipelineElement>) {
                  if (Name == "buggy") {
                    PM.addPass(BuggyPass());
                    return true;
                  }
                  return false;
                });
          }};
}

// Declare plugin extension function declarations.
// #define HANDLE_EXTENSION(Ext) llvm::PassPluginLibraryInfo
// get##Ext##PluginInfo(); #include "llvm/Support/Extension.def"

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getBuggyPluginInfo();
}
