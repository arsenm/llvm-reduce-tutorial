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
#include "llvm/Support/Process.h"

using namespace llvm;

namespace {

struct BuggyOptions {
  bool CrashOnVector = false;
  bool CrashOnShuffleVector = false;
  bool CrashOnLoadOfIntToPtr = false;
  bool CrashOnAggregatePhi = false;
  bool CrashOnSwitchOddNumberCases = false;
  bool InfLoopOnIndirectCall = false;
  bool BugOnlyIfOddNumberInsts = false;
};

static volatile int side_effect;

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

  size_t InstCount = 0;
  if (Options.BugOnlyIfOddNumberInsts) {
    for (BasicBlock &BB : F)
      InstCount += BB.size();
    if ((InstCount & 1) == 0)
      return PreservedAnalyses::all();
  }

  for (BasicBlock &BB : F) {
    for (Instruction &I : BB) {

      if (Options.CrashOnSwitchOddNumberCases) {
        if (const auto *Switch = dyn_cast<SwitchInst>(&I)) {
          if (Switch->getNumCases() & 1)
            report_fatal_error("switch with odd number of cases is broken");
        }
      }

      if (Options.CrashOnShuffleVector && isa<ShuffleVectorInst>(I))
        report_fatal_error("shufflevector instructions are broken");

      if (Options.CrashOnVector && isa<VectorType>(I.getType()))
        report_fatal_error("vector instructions are broken");

      if (Options.CrashOnAggregatePhi && isa<PHINode>(I) &&
          I.getType()->isAggregateType())
        report_fatal_error("aggregate phis are broken");

      if (Options.CrashOnLoadOfIntToPtr) {
        auto *LI = dyn_cast<LoadInst>(&I);
        if (LI && isa<IntToPtrInst>(LI->getPointerOperand()))
          report_fatal_error("load of inttoptr is broken");
      }

      if (Options.InfLoopOnIndirectCall) {
        auto *CI = dyn_cast<CallBase>(&I);
        while (CI && !CI->getCalledFunction())
          side_effect = 0;
      }

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
    if (ParamName == "crash-on-vector")
      Result.CrashOnVector = Enable;
    else if (ParamName == "crash-on-shufflevector")
      Result.CrashOnShuffleVector = Enable;
    else if (ParamName == "crash-on-aggregate-phi")
      Result.CrashOnAggregatePhi = Enable;
    else if (ParamName == "crash-load-of-inttoptr")
      Result.CrashOnLoadOfIntToPtr = Enable;
    else if (ParamName == "crash-switch-odd-number-cases")
      Result.CrashOnSwitchOddNumberCases = Enable;
    else if (ParamName == "infloop-on-indirect-call")
      Result.InfLoopOnIndirectCall = Enable;
    else if (ParamName == "bug-only-if-odd-number-insts")
      Result.BugOnlyIfOddNumberInsts = Enable;
    else {
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
                  if (std::optional<std::string> PassPipelineOpts =
                          sys::Process::GetEnv("BUGGY_PLUGIN_OPTS")) {

                    Expected<BuggyOptions> Options =
                        parseBuggyOptions(*PassPipelineOpts);
                    if (Options) {
                      PM.addPass(BuggyPass(*Options));
                      return true;
                    }

                    report_fatal_error(Options.takeError(), false);
                  }

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
