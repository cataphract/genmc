target datalayout = "e-p:64:64-i64:64-n8:16:32:64"

@table = private constant [2 x ptr] [ptr @leaf, ptr @walk]
@index = global i64 0
@next = global i32 1
@message = private constant [5 x i8] c"test\00"

declare void @__VERIFIER_assert_fail(ptr, ptr, i32)

define internal i32 @leaf(ptr %state) {
  %value = load i32, ptr %state
  ret i32 %value
}

; @walk can reach itself through @table. Promoting that dispatch would make it
; directly recursive, and the inliner would no longer inline it into @main.
; @main's local is then accessed with mixed sizes instead of being promoted.
define internal i32 @walk(ptr %state) {
  %low = load i32, ptr %state
  %index = load i64, ptr @index
  %entry = getelementptr inbounds [2 x ptr], ptr @table, i64 0, i64 %index
  %callee = load ptr, ptr %entry
  %next = call i32 %callee(ptr @next)
  %sum = add i32 %low, %next
  ret i32 %sum
}

define i32 @main() {
  %state = alloca i64
  store i64 4294967298, ptr %state
  %value = call i32 @walk(ptr %state)
  %correct = icmp eq i32 %value, 3
  br i1 %correct, label %done, label %failed

failed:
  call void @__VERIFIER_assert_fail(ptr @message, ptr @message, i32 0)
  unreachable

done:
  ret i32 0
}
