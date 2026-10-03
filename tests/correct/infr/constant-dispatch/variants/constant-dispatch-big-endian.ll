target datalayout = "E-p:64:64-i64:64-n8:16:32:64"

@table = private constant [2 x ptr] [ptr @first, ptr @second]
@index = global i64 0
@message = private constant [5 x i8] c"test\00"

declare void @__VERIFIER_assert_fail(ptr, ptr, i32)

define internal i32 @first(ptr %state) {
  %value = load i32, ptr %state
  ret i32 %value
}

define internal i32 @second(ptr %state) {
  %field = getelementptr i8, ptr %state, i64 4
  %value = load i32, ptr %field
  ret i32 %value
}

define i32 @main() {
  %state = alloca i64
  store i64 4294967298, ptr %state
  %index = load i64, ptr @index
  %entry = getelementptr inbounds [2 x ptr], ptr @table, i64 0, i64 %index
  %callee = load ptr, ptr %entry
  %first = call i32 %callee(ptr %state)
  %other = xor i64 %index, 1
  %entry2 = getelementptr inbounds [2 x ptr], ptr @table, i64 0, i64 %other
  %callee2 = load ptr, ptr %entry2
  %second = call i32 %callee2(ptr %state)
  %correct1 = icmp eq i32 %first, 1
  %correct2 = icmp eq i32 %second, 2
  %correct = and i1 %correct1, %correct2
  br i1 %correct, label %done, label %failed

failed:
  call void @__VERIFIER_assert_fail(ptr @message, ptr @message, i32 0)
  unreachable

done:
  ret i32 0
}
