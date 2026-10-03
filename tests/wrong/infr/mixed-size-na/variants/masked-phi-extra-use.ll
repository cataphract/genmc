target datalayout = "e-p:64:64-i64:64-n8:16:32:64"

@payload = global i32 42
@observed = global i64 0

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

; The incoming load has a second, pure use that escapes the mask through
; another phi. Only the merged pointer is masked, so the load is kept.
define i32 @main() {
  %storage = alloca { ptr, i8 }
  call void @fill(ptr %storage, i32 1, i1 false)
  %flag = getelementptr { ptr, i8 }, ptr %storage, i32 0, i32 1
  %tag = load i8, ptr %flag
  %has_value = trunc i8 %tag to i1
  %route = icmp eq i8 %tag, 0
  br i1 %route, label %speculate, label %known
speculate:
  %pointer = load ptr, ptr %storage
  %bits = ptrtoint ptr %pointer to i64
  br label %merge
known:
  br label %merge
merge:
  %active = phi i1 [ %has_value, %speculate ], [ true, %known ]
  %selected = phi ptr [ %pointer, %speculate ], [ @payload, %known ]
  %unmasked = phi i64 [ %bits, %speculate ], [ 0, %known ]
  %nonnull = icmp ne ptr %selected, null
  %destroy = select i1 %active, i1 %nonnull, i1 false
  br i1 %destroy, label %consume, label %done
consume:
  %value = load i32, ptr %selected
  br label %done
done:
  store i64 %unmasked, ptr @observed
  ret i32 0
}
