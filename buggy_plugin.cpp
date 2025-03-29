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
  bool CrashOnStoreToConstantExpr = false;
  bool CrashOnAggregatePhi = false;
  bool CrashOnPhiRepeatedPredecessor = false;
  bool CrashOnPhiSelfReference = false;
  bool CrashOnSwitchOddNumberCases = false;
  bool CrashOnI1Select = false;
  bool CrashIfWeakGlobalExists = false;
  bool InfLoopOnIndirectCall = false;
  bool BugOnlyIfOddNumberInsts = false;
  bool BugOnlyIfInternalFunc = false;
  bool BugOnlyIfExternalFunc = false;
  bool InsertUnparseableAsm = false;
  bool MiscompileICmpSltToSle = false;
  bool CrashOnBuggyAttr = false;
};

static volatile int side_effect;
static StringLiteral Name = "buggy";
static StringLiteral PassName = "buggy";

class BuggyPass : public PassInfoMixin<BuggyPass> {
  const BuggyOptions Options;

public:
  BuggyPass(BuggyOptions Opts = BuggyOptions()) : Options(Opts) {}
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);

  static StringRef name() { return PassName; }
};

class BuggyAttrPass : public PassInfoMixin<BuggyAttrPass> {
public:
  BuggyAttrPass() {}
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);

  static StringRef name() { return "buggy-attr"; }
};

} // anonymous namespace


PreservedAnalyses BuggyPass::run(Function &F, FunctionAnalysisManager &AM) {
  bool Changed = false;

  if (Options.BugOnlyIfInternalFunc && !F.hasInternalLinkage())
    return PreservedAnalyses::all();
  if (Options.BugOnlyIfExternalFunc && !F.hasExternalLinkage())
    return PreservedAnalyses::all();

  if (Options.CrashOnBuggyAttr && F.hasFnAttribute("buggy-attr"))
    report_fatal_error("buggy-attr is broken");

  size_t InstCount = 0;
  if (Options.BugOnlyIfOddNumberInsts) {
    for (BasicBlock &BB : F)
      InstCount += BB.size();
    if ((InstCount & 1) == 0)
      return PreservedAnalyses::all();
  }

  if (Options.CrashIfWeakGlobalExists) {
    for (const GlobalValue &GV : F.getParent()->globals()) {
      if (GV.hasWeakLinkage())
        report_fatal_error("broken if there is a weak global");
    }
  }

  LLVMContext &Ctx = F.getContext();

  if (Options.InsertUnparseableAsm) {
    BasicBlock &InsertBB = F.getEntryBlock();
    BasicBlock::iterator It = InsertBB.getFirstInsertionPt();
    FunctionType *FTy =
        FunctionType::get(Type::getVoidTy(Ctx), {}, /*isVarArg=*/false);
    InlineAsm *Asm =
        InlineAsm::get(FTy, "skynet", "", /*hasSideEffects=*/false);
    CallInst::Create(FTy, Asm, "", It);
    return PreservedAnalyses::none();
  }

  for (BasicBlock &BB : F) {
    for (Instruction &I : BB) {

      if (ICmpInst *ICmp = dyn_cast<ICmpInst>(&I)) {
        if (Options.MiscompileICmpSltToSle) {
          if (ICmp->getPredicate() == ICmpInst::ICMP_SLT) {
            ICmp->setPredicate(ICmpInst::ICMP_SLE);
            Changed = true;
          }
        }
      }

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

      if (PHINode *Phi = dyn_cast<PHINode>(&I)) {
        if (Options.CrashOnPhiRepeatedPredecessor) {
          SmallPtrSet<BasicBlock *, 4> VisitedPreds;
          for (BasicBlock *Pred : Phi->blocks()) {
            if (!VisitedPreds.insert(Pred).second)
              report_fatal_error("phi with repeated predecessor is broken");
          }
        }

        if (Options.CrashOnPhiSelfReference) {
          for (Value *Incoming : Phi->incoming_values()) {
            if (Incoming == Phi)
              report_fatal_error("self referential phi is broken");
          }
        }

        if (Options.CrashOnAggregatePhi && Phi->getType()->isAggregateType())
          report_fatal_error("aggregate phis are broken");
      }

      if (Options.CrashOnI1Select) {
        if (const auto *SI = dyn_cast<SelectInst>(&I))
          if (SI->getType()->isIntegerTy(1))
            report_fatal_error("i1 typed select is broken");
      }

      if (Options.CrashOnStoreToConstantExpr) {
        if (const auto *SI = dyn_cast<StoreInst>(&I)) {
          if (isa<ConstantExpr>(SI->getPointerOperand()))
            report_fatal_error("store to constantexpr pointer is broken");
        }
      }

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
    else if (ParamName == "crash-on-repeated-phi-predecessor")
      Result.CrashOnPhiRepeatedPredecessor = Enable;
    else if (ParamName == "crash-on-phi-self-reference")
      Result.CrashOnPhiSelfReference = Enable;
    else if (ParamName == "crash-load-of-inttoptr")
      Result.CrashOnLoadOfIntToPtr = Enable;
    else if (ParamName == "crash-store-to-constantexpr")
      Result.CrashOnStoreToConstantExpr = Enable;
    else if (ParamName == "crash-switch-odd-number-cases")
      Result.CrashOnSwitchOddNumberCases = Enable;
    else if (ParamName == "crash-on-i1-select")
      Result.CrashOnI1Select = Enable;
    else if (ParamName == "crash-if-weak-global-exists")
      Result.CrashIfWeakGlobalExists = Enable;
    else if (ParamName == "infloop-on-indirect-call")
      Result.InfLoopOnIndirectCall = Enable;
    else if (ParamName == "bug-only-if-odd-number-insts")
      Result.BugOnlyIfOddNumberInsts = Enable;
    else if (ParamName == "bug-only-if-internal-func")
      Result.BugOnlyIfInternalFunc = Enable;
    else if (ParamName == "bug-only-if-external-func")
      Result.BugOnlyIfExternalFunc = Enable;
    else if (ParamName == "insert-unparseable-asm")
      Result.InsertUnparseableAsm = Enable;
    else if (ParamName == "miscompile-icmp-slt-to-sle")
      Result.MiscompileICmpSltToSle = Enable;
    else if (ParamName == "crash-on-buggy-attr")
      Result.CrashOnBuggyAttr = Enable;
    else {
      return make_error<StringError>(
          formatv("invalid buggy pass parameter '{0}'", Params).str(),
          inconvertibleErrorCode());
    }
  }

  return Result;
}

PreservedAnalyses BuggyAttrPass::run(Module &M, ModuleAnalysisManager &AM) {
  for (Function &F : M) {
    if (!F.isDeclaration())
      F.addFnAttr("buggy-attr");
  }

  return PreservedAnalyses::all();
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

            PB.registerOptimizerEarlyEPCallback([](ModulePassManager &PM,
                                                   OptimizationLevel,
                                                   ThinOrFullLTOPhase) {
              if (std::optional<std::string> PassPipelineOpts =
                      sys::Process::GetEnv("BUGGY_PLUGIN_OPTS")) {
                Expected<BuggyOptions> Options =
                    parseBuggyOptions(*PassPipelineOpts);
                if (Options) {
                  if (Options->CrashOnBuggyAttr)
                    PM.addPass(BuggyAttrPass());
                }
              }
            });
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &PM,
                   ArrayRef<llvm::PassBuilder::PipelineElement>) {
                  if (Name == "buggy-attr") {
                    PM.addPass(BuggyAttrPass());
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
