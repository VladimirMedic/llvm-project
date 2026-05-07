//===- raiser.cpp - Hotswap MC -> LLVM IR raiser scaffolding --------------===//
//
// Part of Comgr, under the Apache License v2.0 with LLVM Exceptions. See
// amd/comgr/LICENSE.TXT in this repository for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Disassembles a kernel's ELF text section into a typed `DecodedInst` stream
// and builds an `llvm::Module` with a kernel function whose body is `ret void`.
// See `raiser.h` for the full raise pipeline (ELF ingestion -> decode ->
// per-format handlers -> post-raise analyses).
//
//===----------------------------------------------------------------------===//

#include "raiser.h"

#include "amdgpu_formats.h"
#include "decode.h"
#include "decoded_inst.h"
#include "mc_state.h"
#include "opcode_map.h"
#include "raise_failure.h"

#include "llvm/ADT/Twine.h"
#include "llvm/TargetParser/TargetParser.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CallingConv.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/TargetParser/Triple.h"

namespace COMGR::hotswap {

RaiseResult raiseToIR(llvm::ArrayRef<uint8_t> TextBytes,
                      llvm::StringRef SourceIsa,
                      llvm::StringRef KernelName,
                      const KernelMeta &Meta,
                      uint64_t KernelOffset,
                      llvm::StringRef CompilationTargetIsa) {
  using namespace llvm;
  RaiseResult Result;

  // === Phase 0: Validate inputs ===
  // Reject before reaching the MC stack — an empty or non-AMDGPU ISA
  // string slips past `createMCSubtargetInfo` (it returns a feature-less
  // subtarget) and only blows up later in `createMCDisassembler` with a
  // process-aborting `LLVM ERROR: disassembly not yet supported for
  // subtarget`. Surface a structured failure instead.
  StringRef SourceCpu = SourceIsa.rsplit('-').second;
  if (SourceCpu.empty())
    SourceCpu = SourceIsa;
  SourceCpu = SourceCpu.split(':').first;
  if (SourceIsa.empty() ||
      AMDGPU::parseArchAMDGCN(SourceCpu) == AMDGPU::GK_NONE) {
    Result.Failure.Reason = RaiseFailureReason::BadInput;
    Result.Failure.Detail =
        (Twine("source ISA '") + SourceIsa +
         "' does not name an AMDGPU GPU").str();
    return Result;
  }
  if (!Meta.HasKernelDescriptor) {
    Result.Failure.Reason = RaiseFailureReason::BadInput;
    Result.Failure.Detail =
        (Twine("kernel '") + KernelName + "' has no kernel descriptor").str();
    return Result;
  }

  // === Phase 1: MC stack + opcode canonicalisation ===
  MCState Mc;
  initMCState(Mc, SourceCpu);
  OpcodeMap OpcMap;
  OpcMap.build(*Mc.InstrInfo);

  // === Phase 2: Disassemble kernel text section ===
  DecodeResult Decoded = decodeKernel(Mc, OpcMap, TextBytes, KernelOffset);
  Result.TotalCount = static_cast<int>(Decoded.Insts.size());

  // === Phase 3: Build LLVM IR module + function ===
  Result.Ctx = std::make_unique<LLVMContext>();
  LLVMContext &C = *Result.Ctx;
  Result.Module = std::make_unique<Module>("transpiler_module", C);
  Module &M = *Result.Module;
  M.setTargetTriple(Triple(kAMDGPUTriple));

  TargetOptions Opts;
  std::unique_ptr<TargetMachine> Tm(Mc.Target->createTargetMachine(
      Triple(kAMDGPUTriple),
      CompilationTargetIsa.empty() ? SourceIsa : CompilationTargetIsa,
      "", Opts, Reloc::PIC_));
  if (Tm)
    M.setDataLayout(Tm->createDataLayout());

  FunctionType *FuncTy =
      FunctionType::get(Type::getVoidTy(C), /*isVarArg=*/false);
  Function *F =
      Function::Create(FuncTy, GlobalValue::ExternalLinkage, KernelName, &M);
  F->setCallingConv(CallingConv::AMDGPU_KERNEL);
  BasicBlock *Entry = BasicBlock::Create(C, "entry", F);
  IRBuilder<> B(Entry);
  B.CreateRetVoid();

  // === Phase 4: Bail on the first instruction ===
  // No per-format handlers are wired up yet, so any non-empty kernel body
  // surfaces as `RaiseFailure::unsupportedOpcode`. An empty kernel still
  // raises successfully.
  if (!Decoded.Insts.empty()) {
    const DecodedInst &Di = Decoded.Insts.front();
    Result.Failure =
        RaiseFailure::unsupportedOpcode(Di, formatName(Di.TsFlags,
                                                        Di.Inst.getOpcode()));
    Result.Success = false;
    return Result;
  }

  Result.Success = true;
  return Result;
}

} // namespace COMGR::hotswap
