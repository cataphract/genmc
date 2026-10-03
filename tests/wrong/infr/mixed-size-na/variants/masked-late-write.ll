target datalayout = "e-p:64:64-i64:64-n8:16:32:64"

@payload = global i32 42
@message = private constant [5 x i8] c"test\00"
declare void @__VERIFIER_assert_fail(ptr, ptr, i32)

; Recursion keeps the union storage from being promoted by the inliner/SROA.
define internal void @fill(ptr nocapture %storage, i32 %depth, i1 %success) {
  %again = icmp ne i32 %depth, 0
  br i1 %again, label %recurse, label %initialize
recurse:
  %next = sub i32 %depth, 1
  call void @fill(ptr %storage, i32 %next, i1 %success)
  ret void
initialize:
  %flag = getelementptr { ptr, i8 }, ptr %storage, i32 0, i32 1
  br i1 %success, label %value, label %error
value:
  store ptr @payload, ptr %storage
  store i8 1, ptr %flag
  ret void
error:
  store i8 6, ptr %storage
  store i8 0, ptr %flag
  ret void
}

; Moving the pointer load past these stores would change the loaded value
; and hide a mixed-size access on an active path.
define internal void @exercise(i1 %success) {
  %storage = alloca { ptr, i8 }
  call void @fill(ptr %storage, i32 1, i1 %success)
  %flag = getelementptr { ptr, i8 }, ptr %storage, i32 0, i32 1
  %pointer = load ptr, ptr %storage
  store ptr @payload, ptr %storage
  store i8 1, ptr %flag
  %tag = load i8, ptr %flag, !noundef !0
  %has_value = trunc i8 %tag to i1
  %nonnull = icmp ne ptr %pointer, null
  %destroy = select i1 %has_value, i1 %nonnull, i1 false
  br i1 %destroy, label %consume, label %inactive
consume:
  %value = load i32, ptr %pointer
  %correct = icmp eq i32 %value, 42
  br i1 %correct, label %done, label %failed
inactive:
  %error = load i8, ptr %storage
  %correct_error = icmp eq i8 %error, 6
  br i1 %correct_error, label %done, label %failed
failed:
  call void @__VERIFIER_assert_fail(ptr @message, ptr @message, i32 0)
  unreachable
done:
  ret void
}

define i32 @main() {
  call void @exercise(i1 false)
  call void @exercise(i1 true)
  ret i32 0
}

!0 = !{}
