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
#include "llvm/Support/FormatVariadic.h"

using namespace llvm;

namespace {

struct BuggyOptions {
  bool CrashOnVector = true;
};

class BuggyPass : public PassInfoMixin<BuggyPass> {
  const BuggyOptions Options;

public:
  BuggyPass(BuggyOptions Opts = BuggyOptions()) : Options(Opts) {}
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);
};

StringLiteral Name = "buggy";
StringLiteral PassName = "buggy";

} // anonymous namespace

PreservedAnalyses BuggyPass::run(Function &F, FunctionAnalysisManager &AM) {
  bool Changed = false;

  for (BasicBlock &BB : F) {
    for (Instruction &I : BB) {

      if (Options.CrashOnVector && isa<VectorType>(I.getType()))
        report_fatal_error("vector instructions are broken");

      Changed = true;
    }
  }

  return Changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
}

static Expected<BuggyOptions> parseBuggyOptions(StringRef Params) {
  if (Params.empty())
    return BuggyOptions();

  BuggyOptions Result;
  while (!Params.empty()) {
    StringRef ParamName;
    std::tie(ParamName, Params) = Params.split(';');

    bool Enable = !ParamName.consume_front("no-");
    if (ParamName == "crash-on-vector") {
      Result.CrashOnVector = Enable;
    } else {
      return make_error<StringError>(
          formatv("invalid buggy pass parameter '{0}'", Params).str(),
          inconvertibleErrorCode());
    }
  }

  return Result;
}

static llvm::PassPluginLibraryInfo getBuggyPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "BuggyPlugin", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerVectorizerStartEPCallback(
                [](llvm::FunctionPassManager &PM, OptimizationLevel Level) {
                  PM.addPass(BuggyPass());
                  return true;
                });
            PB.registerPipelineParsingCallback(
                [](StringRef Name, llvm::FunctionPassManager &PM,
                   ArrayRef<llvm::PassBuilder::PipelineElement>) {
                  if (PassBuilder::checkParametrizedPassName(Name, PassName)) {
                    auto Params = PassBuilder::parsePassParameters(
                        parseBuggyOptions, Name, PassName);
                    if (!Params)
                      return false;
                    PM.addPass(BuggyPass(*Params));
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
