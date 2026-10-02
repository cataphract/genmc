; Lengths bounded only by their value range need no bounds check. memcmp may
; also be declared with a non-int result, e.g. by non-C frontends.
@left = global [4 x i8] c"\01\02\03\04"
@right = global [4 x i8] c"\01\02\09\04"
@copy = global [4 x i8] zeroinitializer
@length = global i64 7
@message = private constant [5 x i8] c"test\00"

declare i64 @memcmp(ptr, ptr, i64)
declare void @llvm.memcpy.p0.p0.i64(ptr, ptr, i64, i1)
declare void @__VERIFIER_assert_fail(ptr, ptr, i32)

define i32 @main() {
  %raw = load volatile i64, ptr @length
  %length = and i64 %raw, 3
  call void @llvm.memcpy.p0.p0.i64(ptr @copy, ptr @left, i64 %length, i1 false)
  %copied = load i8, ptr getelementptr (i8, ptr @copy, i64 2)
  %untouched = load i8, ptr getelementptr (i8, ptr @copy, i64 3)
  %copyCorrect = icmp eq i8 %copied, 3
  %suffixCorrect = icmp eq i8 %untouched, 0
  %difference = call i64 @memcmp(ptr @left, ptr @right, i64 %length)
  %compareCorrect = icmp slt i64 %difference, 0
  %partial = and i1 %copyCorrect, %suffixCorrect
  %correct = and i1 %partial, %compareCorrect
  br i1 %correct, label %done, label %failed

failed:
  call void @__VERIFIER_assert_fail(ptr @message, ptr @message, i32 0)
  unreachable

done:
  ret i32 0
}
