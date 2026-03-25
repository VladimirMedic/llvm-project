; RUN: llc -mtriple=amdgcn-amd-amdhsa -mcpu=gfx1100 -O0 -stop-after=finalize-isel < %s | FileCheck %s

; Test that DBG_VALUE instructions for divergent values (VGPRs) get the
; lane-specific byte offset operations added to their DIExpression.
; This is needed for correct debug info in SIMT execution model where
; each lane has its own value in the VGPR.

; CHECK-LABEL: name: test_vgpr_dbg_value
; CHECK: DBG_VALUE %{{[0-9]+}}, $noreg, !{{[0-9]+}}, !DIExpression(DIOpArg(0, i32), DIOpPushLane(i32), DIOpConstant(i32 4), DIOpMul(), DIOpByteOffset(i32))

define amdgpu_kernel void @test_vgpr_dbg_value(ptr addrspace(1) %out) #0 !dbg !5 {
entry:
  %tid = call i32 @llvm.amdgcn.workitem.id.x(), !dbg !11
  %val = shl i32 %tid, 2, !dbg !12
    #dbg_value(i32 %val, !9, !DIExpression(DIOpArg(0, i32)), !12)
  store i32 %val, ptr addrspace(1) %out, align 4, !dbg !13
  ret void, !dbg !14
}

; Also test with 64-bit values to verify correct shift amount calculation
; CHECK-LABEL: name: test_vgpr_dbg_value_i64
; CHECK: DBG_VALUE %{{[0-9]+}}, $noreg, !{{[0-9]+}}, !DIExpression(DIOpArg(0, i64), DIOpPushLane(i32), DIOpConstant(i32 8), DIOpMul(), DIOpByteOffset(i64))

define amdgpu_kernel void @test_vgpr_dbg_value_i64(ptr addrspace(1) %out) #0 !dbg !15 {
entry:
  %tid = call i32 @llvm.amdgcn.workitem.id.x(), !dbg !20
  %tid64 = zext i32 %tid to i64, !dbg !21
  %val = shl i64 %tid64, 2, !dbg !22
    #dbg_value(i64 %val, !18, !DIExpression(DIOpArg(0, i64)), !22)
  store i64 %val, ptr addrspace(1) %out, align 8, !dbg !23
  ret void, !dbg !24
}

; Test with float type
; CHECK-LABEL: name: test_vgpr_dbg_value_float
; CHECK: DBG_VALUE %{{[0-9]+}}, $noreg, !{{[0-9]+}}, !DIExpression(DIOpArg(0, float), DIOpPushLane(i32), DIOpConstant(i32 4), DIOpMul(), DIOpByteOffset(float))

define amdgpu_kernel void @test_vgpr_dbg_value_float(ptr addrspace(1) %out) #0 !dbg !25 {
entry:
  %tid = call i32 @llvm.amdgcn.workitem.id.x(), !dbg !30
  %tidf = uitofp i32 %tid to float, !dbg !31
  %val = fmul float %tidf, 2.0, !dbg !32
    #dbg_value(float %val, !28, !DIExpression(DIOpArg(0, float)), !32)
  store float %val, ptr addrspace(1) %out, align 4, !dbg !33
  ret void, !dbg !34
}

; Test that SGPR (uniform) values do NOT get lane-specific operations
; CHECK-LABEL: name: test_sgpr_dbg_value
; CHECK: DBG_VALUE %{{[0-9]+}}, $noreg, !{{[0-9]+}}, !DIExpression(DIOpArg(0, i32)), debug-location

define amdgpu_kernel void @test_sgpr_dbg_value(ptr addrspace(1) %out, i32 %uniform_val) #0 !dbg !35 {
entry:
  %val = add i32 %uniform_val, 1, !dbg !40
    #dbg_value(i32 %val, !38, !DIExpression(DIOpArg(0, i32)), !40)
  store i32 %val, ptr addrspace(1) %out, align 4, !dbg !41
  ret void, !dbg !42
}

; Test with i16 type - lane stride is 4 bytes (VGPR_32 register width),
; not 2 bytes (type size), because i16 occupies a full 32-bit VGPR lane.
; CHECK-LABEL: name: test_vgpr_dbg_value_i16
; CHECK: DBG_VALUE %{{[0-9]+}}, $noreg, !{{[0-9]+}}, !DIExpression(DIOpArg(0, i16), DIOpPushLane(i32), DIOpConstant(i32 4), DIOpMul(), DIOpByteOffset(i16))

