target datalayout = "e-p:64:64-i64:64-n8:16:32:64"

@x = global i32 0, align 4
@y = global i32 0, align 4
@payload = global i32 42, align 4
@sink = global i32 0, align 4

declare i32 @__VERIFIER_thread_create(ptr, ptr, ptr)

; Recursion keeps the storage from being promoted by the inliner/SROA.
define internal void @fill(ptr nocapture %storage, i32 %depth) {
  %again = icmp ne i32 %depth, 0
  br i1 %again, label %recurse, label %initialize
recurse:
  %next = sub i32 %depth, 1
  call void @fill(ptr %storage, i32 %next)
  ret void
initialize:
  store ptr @payload, ptr %storage
  ret void
}

; The discriminator is a private copy of x, loaded after the speculative load.
; Guarding after the discriminator but before the store of y would order the
; store after the read of x (data; rfi; ctrl), forbidding the fourth
; (load-buffering) execution that IMM allows.
define ptr @first(ptr %unused) {
  %storage = alloca { ptr, i8 }
  call void @fill(ptr %storage, i32 1)
  %flag = getelementptr { ptr, i8 }, ptr %storage, i32 0, i32 1
  %read = load atomic i32, ptr @x monotonic, align 4
  %copy = trunc i32 %read to i8
  store i8 %copy, ptr %flag
  %pointer = load ptr, ptr %storage
  %tag = load i8, ptr %flag
  %active = trunc i8 %tag to i1
  store atomic i32 1, ptr @y monotonic, align 4
  %nonnull = icmp ne ptr %pointer, null
  %use = select i1 %active, i1 %nonnull, i1 false
  br i1 %use, label %consume, label %done
consume:
  %value = load i32, ptr %pointer
  store i32 %value, ptr @sink
  br label %done
done:
  ret ptr null
}

define ptr @second(ptr %unused) {
  %read = load atomic i32, ptr @y monotonic, align 4
  store atomic i32 %read, ptr @x monotonic, align 4
  ret ptr null
}

define i32 @main() {
  %first = call i32 @__VERIFIER_thread_create(ptr null, ptr @first, ptr null)
  %second = call i32 @__VERIFIER_thread_create(ptr null, ptr @second, ptr null)
  ret i32 0
}
