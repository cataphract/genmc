target datalayout = "e-m:e-p:64:64-i64:64-i128:128-n8:16:32:64-S128"

; A memset shorter than its destination type only sets the requested
; prefix, and fields are set to the byte splatted over their width.
%pair = type { i64, i64 }

@message = private constant [5 x i8] c"test\00"

declare void @llvm.memset.p0.i64(ptr, i8, i64, i1)
declare void @__VERIFIER_assert_fail(ptr, ptr, i32)

define i32 @main() {
  %pair = alloca %pair, align 8
  %secondPtr = getelementptr inbounds %pair, ptr %pair, i64 0, i32 1
  store i64 7, ptr %secondPtr, align 8
  call void @llvm.memset.p0.i64(ptr align 8 %pair, i8 1, i64 8, i1 false)
  %first = load i64, ptr %pair, align 8
  %firstCorrect = icmp eq i64 %first, 72340172838076673
  %second = load i64, ptr %secondPtr, align 8
  %secondCorrect = icmp eq i64 %second, 7
  %correct = and i1 %firstCorrect, %secondCorrect
  br i1 %correct, label %done, label %failed

failed:
  call void @__VERIFIER_assert_fail(ptr @message, ptr @message, i32 0)
  unreachable

done:
  ret i32 0
}
