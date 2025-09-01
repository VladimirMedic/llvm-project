; RUN: llvm-link %s %S/Inputs/amdgpu-no-subarch.ll -o /dev/null 2>&1 | FileCheck --allow-empty %s
; RUN: llvm-link %S/Inputs/amdgpu-no-subarch.ll %s -o /dev/null 2>&1 | FileCheck --allow-empty %s

; RUN: llvm-link %s %S/Inputs/amdgpu10-subarch.ll -o /dev/null 2>&1 | FileCheck -check-prefix=WARN_A %s
; RUN: llvm-link %S/Inputs/amdgpu10-subarch.ll %s -o /dev/null 2>&1 | FileCheck -check-prefix=WARN_B %s

; CHECK-NOT: warning

; Check that there is no warning when linking an amdgpu triple without
; a subarch with one with a subarch.


; WARN_A: warning: Linking two modules of different target triples: '{{.*}}' is 'amdgpu10-amd-amdhsa' whereas 'llvm-link' is 'amdgpu9-amd-amdhsa'
; WARN_B: warning: Linking two modules of different target triples: '{{.*}}' is 'amdgpu9-amd-amdhsa' whereas 'llvm-link' is 'amdgpu10-amd-amdhsa'


target triple = "amdgpu9-amd-amdhsa"

declare i32 @foo()

define i32 @bar() {
  %x = call i32 @foo()
  ret i32 %x
}
