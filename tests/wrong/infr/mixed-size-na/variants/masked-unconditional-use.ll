target datalayout = "e-p:64:64-i64:64-n8:16:32:64"

@active = global i1 false
@observed = global ptr null
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

; One masked comparison does not justify suppressing an unmasked use, even one
; after the masked branch.
define i32 @main() {
  %storage = alloca ptr
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
  store ptr %pointer, ptr @observed
  ret i32 0
}