define amdgpu_kernel void @test_vgpr_dbg_value_i16(ptr addrspace(1) %out) #0 !dbg !43 {
entry:
  %tid = call i32 @llvm.amdgcn.workitem.id.x(), !dbg !48
  %tid16 = trunc i32 %tid to i16, !dbg !49
  %val = shl i16 %tid16, 1, !dbg !50
    #dbg_value(i16 %val, !46, !DIExpression(DIOpArg(0, i16)), !50)
  store i16 %val, ptr addrspace(1) %out, align 2, !dbg !51
  ret void, !dbg !52
}

; Test with DIOpFragment - lane offset operations should be inserted before Fragment
; Use a 64-bit variable with a 32-bit fragment to avoid "fragment covers entire variable" error
; CHECK-LABEL: name: test_vgpr_dbg_value_with_fragment
; CHECK: DBG_VALUE %{{[0-9]+}}, $noreg, !{{[0-9]+}}, !DIExpression(DIOpArg(0, i32), DIOpPushLane(i32), DIOpConstant(i32 4), DIOpMul(), DIOpByteOffset(i32), DIOpFragment(0, 32))

define amdgpu_kernel void @test_vgpr_dbg_value_with_fragment(ptr addrspace(1) %out) #0 !dbg !53 {
entry:
  %tid = call i32 @llvm.amdgcn.workitem.id.x(), !dbg !58
  %val = shl i32 %tid, 2, !dbg !59
    #dbg_value(i32 %val, !56, !DIExpression(DIOpArg(0, i32), DIOpFragment(0, 32)), !59)
  store i32 %val, ptr addrspace(1) %out, align 4, !dbg !60
  ret void, !dbg !61
}

; Test with old-format DIExpression (DWARF ops) - should NOT be modified
; CHECK-LABEL: name: test_old_diexpression
; CHECK: DBG_VALUE %{{[0-9]+}}, $noreg, !{{[0-9]+}}, !DIExpression(), debug-location

define amdgpu_kernel void @test_old_diexpression(ptr addrspace(1) %out) #0 !dbg !62 {
entry:
  %tid = call i32 @llvm.amdgcn.workitem.id.x(), !dbg !67
  %val = shl i32 %tid, 2, !dbg !68
    #dbg_value(i32 %val, !65, !DIExpression(), !68)
  store i32 %val, ptr addrspace(1) %out, align 4, !dbg !69
  ret void, !dbg !70
}

; Test with pointer type - 64-bit pointer uses size 8 bytes
; Note: DIOpByteOffset uses i64 because getTypeForEVT returns integer type for pointers
; CHECK-LABEL: name: test_vgpr_dbg_value_ptr
; CHECK: DBG_VALUE %{{[0-9]+}}, $noreg, !{{[0-9]+}}, !DIExpression(DIOpArg(0, ptr addrspace(1)), DIOpPushLane(i32), DIOpConstant(i32 8), DIOpMul(), DIOpByteOffset(i64))

define amdgpu_kernel void @test_vgpr_dbg_value_ptr(ptr addrspace(1) %base, ptr addrspace(1) %out) #0 !dbg !71 {
entry:
  %tid = call i32 @llvm.amdgcn.workitem.id.x(), !dbg !76
  %tid64 = zext i32 %tid to i64, !dbg !77
  %ptr = getelementptr i8, ptr addrspace(1) %base, i64 %tid64, !dbg !78
    #dbg_value(ptr addrspace(1) %ptr, !74, !DIExpression(DIOpArg(0, ptr addrspace(1))), !78)
  store ptr addrspace(1) %ptr, ptr addrspace(1) %out, align 8, !dbg !79
  ret void, !dbg !80
}

; Test with wave64 - lane stride is still 4 bytes (register-based, not wave-based)
; CHECK-LABEL: name: test_vgpr_dbg_value_wave64
; CHECK: DBG_VALUE %{{[0-9]+}}, $noreg, !{{[0-9]+}}, !DIExpression(DIOpArg(0, i32), DIOpPushLane(i32), DIOpConstant(i32 4), DIOpMul(), DIOpByteOffset(i32))

