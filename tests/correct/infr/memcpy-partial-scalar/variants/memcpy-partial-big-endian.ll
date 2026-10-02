target datalayout = "E-p:64:64-i64:64-n8:16:32:64"

@source = global i64 72623859790382856, align 1
@message = private constant [5 x i8] c"test\00"

declare void @llvm.memcpy.p0.p0.i64(ptr, ptr, i64, i1)
declare void @__VERIFIER_assert_fail(ptr, ptr, i32)

; Copy the first three bytes of 0x0102030405060708 into an unaligned scalar.
define i32 @main() {
  %destination = alloca i64, align 1
  store i64 1234605616436508552, ptr %destination, align 1
  call void @llvm.memcpy.p0.p0.i64(ptr align 1 %destination, ptr align 1 @source, i64 3, i1 false)
  %value = load i64, ptr %destination, align 1
  %correct = icmp eq i64 %value, 72624136016787336
  br i1 %correct, label %done, label %failed

failed:
  call void @__VERIFIER_assert_fail(ptr @message, ptr @message, i32 0)
  unreachable

done:
  ret i32 0
}
