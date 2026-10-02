@value = global i64 0, align 8
@message = private constant [5 x i8] c"test\00"

declare void @__VERIFIER_assert_fail(ptr, ptr, i32)

define i32 @main() {
entry:
  %cas = cmpxchg ptr @value, i64 0, i64 1 seq_cst seq_cst
  %frozen = freeze { i64, i1 } %cas
  %old = extractvalue { i64, i1 } %frozen, 0
  %success = extractvalue { i64, i1 } %frozen, 1
  %old-is-zero = icmp eq i64 %old, 0
  %valid = and i1 %old-is-zero, %success
  br i1 %valid, label %done, label %failed

failed:
  call void @__VERIFIER_assert_fail(ptr @message, ptr @message, i32 0)
  unreachable

done:
  ret i32 0
}
