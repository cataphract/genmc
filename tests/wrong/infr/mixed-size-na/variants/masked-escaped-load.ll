target datalayout = "e-p:64:64-i64:64-n8:16:32:64"

@published = global ptr null
@active = global i1 false
@output = global i32 0

; An alloca whose address escapes is not proven thread-private.
define i32 @main() {
  %storage = alloca ptr
  store ptr %storage, ptr @published
  store i8 6, ptr %storage
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
