target datalayout = "e-m:e-p:64:64-i64:64-i128:128-n8:16:32:64-S128"

; Set ranges through byte GEPs into a larger object. The GEP type says
; nothing about the layout, so the whole range is set with the widest
; aligned stores, and reading it back as words is neither uninitialized nor
; mixed-size.
@message = private constant [5 x i8] c"test\00"

declare void @llvm.memset.p0.i64(ptr, i8, i64, i1)
declare ptr @__VERIFIER_malloc_aligned(i64, i64)
declare void @__VERIFIER_assert_fail(ptr, ptr, i32)

define i32 @main() {
  %storage = call ptr @__VERIFIER_malloc_aligned(i64 64, i64 256)
  %words = getelementptr inbounds i8, ptr %storage, i64 64
  call void @llvm.memset.p0.i64(ptr align 64 %words, i8 0, i64 144, i1 false)
  %first = load i64, ptr %words, align 8
  %lastPtr = getelementptr inbounds i8, ptr %storage, i64 200
  %last = load atomic i64, ptr %lastPtr monotonic, align 8
  %words0 = or i64 %first, %last
  %wordsCorrect = icmp eq i64 %words0, 0

  ; Narrower stores for the tail
  %ones = getelementptr inbounds i8, ptr %storage, i64 232
  call void @llvm.memset.p0.i64(ptr align 8 %ones, i8 1, i64 13, i1 false)
  %head = load i64, ptr %ones, align 8
  %headCorrect = icmp eq i64 %head, 72340172838076673
  %tailPtr = getelementptr inbounds i8, ptr %storage, i64 240
  %tail = load i32, ptr %tailPtr, align 4
  %tailCorrect0 = icmp eq i32 %tail, 16843009
  %lastBytePtr = getelementptr inbounds i8, ptr %storage, i64 244
  %lastByte = load i8, ptr %lastBytePtr, align 1
  %lastByteCorrect = icmp eq i8 %lastByte, 1
  %tailCorrect = and i1 %tailCorrect0, %lastByteCorrect

  %correct0 = and i1 %wordsCorrect, %headCorrect
  %correct = and i1 %correct0, %tailCorrect
  br i1 %correct, label %done, label %failed

failed:
  call void @__VERIFIER_assert_fail(ptr @message, ptr @message, i32 0)
  unreachable

done:
  ret i32 0
}
