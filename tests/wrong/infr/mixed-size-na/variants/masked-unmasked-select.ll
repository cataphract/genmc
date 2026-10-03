target datalayout = "e-p:64:64-i64:64-n8:16:32:64"

@active = global i1 false
@output = global i64 0

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

; With a true false-operand, the pointer is used when the discriminator is false.
define i32 @main() {
  %storage = alloca ptr
  call void @fill(ptr %storage, i32 1)
  %active = load i1, ptr @active
  %pointer = load ptr, ptr %storage
  %nonnull = icmp ne ptr %pointer, null
  %use = select i1 %active, i1 %nonnull, i1 true
  br i1 %use, label %consume, label %done
consume:
  %bits = ptrtoint ptr %pointer to i64
  store i64 %bits, ptr @output
  br label %done
done:
  ret i32 0
}