define amdgpu_kernel void @test_vgpr_dbg_value_wave64(ptr addrspace(1) %out) #2 !dbg !81 {
entry:
  %tid = call i32 @llvm.amdgcn.workitem.id.x(), !dbg !85
  %val = shl i32 %tid, 2, !dbg !86
    #dbg_value(i32 %val, !83, !DIExpression(DIOpArg(0, i32)), !86)
  store i32 %val, ptr addrspace(1) %out, align 4, !dbg !87
  ret void, !dbg !88
}

declare i32 @llvm.amdgcn.workitem.id.x() #1

attributes #0 = { nounwind "target-features"="+wavefrontsize32" }
attributes #1 = { nounwind readnone speculatable }
attributes #2 = { nounwind "target-features"="+wavefrontsize64" }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!3, !4}

!0 = distinct !DICompileUnit(language: DW_LANG_C99, file: !1, producer: "clang", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug, enums: !2)
!1 = !DIFile(filename: "test.cl", directory: "/tmp")
!2 = !{}
!3 = !{i32 7, !"Dwarf Version", i32 5}
!4 = !{i32 2, !"Debug Info Version", i32 3}

; Function 1: test_vgpr_dbg_value
!5 = distinct !DISubprogram(name: "test_vgpr_dbg_value", scope: !1, file: !1, line: 1, type: !6, isLocal: false, isDefinition: true, scopeLine: 1, flags: DIFlagPrototyped, isOptimized: false, unit: !0, retainedNodes: !8)
!6 = !DISubroutineType(types: !7)
!7 = !{null}
!8 = !{!9}
!9 = !DILocalVariable(name: "val", scope: !5, file: !1, line: 2, type: !10)
!10 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!11 = !DILocation(line: 1, column: 1, scope: !5)
!12 = !DILocation(line: 2, column: 1, scope: !5)
!13 = !DILocation(line: 3, column: 1, scope: !5)
!14 = !DILocation(line: 4, column: 1, scope: !5)

; Function 2: test_vgpr_dbg_value_i64
!15 = distinct !DISubprogram(name: "test_vgpr_dbg_value_i64", scope: !1, file: !1, line: 10, type: !6, isLocal: false, isDefinition: true, scopeLine: 10, flags: DIFlagPrototyped, isOptimized: false, unit: !0, retainedNodes: !16)
!16 = !{!18}
!17 = !DIBasicType(name: "long", size: 64, encoding: DW_ATE_signed)
!18 = !DILocalVariable(name: "val64", scope: !15, file: !1, line: 11, type: !17)
!20 = !DILocation(line: 10, column: 1, scope: !15)
!21 = !DILocation(line: 11, column: 1, scope: !15)
!22 = !DILocation(line: 12, column: 1, scope: !15)
!23 = !DILocation(line: 13, column: 1, scope: !15)
!24 = !DILocation(line: 14, column: 1, scope: !15)

; Function 3: test_vgpr_dbg_value_float
!25 = distinct !DISubprogram(name: "test_vgpr_dbg_value_float", scope: !1, file: !1, line: 20, type: !6, isLocal: false, isDefinition: true, scopeLine: 20, flags: DIFlagPrototyped, isOptimized: false, unit: !0, retainedNodes: !26)
!26 = !{!28}
!27 = !DIBasicType(name: "float", size: 32, encoding: DW_ATE_float)
!28 = !DILocalVariable(name: "valf", scope: !25, file: !1, line: 21, type: !27)
!30 = !DILocation(line: 20, column: 1, scope: !25)
!31 = !DILocation(line: 21, column: 1, scope: !25)
!32 = !DILocation(line: 22, column: 1, scope: !25)
!33 = !DILocation(line: 23, column: 1, scope: !25)
!34 = !DILocation(line: 24, column: 1, scope: !25)

; Function 4: test_sgpr_dbg_value (uniform/SGPR value - should NOT have lane offset)
!35 = distinct !DISubprogram(name: "test_sgpr_dbg_value", scope: !1, file: !1, line: 30, type: !6, isLocal: false, isDefinition: true, scopeLine: 30, flags: DIFlagPrototyped, isOptimized: false, unit: !0, retainedNodes: !36)
!36 = !{!38}
!37 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!38 = !DILocalVariable(name: "val", scope: !35, file: !1, line: 31, type: !37)
!40 = !DILocation(line: 31, column: 1, scope: !35)
!41 = !DILocation(line: 32, column: 1, scope: !35)
!42 = !DILocation(line: 33, column: 1, scope: !35)

