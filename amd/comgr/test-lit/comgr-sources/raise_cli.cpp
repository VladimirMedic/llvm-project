//===- raise_cli.cpp - Hotswap transpiler ---------------------------------===//
//
// Part of Comgr, under the Apache License v2.0 with LLVM Exceptions. See
// amd/comgr/LICENSE.TXT in this repository for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// Per-file raiser CLI used by the hotswap-transpile/ lit fixtures.
//
// Usage:
//   raise_cli <code-object.co|.hsaco> --emit-ir[=<kernel>]
//             [--isa=<arch>] [--target-isa=<arch>]
//
// --emit-ir mode runs `raiseToIR` in-process, dumps the raised LLVM IR
// for a single kernel on stdout, and leaves stderr alone so FileCheck
// can match warnings / abort-gate diagnostics.  Selects the only kernel
// when the code object has one, or requires the `=<kernel>` form when
// there are multiple.  Exits 0 iff the kernel raised successfully;
// non-zero otherwise.
//
// `--target-isa=<arch>` overrides the codegen target ISA (defaults to
// the source ISA).  `--isa=<arch>` overrides the source ISA (defaults to
// auto-detect from the filename, then from the ELF MACH field).

#include "hotswap/code_object_utils.h"
#include "hotswap/raiser.h"

#include "llvm/IR/Module.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

std::string autoDetectIsa(llvm::StringRef Path) {
  for (size_t I = 0; I + 3 < Path.size(); ++I) {
    if (Path[I] == 'g' && Path[I + 1] == 'f' && Path[I + 2] == 'x') {
      size_t J = I + 3;
      while (J < Path.size() &&
             std::isdigit(static_cast<unsigned char>(Path[J])))
        ++J;
      if (J > I + 3) {
        if (J < Path.size() && Path[J] >= 'a' && Path[J] <= 'z')
          ++J;
        return Path.substr(I, J - I).str();
      }
    }
  }
  return {};
}

int usage() {
  std::fprintf(
      stderr,
      "usage:\n"
      "  raise_cli <code-object.co|.hsaco> --emit-ir[=<kernel>] "
      "[--isa=<arch>] [--target-isa=<arch>]\n"
      "\n"
      "--emit-ir mode dumps raised LLVM IR for a single kernel on stdout.\n"
      "  No fork; stderr left alone for FileCheck.\n"
      "ISA is inferred from the filename when --isa is not given.\n");
  return 2;
}

} // namespace

int main(int argc, char **argv) {
  std::string CoPath;
  std::string Isa;
  std::string TargetIsa;
  bool EmitIr = false;
  std::string EmitIrKernel;
  for (int I = 1; I < argc; ++I) {
    std::string A = argv[I];
    if (A.rfind("--isa=", 0) == 0) {
      Isa = A.substr(6);
    } else if (A == "--isa") {
      if (I + 1 >= argc)
        return usage();
      Isa = argv[++I];
    } else if (A.rfind("--target-isa=", 0) == 0) {
      TargetIsa = A.substr(13);
    } else if (A == "--target-isa") {
      if (I + 1 >= argc)
        return usage();
      TargetIsa = argv[++I];
    } else if (A == "--emit-ir") {
      EmitIr = true;
    } else if (A.rfind("--emit-ir=", 0) == 0) {
      EmitIr = true;
      EmitIrKernel = A.substr(10);
    } else if (!A.empty() && A[0] == '-') {
      std::fprintf(stderr, "raise_cli: unknown flag: %s\n", A.c_str());
      return usage();
    } else if (CoPath.empty()) {
      CoPath = A;
    } else {
      std::fprintf(stderr, "raise_cli: unexpected positional arg: %s\n",
                   A.c_str());
      return usage();
    }
  }
  if (CoPath.empty())
    return usage();
  if (!EmitIr) {
    std::fprintf(stderr,
                 "raise_cli: this build only supports --emit-ir mode\n");
    return usage();
  }

  auto BufOrErr = llvm::MemoryBuffer::getFile(CoPath, /*IsText=*/false);
  if (!BufOrErr) {
    std::fprintf(stderr, "raise_cli: cannot read %s: %s\n", CoPath.c_str(),
                 BufOrErr.getError().message().c_str());
    return 2;
  }
  llvm::StringRef CoStr = (*BufOrErr)->getBuffer();
  std::vector<uint8_t> CoData(CoStr.bytes_begin(), CoStr.bytes_end());

  if (Isa.empty()) {
    Isa = autoDetectIsa(CoPath);
    if (Isa.empty())
      Isa = COMGR::hotswap::detectIsaFromElf(CoData);
    if (Isa.empty()) {
      std::fprintf(stderr,
                   "raise_cli: could not infer ISA from %s; pass --isa=<arch>\n",
                   CoPath.c_str());
      return 2;
    }
  }

  auto KernelNames = COMGR::hotswap::listKernelNames(CoData);
  if (KernelNames.empty()) {
    std::fprintf(stderr, "raise_cli: no kernels in %s\n", CoPath.c_str());
    return 2;
  }

  auto Text = COMGR::hotswap::extractTextSection(CoData);
  if (!Text.Valid) {
    std::fprintf(stderr, "raise_cli: could not extract .text from %s\n",
                 CoPath.c_str());
    return 2;
  }

  std::string Target;
  if (EmitIrKernel.empty()) {
    if (KernelNames.size() != 1) {
      std::fprintf(stderr,
                   "raise_cli: --emit-ir requires =<kernel> when the "
                   "code object has %zu kernels\n",
                   KernelNames.size());
      return 2;
    }
    Target = KernelNames.front();
  } else {
    bool Found = false;
    for (const auto &Kn : KernelNames)
      if (Kn == EmitIrKernel) {
        Target = Kn;
        Found = true;
        break;
      }
    if (!Found) {
      std::fprintf(stderr, "raise_cli: kernel '%s' not found in %s\n",
                   EmitIrKernel.c_str(), CoPath.c_str());
      return 2;
    }
  }

  auto Meta = COMGR::hotswap::extractKernelMeta(CoData, Target);
  auto KernelOffsetOrErr =
      COMGR::hotswap::findKernelSymbolOffset(CoData, Target);
  if (!KernelOffsetOrErr) {
    std::string Err = llvm::toString(KernelOffsetOrErr.takeError());
    std::fprintf(stderr,
                 "raise_cli: kernel '%s' offset lookup failed: %s\n",
                 Target.c_str(), Err.c_str());
    return 1;
  }
  uint64_t KernelOffset = *KernelOffsetOrErr;

  auto Raised = COMGR::hotswap::raiseToIR(Text.Bytes, Isa, Target, Meta,
                                          KernelOffset, TargetIsa);
  if (!Raised.Success) {
    std::fprintf(stderr,
                 "raise_cli: kernel '%s' failed to raise: %s [%s]"
                 " @offset=0x%llx%s%s\n",
                 Target.c_str(),
                 Raised.Failure.Mnemonic.empty()
                     ? "unknown"
                     : Raised.Failure.Mnemonic.c_str(),
                 Raised.Failure.Format.empty()
                     ? "unknown"
                     : Raised.Failure.Format.c_str(),
                 static_cast<unsigned long long>(Raised.Failure.Offset),
                 Raised.Failure.Detail.empty() ? "" : " :: ",
                 Raised.Failure.Detail.empty()
                     ? ""
                     : Raised.Failure.Detail.c_str());
    return 1;
  }

  llvm::raw_fd_ostream Os(/*fd=*/1, /*shouldClose=*/false);
  Raised.Module->print(Os, /*AAW=*/nullptr);
  return 0;
}
