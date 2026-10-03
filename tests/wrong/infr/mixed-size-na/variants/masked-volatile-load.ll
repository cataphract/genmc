target datalayout = "e-p:64:64-i64:64-n8:16:32:64"

@active = global i1 false
@output = global i32 0

; Volatile accesses must execute even when their value is masked. They also
; make the storage escape; masked-atomic-load covers non-simple loads of
; storage that capture tracking accepts.
define i32 @main() {
  %storage = alloca ptr
  store i8 6, ptr %storage
  %active = load i1, ptr @active
  %pointer = load volatile ptr, ptr %storage
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
