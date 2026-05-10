//===- comgr-env.h - Comgr environment variables --------------------------===//
//
// Part of Comgr, under the Apache License v2.0 with LLVM Exceptions. See
// amd/comgr/LICENSE.TXT in this repository for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef COMGR_ENV_H
#define COMGR_ENV_H

#include "llvm/ADT/StringRef.h"

namespace COMGR {
namespace env {

/// Return whether the environment requests temps be saved.
bool shouldSaveTemps();
bool shouldSaveLLVMTemps();
std::optional<bool> shouldUseVFS();

/// If the environment requests logs be redirected, return the string identifier
/// of where to redirect. Otherwise return @p None.
std::optional<llvm::StringRef> getRedirectLogs();

/// Return whether the environment requests verbose logging.
bool shouldEmitVerboseLogs();

/// Return whether the environment requests time statistics collection.
bool needTimeStatistics();

/// If environment variable LLVM_PATH is set, return the environment variable,
/// otherwise return the default LLVM path.
llvm::StringRef getLLVMPath();

/// Return the clang binary path "<LLVM_PATH>/bin/clang", constructed via
/// Twine concatenation. This matches the path passed to clang's Driver
/// constructor so that clang::GetResourcesPath() yields a resource-dir path
/// matching what comgr embeds into the VFS, regardless of whether LLVM_PATH
/// is set. Using sys::path::append on an empty base produces a relative
/// "bin/clang" instead of "/bin/clang", which would cause the resource dir
/// to drift between Driver and embed code.
std::string getClangBinaryPath();

/// If environment variable AMD_COMGR_CACHE_POLICY is set, return the
/// environment variable, otherwise return empty
llvm::StringRef getCachePolicy();

/// If environment variable AMD_COMGR_CACHE_DIR is set, return the environment
/// variable, otherwise return the default path: On Linux it's typically
/// $HOME/.cache/comgr_cache (depends on XDG_CACHE_HOME)
llvm::StringRef getCacheDirectory();

/// If environment variable AMD_COMGR_DRIVER_OPTIONS_APPEND is set, return the
/// space-separated options to append to clang driver invocations.
llvm::StringRef getDriverOptionsAppend();

/// Override for embedded libc++ header injection.
///   Auto    — detect system C++ headers and skip embedded if found (default).
///   Force   — always inject embedded headers, ignore detection.
///   Disable — never inject embedded headers, regardless of detection.
enum class EmbeddedLibcxxMode { Auto, Force, Disable };

/// Read AMD_COMGR_USE_EMBEDDED_LIBCXX. Defaults to Auto.
EmbeddedLibcxxMode getEmbeddedLibcxxMode();

} // namespace env
} // namespace COMGR

#endif // COMGR_ENV_H
