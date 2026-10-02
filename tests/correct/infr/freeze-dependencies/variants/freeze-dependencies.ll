@x = global i32 0, align 4
@y = global i32 0, align 4

declare i32 @__VERIFIER_thread_create(ptr, ptr, ptr)

; Both stores depend on their preceding reads, even though they write 1.
; IMM must forbid the fourth execution in which both reads see 1.
define ptr @first(ptr %unused) {
  %read = load atomic i32, ptr @x monotonic, align 4
  %frozen = freeze i32 %read
  %zero = xor i32 %frozen, %frozen
  %one = add i32 %zero, 1
  store atomic i32 %one, ptr @y monotonic, align 4
  ret ptr null
}

define ptr @second(ptr %unused) {
  %read = cmpxchg ptr @y, i32 -1, i32 0 monotonic monotonic
  %frozen = freeze { i32, i1 } %read
  %value = extractvalue { i32, i1 } %frozen, 0
  %zero = xor i32 %value, %value
  %one = add i32 %zero, 1
  store atomic i32 %one, ptr @x monotonic, align 4
  ret ptr null
}

define i32 @main() {
  %first = call i32 @__VERIFIER_thread_create(ptr null, ptr @first, ptr null)
  %second = call i32 @__VERIFIER_thread_create(ptr null, ptr @second, ptr null)
  ret i32 0
}
