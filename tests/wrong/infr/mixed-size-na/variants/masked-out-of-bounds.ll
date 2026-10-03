target datalayout = "e-p:64:64-i64:64-n8:16:32:64"

@active = global i1 false
@output = global i32 0

define internal void @fill(ptr nocapture %storage, i32 %depth) {
  %again = icmp ne i32 %depth, 0
  br i1 %again, label %recurse, label %initialize
recurse:
  %next = sub i32 %depth, 1
  call void @fill(ptr %storage, i32 %next)
  ret void
initialize:
  store i8 6, ptr %storage
  ret void
}

; A pointer-width load of one-byte storage is out of bounds even when masked.
; The alignment passes, so only the bounds check rejects it.
define i32 @main() {
  %storage = alloca i8, align 8
  call void @fill(ptr %storage, i32 1)
  %active = load i1, ptr @active
  %pointer = load ptr, ptr %storage
  %nonnull = icmp ne ptr %pointer, null
  %use = select i1 %active, i1 %nonnull, i1 false
  br i1 %use, label %consume, label %done
consume:
  %value = load i32, ptr %pointer
  store i32 %value, ptr @output
  br label %done
done:
  ret i32 0
}
