target datalayout = "e-p:64:64-i64:64-n8:16:32:64"

@table = global [2 x ptr] [ptr @first, ptr @second]
@index = global i64 1
@message = private constant [5 x i8] c"test\00"

declare void @__VERIFIER_assert_fail(ptr, ptr, i32)

define internal i32 @first() {
  ret i32 1
}

define internal i32 @second() {
  ret i32 2
}

define internal i32 @replacement() {
  ret i32 3
}

; A mutable table's initializer is not an exhaustive set of call targets.
define i32 @main() {
  %entry = getelementptr inbounds [2 x ptr], ptr @table, i64 0, i64 1
  store ptr @replacement, ptr %entry
  %index = load i64, ptr @index
  %selected = getelementptr inbounds [2 x ptr], ptr @table, i64 0, i64 %index
  %callee = load ptr, ptr %selected
  %value = call i32 %callee()
  %correct = icmp eq i32 %value, 3
  br i1 %correct, label %done, label %failed

failed:
  call void @__VERIFIER_assert_fail(ptr @message, ptr @message, i32 0)
  unreachable

done:
  ret i32 0
}
