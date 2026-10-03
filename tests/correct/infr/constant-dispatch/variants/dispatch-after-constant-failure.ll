target datalayout = "e-p:64:64-i64:64-n8:16:32:64"

@table = private constant [2 x ptr] [ptr @first, ptr @second]
@index = global i64 0
@enabled = global i32 0
@message = private constant [5 x i8] c"test\00"

declare void @__VERIFIER_assert_fail(ptr, ptr, i32)

define internal i32 @first() {
  ret i32 1
}

define internal i32 @second() {
  ret i32 2
}

; The first dispatch reads past the end of the table at a constant offset, so it
; always fails and the rest of its block is dead. That dead code holds another
; dispatch, and values that the dispatches in later blocks use.
define i32 @main() {
entry:
  %enabled = load i32, ptr @enabled
  %disabled = icmp eq i32 %enabled, 0
  br i1 %disabled, label %done, label %fail

fail:
  %invalid = load ptr, ptr getelementptr inbounds ([2 x ptr], ptr @table, i64 0, i64 5)
  %ignored = call i32 %invalid()
  %index = load i64, ptr @index
  %slot = getelementptr inbounds [2 x ptr], ptr @table, i64 0, i64 %index
  %callee = load ptr, ptr %slot
  %erased = call i32 %callee()
  br label %dead

dead:
  %value = call i32 %callee()
  %indexed = getelementptr inbounds [2 x ptr], ptr @table, i64 0, i64 %index
  %other = load ptr, ptr %indexed
  %next = call i32 %other()
  %sum = add i32 %value, %next
  %correct = icmp eq i32 %sum, 2
  br i1 %correct, label %done, label %failed

failed:
  call void @__VERIFIER_assert_fail(ptr @message, ptr @message, i32 0)
  unreachable

done:
  ret i32 0
}
