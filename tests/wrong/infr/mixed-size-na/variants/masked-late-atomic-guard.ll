target datalayout = "e-p:64:64-i64:64-n8:16:32:64"

@payload = global i32 42

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

; An atomic discriminator read is not a plain private read, even of
; non-escaping storage, and must block delaying the speculative load.
define i32 @main() {
  %storage = alloca { ptr, i8 }
  call void @fill(ptr %storage, i32 1, i1 false)
  %flag = getelementptr { ptr, i8 }, ptr %storage, i32 0, i32 1
  %pointer = load ptr, ptr %storage
  %tag = load atomic i8, ptr %flag monotonic, align 1
  %has_value = trunc i8 %tag to i1
  %nonnull = icmp ne ptr %pointer, null
  %destroy = select i1 %has_value, i1 %nonnull, i1 false
  br i1 %destroy, label %consume, label %done
consume:
  %value = load i32, ptr %pointer
  br label %done
done:
  ret i32 0
}
