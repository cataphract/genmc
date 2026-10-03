target datalayout = "e-p:64:64-i64:64-n8:16:32:64"

@table = private constant [2 x ptr] [ptr @first, ptr @second]
@index = global i64 0
@value = global i32 1
@message = private constant [5 x i8] c"test\00"

declare void @__VERIFIER_assert_fail(ptr, ptr, i32)

define internal i32 @first() !dbg !5 {
  %value = load i32, ptr @value, !dbg !9
  ret i32 %value, !dbg !9
}

define internal i32 @second() !dbg !10 {
  %value = load i32, ptr @value, !dbg !11
  %next = add i32 %value, 1, !dbg !11
  ret i32 %next, !dbg !11
}

; The indirect calls have no location, but the direct calls replacing them are
; inlinable calls in a function with debug info, which require one. The first
; dispatch selects its target at run time, the second at a constant offset.
; GenMC verifies the transformed module only in builds with assertions, and
; checks the direct calls' locations itself in all builds.
define i32 @main() !dbg !12 {
  %index = load i64, ptr @index, !dbg !13
  %entry = getelementptr inbounds [2 x ptr], ptr @table, i64 0, i64 %index, !dbg !13
  %callee = load ptr, ptr %entry, !dbg !13
  %value = call i32 %callee()
  %constant = load ptr, ptr getelementptr inbounds ([2 x ptr], ptr @table, i64 0, i64 1), !dbg !13
  %other = call i32 %constant()
  %sum = add i32 %value, %other, !dbg !14
  %correct = icmp eq i32 %sum, 3, !dbg !14
  br i1 %correct, label %done, label %failed, !dbg !14

failed:
  call void @__VERIFIER_assert_fail(ptr @message, ptr @message, i32 0), !dbg !14
  unreachable, !dbg !14

done:
  ret i32 0, !dbg !15
}

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!2, !3}

!0 = distinct !DICompileUnit(language: DW_LANG_C11, file: !1, producer: "hand-written", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug)
!1 = !DIFile(filename: "dispatch-without-location.c", directory: "")
!2 = !{i32 7, !"Dwarf Version", i32 5}
!3 = !{i32 2, !"Debug Info Version", i32 3}
!4 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!5 = distinct !DISubprogram(name: "first", scope: !1, file: !1, line: 1, type: !6, scopeLine: 1, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition, unit: !0)
!6 = !DISubroutineType(types: !7)
!7 = !{!4}
!9 = !DILocation(line: 1, column: 1, scope: !5)
!10 = distinct !DISubprogram(name: "second", scope: !1, file: !1, line: 2, type: !6, scopeLine: 2, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition, unit: !0)
!11 = !DILocation(line: 2, column: 1, scope: !10)
!12 = distinct !DISubprogram(name: "main", scope: !1, file: !1, line: 3, type: !6, scopeLine: 3, spFlags: DISPFlagDefinition, unit: !0)
!13 = !DILocation(line: 4, column: 1, scope: !12)
!14 = !DILocation(line: 5, column: 1, scope: !12)
!15 = !DILocation(line: 6, column: 1, scope: !12)
