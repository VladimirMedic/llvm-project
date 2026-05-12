//===- RaiserScaffoldingTest.cpp - Hotswap transpiler scaffolding test ----===//
//
// Part of Comgr, under the Apache License v2.0 with LLVM Exceptions. See
// amd/comgr/LICENSE.TXT in this repository for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Pins the scaffolding contract `raiseToIR` advertises:
//
//   * Empty text bytes + valid SourceISA + KernelName +
//     HasKernelDescriptor=true: the production raiser walks zero
//     instructions, emits a single AMDGPU_KERNEL function with
//     `ret void`, and returns a populated `RaiseResult` wrapped in
//     an `llvm::Expected`.
//
//   * Invalid input (empty SourceISA, malformed ISA, missing KD):
//     raiseToIR aborts early and surfaces a `HotswapError` subclass
//     via `llvm::Expected<>`'s error channel.
//
// Failures are categorised `HotswapError` ErrorInfo subclasses (see
// hotswap-error.h). Tests assert on the failure category via
// `Err.isA<HotswapError>()`.
//
//===----------------------------------------------------------------------===//

#include "hotswap/hotswap-error.h"
#include "hotswap/raiser.h"

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CallingConv.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/raw_ostream.h"

#include "gtest/gtest.h"

namespace {

// Build a KernelMeta with HasKernelDescriptor=true so non-empty-input
// raiseToIR calls don't trip the KD precondition. Tests that want to
// exercise the missing-KD rejection construct their own KernelMeta
// directly.
COMGR::hotswap::KernelMeta makeKernelMeta(llvm::StringRef Name) {
  COMGR::hotswap::KernelMeta Meta;
  Meta.Name = Name.str();
  Meta.HasKernelDescriptor = true;
  return Meta;
}

bool isHotswapError(const llvm::Error &E) {
  return E.isA<COMGR::hotswap::HotswapError>();
}

} // namespace

TEST(RaiserScaffolding, ValidInputProducesValidModule) {
  COMGR::hotswap::KernelMeta Meta = makeKernelMeta("kernel");
  llvm::Expected<COMGR::hotswap::RaiseResult> Result =
      COMGR::hotswap::raiseToIR({}, "gfx942", "kernel", Meta);

  ASSERT_TRUE(static_cast<bool>(Result)) << llvm::toString(Result.takeError());
  ASSERT_NE(Result->Ctx, nullptr);
  ASSERT_NE(Result->Module, nullptr);

  std::string Err;
  llvm::raw_string_ostream ErrStream(Err);
  EXPECT_FALSE(llvm::verifyModule(*Result->Module, &ErrStream)) << Err;
}

TEST(RaiserScaffolding, ModuleAdvertisesAMDGPUTriple) {
  COMGR::hotswap::KernelMeta Meta = makeKernelMeta("kernel");
  llvm::Expected<COMGR::hotswap::RaiseResult> Result =
      COMGR::hotswap::raiseToIR({}, "gfx942", "kernel", Meta);

  ASSERT_TRUE(static_cast<bool>(Result)) << llvm::toString(Result.takeError());
  ASSERT_NE(Result->Module, nullptr);
  EXPECT_EQ(Result->Module->getTargetTriple().str(), "amdgcn-amd-amdhsa");
}

TEST(RaiserScaffolding, KernelFunctionIsAMDGPUKernelWithRetVoid) {
  COMGR::hotswap::KernelMeta Meta = makeKernelMeta("kernel");
  llvm::Expected<COMGR::hotswap::RaiseResult> Result =
      COMGR::hotswap::raiseToIR({}, "gfx942", "kernel", Meta);

  ASSERT_TRUE(static_cast<bool>(Result)) << llvm::toString(Result.takeError());
  llvm::Function *Fn = Result->Module->getFunction("kernel");
  ASSERT_NE(Fn, nullptr);
  EXPECT_EQ(Fn->getCallingConv(), llvm::CallingConv::AMDGPU_KERNEL);
  ASSERT_EQ(Fn->size(), 1u);
  llvm::BasicBlock &Entry = Fn->getEntryBlock();
  ASSERT_FALSE(Entry.empty());
  EXPECT_TRUE(llvm::isa<llvm::ReturnInst>(Entry.getTerminator()));
}

TEST(RaiserScaffolding, MalformedTargetIsaIsRejected) {
  COMGR::hotswap::KernelMeta Meta = makeKernelMeta("kernel");
  llvm::Expected<COMGR::hotswap::RaiseResult> Result =
      COMGR::hotswap::raiseToIR({}, "not-a-real-isa", "kernel", Meta);

  ASSERT_FALSE(static_cast<bool>(Result));
  llvm::Error Err = Result.takeError();
  EXPECT_TRUE(isHotswapError(Err));
  llvm::consumeError(std::move(Err));
}

TEST(RaiserScaffolding, EmptyTargetIsaIsRejected) {
  COMGR::hotswap::KernelMeta Meta = makeKernelMeta("kernel");
  llvm::Expected<COMGR::hotswap::RaiseResult> Result =
      COMGR::hotswap::raiseToIR({}, "", "kernel", Meta);

  ASSERT_FALSE(static_cast<bool>(Result));
  llvm::Error Err = Result.takeError();
  EXPECT_TRUE(isHotswapError(Err));
  llvm::consumeError(std::move(Err));
}

// Non-empty input with HasKernelDescriptor=false is rejected -- this
// is the production precondition the raiser enforces for any real
// lift request.
TEST(RaiserScaffolding, MissingKernelDescriptorIsRejected) {
  COMGR::hotswap::KernelMeta Meta;
  Meta.Name = "kernel";
  Meta.HasKernelDescriptor = false;
  llvm::Expected<COMGR::hotswap::RaiseResult> Result =
      COMGR::hotswap::raiseToIR({}, "gfx942", "kernel", Meta);

  ASSERT_FALSE(static_cast<bool>(Result));
  llvm::Error Err = Result.takeError();
  EXPECT_TRUE(isHotswapError(Err));
  llvm::consumeError(std::move(Err));
}