; Function 5: test_vgpr_dbg_value_i16
!43 = distinct !DISubprogram(name: "test_vgpr_dbg_value_i16", scope: !1, file: !1, line: 40, type: !6, isLocal: false, isDefinition: true, scopeLine: 40, flags: DIFlagPrototyped, isOptimized: false, unit: !0, retainedNodes: !44)
!44 = !{!46}
!45 = !DIBasicType(name: "short", size: 16, encoding: DW_ATE_signed)
!46 = !DILocalVariable(name: "val16", scope: !43, file: !1, line: 41, type: !45)
!48 = !DILocation(line: 40, column: 1, scope: !43)
!49 = !DILocation(line: 41, column: 1, scope: !43)
!50 = !DILocation(line: 42, column: 1, scope: !43)
!51 = !DILocation(line: 43, column: 1, scope: !43)
!52 = !DILocation(line: 44, column: 1, scope: !43)

; Function 6: test_vgpr_dbg_value_with_fragment (64-bit variable with 32-bit fragment)
!53 = distinct !DISubprogram(name: "test_vgpr_dbg_value_with_fragment", scope: !1, file: !1, line: 50, type: !6, isLocal: false, isDefinition: true, scopeLine: 50, flags: DIFlagPrototyped, isOptimized: false, unit: !0, retainedNodes: !54)
!54 = !{!56}
!55 = !DIBasicType(name: "long", size: 64, encoding: DW_ATE_signed)
!56 = !DILocalVariable(name: "valfrag", scope: !53, file: !1, line: 51, type: !55)
!58 = !DILocation(line: 50, column: 1, scope: !53)
!59 = !DILocation(line: 51, column: 1, scope: !53)
!60 = !DILocation(line: 52, column: 1, scope: !53)
!61 = !DILocation(line: 53, column: 1, scope: !53)

; Function 7: test_old_diexpression (uses old format DIExpression)
!62 = distinct !DISubprogram(name: "test_old_diexpression", scope: !1, file: !1, line: 60, type: !6, isLocal: false, isDefinition: true, scopeLine: 60, flags: DIFlagPrototyped, isOptimized: false, unit: !0, retainedNodes: !63)
!63 = !{!65}
!64 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!65 = !DILocalVariable(name: "valold", scope: !62, file: !1, line: 61, type: !64)
!67 = !DILocation(line: 60, column: 1, scope: !62)
!68 = !DILocation(line: 61, column: 1, scope: !62)
!69 = !DILocation(line: 62, column: 1, scope: !62)
!70 = !DILocation(line: 63, column: 1, scope: !62)

; Function 8: test_vgpr_dbg_value_ptr (pointer type)
!71 = distinct !DISubprogram(name: "test_vgpr_dbg_value_ptr", scope: !1, file: !1, line: 70, type: !6, isLocal: false, isDefinition: true, scopeLine: 70, flags: DIFlagPrototyped, isOptimized: false, unit: !0, retainedNodes: !72)
!72 = !{!74}
!73 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !10, size: 64)
!74 = !DILocalVariable(name: "valptr", scope: !71, file: !1, line: 71, type: !73)
!76 = !DILocation(line: 70, column: 1, scope: !71)
!77 = !DILocation(line: 71, column: 1, scope: !71)
!78 = !DILocation(line: 72, column: 1, scope: !71)
!79 = !DILocation(line: 73, column: 1, scope: !71)
!80 = !DILocation(line: 74, column: 1, scope: !71)

; Function 9: test_vgpr_dbg_value_wave64 (wave64 mode)
!81 = distinct !DISubprogram(name: "test_vgpr_dbg_value_wave64", scope: !1, file: !1, line: 80, type: !6, isLocal: false, isDefinition: true, scopeLine: 80, flags: DIFlagPrototyped, isOptimized: false, unit: !0, retainedNodes: !82)
!82 = !{!83}
!83 = !DILocalVariable(name: "valw64", scope: !81, file: !1, line: 81, type: !10)
!85 = !DILocation(line: 80, column: 1, scope: !81)
!86 = !DILocation(line: 81, column: 1, scope: !81)
!87 = !DILocation(line: 82, column: 1, scope: !81)
!88 = !DILocation(line: 83, column: 1, scope: !81)
