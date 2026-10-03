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

; The store of y follows the merge of the speculated load. Guarding the load
; in its predecessor would put the store under a branch on x, forbidding the
; fourth (load-buffering) execution that IMM allows.
define ptr @first(ptr %unused) {
  %storage = alloca ptr
  call void @fill(ptr %storage, i32 1)
  %read = load atomic i32, ptr @x monotonic, align 4
  %active = icmp ne i32 %read, 0
  %route = icmp eq ptr %unused, null
  br i1 %route, label %speculate, label %known
speculate:
  %pointer = load ptr, ptr %storage
  br label %merge
known:
  br label %merge
merge:
  %guard = phi i1 [ %active, %speculate ], [ true, %known ]
  %selected = phi ptr [ %pointer, %speculate ], [ @payload, %known ]
  store atomic i32 1, ptr @y monotonic, align 4
  %nonnull = icmp ne ptr %selected, null
  %use = select i1 %guard, i1 %nonnull, i1 false
  br i1 %use, label %consume, label %done
consume:
  %value = load i32, ptr %selected
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
