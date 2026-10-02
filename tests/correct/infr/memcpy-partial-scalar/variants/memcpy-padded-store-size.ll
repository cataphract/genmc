target datalayout = "e-m:e-p:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"

; Fields whose store size is smaller than their allocation size are accessed
; with their allocation size; copying the gap again would make later field
; loads mixed-size.
%odd = type { i24, i8 }
%long = type { x86_fp80 }

@oddSource = global %odd { i24 1193046, i8 7 }
@oddDestination = global %odd zeroinitializer
@longSource = global %long { x86_fp80 0xK3FFF8000000000000000 }
@longDestination = global %long zeroinitializer
@message = private constant [5 x i8] c"test\00"

declare void @llvm.memcpy.p0.p0.i64(ptr, ptr, i64, i1)
declare void @__VERIFIER_assert_fail(ptr, ptr, i32)

define i32 @main() {
  call void @llvm.memcpy.p0.p0.i64(ptr @oddDestination, ptr @oddSource, i64 8, i1 false)
  %odd = load i24, ptr @oddDestination
  %oddCorrect = icmp eq i24 %odd, 1193046
  call void @llvm.memcpy.p0.p0.i64(ptr @longDestination, ptr @longSource, i64 16, i1 false)
  ; The interpreter cannot represent x86_fp80 values; only check the access.
  %long = load x86_fp80, ptr @longDestination
  br i1 %oddCorrect, label %done, label %failed

failed:
  call void @__VERIFIER_assert_fail(ptr @message, ptr @message, i32 0)
  unreachable

done:
  ret i32 0
}
