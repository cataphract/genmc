target datalayout = "e-p:64:64-i64:64-n8:16:32:64"

@payload = global i32 42
@sink = global i32 0

; A global compared with null also has users in other functions, which the
; dominator tree of this function cannot place. The pass must skip it.
define void @consume(i1 %active) {
  %nonnull = icmp ne ptr @payload, null
  %use = select i1 %active, i1 %nonnull, i1 false
  br i1 %use, label %then, label %done
then:
  store i32 1, ptr @sink
  br label %done
done:
  ret void
}

define i32 @main() {
  %value = load i32, ptr @payload
  %active = icmp eq i32 %value, 42
  call void @consume(i1 %active)
  ret i32 0
}
