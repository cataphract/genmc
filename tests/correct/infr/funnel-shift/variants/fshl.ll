target datalayout = "e-m:e-p:64:64-i64:64-i128:128-n8:16:32:64-S128"

; Each @llvm.fshl result is checked against its exact expected value,
; covering shift amounts of zero, equal to and above the bit width, and
; rotates (both value operands equal). Operands are volatile loads so
; that nothing folds before the intrinsic is lowered.

@message = private constant [5 x i8] c"test\00"
@a0 = global i8 -91
@b0 = global i8 60
@c0 = global i8 3
@a1 = global i8 -91
@b1 = global i8 60
@c1 = global i8 0
@a2 = global i8 -91
@b2 = global i8 60
@c2 = global i8 8
@a3 = global i8 -91
@b3 = global i8 60
@c3 = global i8 11
@a4 = global i8 -91
@b4 = global i8 60
@c4 = global i8 -1
@a5 = global i8 -91
@c5 = global i8 3
@a6 = global i16 -32767
@b6 = global i16 32766
@c6 = global i16 17
@a7 = global i32 -559038737
@b7 = global i32 19088743
@c7 = global i32 12
@a8 = global i32 -559038737
@b8 = global i32 19088743
@c8 = global i32 0
@a9 = global i32 -559038737
@b9 = global i32 19088743
@c9 = global i32 32
@a10 = global i32 -559038737
@b10 = global i32 19088743
@c10 = global i32 45
@a11 = global i32 -559038737
@c11 = global i32 31
@a12 = global i64 81985529216486895
@b12 = global i64 -81985529216486896
@c12 = global i64 13
@a13 = global i64 81985529216486895
@b13 = global i64 -81985529216486896
@c13 = global i64 0
@a14 = global i64 81985529216486895
@b14 = global i64 -81985529216486896
@c14 = global i64 64
@a15 = global i64 81985529216486895
@b15 = global i64 -81985529216486896
@c15 = global i64 77
@a16 = global i64 81985529216486895
@b16 = global i64 -81985529216486896
@c16 = global i64 -1
@a17 = global i64 -9223372036854775807
@c17 = global i64 1
@a18 = global <4 x i32> <i32 -559038737, i32 -2147483648, i32 305419896, i32 -889275714>
@b18 = global <4 x i32> <i32 19088743, i32 1, i32 305419896, i32 252645135>
@c18 = global <4 x i32> <i32 0, i32 5, i32 32, i32 37>
@a19 = global <2 x i8> <i8 -127, i8 -91>
@b19 = global <2 x i8> <i8 -127, i8 60>
@c19 = global <2 x i8> <i8 1, i8 9>

declare <2 x i8> @llvm.fshl.v2i8(<2 x i8>, <2 x i8>, <2 x i8>)
declare <4 x i32> @llvm.fshl.v4i32(<4 x i32>, <4 x i32>, <4 x i32>)
declare i16 @llvm.fshl.i16(i16, i16, i16)
declare i32 @llvm.fshl.i32(i32, i32, i32)
declare i64 @llvm.fshl.i64(i64, i64, i64)
declare i8 @llvm.fshl.i8(i8, i8, i8)
declare void @__VERIFIER_assert_fail(ptr, ptr, i32)

define i32 @main() {
  ; i8, in-range shift
  %a0 = load volatile i8, ptr @a0
  %b0 = load volatile i8, ptr @b0
  %c0 = load volatile i8, ptr @c0
  %r0 = call i8 @llvm.fshl.i8(i8 %a0, i8 %b0, i8 %c0)
  %ok0 = icmp eq i8 %r0, 41
  ; i8, shift by zero
  %a1 = load volatile i8, ptr @a1
  %b1 = load volatile i8, ptr @b1
  %c1 = load volatile i8, ptr @c1
  %r1 = call i8 @llvm.fshl.i8(i8 %a1, i8 %b1, i8 %c1)
  %ok1 = icmp eq i8 %r1, -91
  ; i8, shift by the bit width
  %a2 = load volatile i8, ptr @a2
  %b2 = load volatile i8, ptr @b2
  %c2 = load volatile i8, ptr @c2
  %r2 = call i8 @llvm.fshl.i8(i8 %a2, i8 %b2, i8 %c2)
  %ok2 = icmp eq i8 %r2, -91
  ; i8, shift above the bit width
  %a3 = load volatile i8, ptr @a3
  %b3 = load volatile i8, ptr @b3
  %c3 = load volatile i8, ptr @c3
  %r3 = call i8 @llvm.fshl.i8(i8 %a3, i8 %b3, i8 %c3)
  %ok3 = icmp eq i8 %r3, 41
  ; i8, all-ones shift amount
  %a4 = load volatile i8, ptr @a4
  %b4 = load volatile i8, ptr @b4
  %c4 = load volatile i8, ptr @c4
  %r4 = call i8 @llvm.fshl.i8(i8 %a4, i8 %b4, i8 %c4)
  %ok4 = icmp eq i8 %r4, -98
  ; i8, rotate
  %a5 = load volatile i8, ptr @a5
  %c5 = load volatile i8, ptr @c5
  %r5 = call i8 @llvm.fshl.i8(i8 %a5, i8 %a5, i8 %c5)
  %ok5 = icmp eq i8 %r5, 45
  ; i16, shift above the bit width
  %a6 = load volatile i16, ptr @a6
  %b6 = load volatile i16, ptr @b6
  %c6 = load volatile i16, ptr @c6
  %r6 = call i16 @llvm.fshl.i16(i16 %a6, i16 %b6, i16 %c6)
  %ok6 = icmp eq i16 %r6, 2
  ; i32, in-range shift
  %a7 = load volatile i32, ptr @a7
  %b7 = load volatile i32, ptr @b7
  %c7 = load volatile i32, ptr @c7
  %r7 = call i32 @llvm.fshl.i32(i32 %a7, i32 %b7, i32 %c7)
  %ok7 = icmp eq i32 %r7, -605097966
  ; i32, shift by zero
  %a8 = load volatile i32, ptr @a8
  %b8 = load volatile i32, ptr @b8
  %c8 = load volatile i32, ptr @c8
  %r8 = call i32 @llvm.fshl.i32(i32 %a8, i32 %b8, i32 %c8)
  %ok8 = icmp eq i32 %r8, -559038737
  ; i32, shift by the bit width
  %a9 = load volatile i32, ptr @a9
  %b9 = load volatile i32, ptr @b9
  %c9 = load volatile i32, ptr @c9
  %r9 = call i32 @llvm.fshl.i32(i32 %a9, i32 %b9, i32 %c9)
  %ok9 = icmp eq i32 %r9, -559038737
  ; i32, shift above the bit width
  %a10 = load volatile i32, ptr @a10
  %b10 = load volatile i32, ptr @b10
  %c10 = load volatile i32, ptr @c10
  %r10 = call i32 @llvm.fshl.i32(i32 %a10, i32 %b10, i32 %c10)
  %ok10 = icmp eq i32 %r10, -1210195932
  ; i32, rotate
  %a11 = load volatile i32, ptr @a11
  %c11 = load volatile i32, ptr @c11
  %r11 = call i32 @llvm.fshl.i32(i32 %a11, i32 %a11, i32 %c11)
  %ok11 = icmp eq i32 %r11, -279519369
  ; i64, in-range shift
  %a12 = load volatile i64, ptr @a12
  %b12 = load volatile i64, ptr @b12
  %c12 = load volatile i64, ptr @c12
  %r12 = call i64 @llvm.fshl.i64(i64 %a12, i64 %b12, i64 %c12)
  %ok12 = icmp eq i64 %r12, 7542668687916793819
  ; i64, shift by zero
  %a13 = load volatile i64, ptr @a13
  %b13 = load volatile i64, ptr @b13
  %c13 = load volatile i64, ptr @c13
  %r13 = call i64 @llvm.fshl.i64(i64 %a13, i64 %b13, i64 %c13)
  %ok13 = icmp eq i64 %r13, 81985529216486895
  ; i64, shift by the bit width
  %a14 = load volatile i64, ptr @a14
  %b14 = load volatile i64, ptr @b14
  %c14 = load volatile i64, ptr @c14
  %r14 = call i64 @llvm.fshl.i64(i64 %a14, i64 %b14, i64 %c14)
  %ok14 = icmp eq i64 %r14, 81985529216486895
  ; i64, shift above the bit width
  %a15 = load volatile i64, ptr @a15
  %b15 = load volatile i64, ptr @b15
  %c15 = load volatile i64, ptr @c15
  %r15 = call i64 @llvm.fshl.i64(i64 %a15, i64 %b15, i64 %c15)
  %ok15 = icmp eq i64 %r15, 7542668687916793819
  ; i64, all-ones shift amount
  %a16 = load volatile i64, ptr @a16
  %b16 = load volatile i64, ptr @b16
  %c16 = load volatile i64, ptr @c16
  %r16 = call i64 @llvm.fshl.i64(i64 %a16, i64 %b16, i64 %c16)
  %ok16 = icmp eq i64 %r16, -40992764608243448
  ; i64, rotate
  %a17 = load volatile i64, ptr @a17
  %c17 = load volatile i64, ptr @c17
  %r17 = call i64 @llvm.fshl.i64(i64 %a17, i64 %a17, i64 %c17)
  %ok17 = icmp eq i64 %r17, 3
  ; <4 x i32>, per-lane shifts of zero, in range, the bit width and above
  %a18 = load volatile <4 x i32>, ptr @a18
  %b18 = load volatile <4 x i32>, ptr @b18
  %c18 = load volatile <4 x i32>, ptr @c18
  %r18 = call <4 x i32> @llvm.fshl.v4i32(<4 x i32> %a18, <4 x i32> %b18, <4 x i32> %c18)
  %e18_0 = extractelement <4 x i32> %r18, i32 0
  %ok18_0 = icmp eq i32 %e18_0, -559038737
  %e18_1 = extractelement <4 x i32> %r18, i32 1
  %ok18_1 = icmp eq i32 %e18_1, 0
  %e18_2 = extractelement <4 x i32> %r18, i32 2
  %ok18_2 = icmp eq i32 %e18_2, 305419896
  %e18_3 = extractelement <4 x i32> %r18, i32 3
  %ok18_3 = icmp eq i32 %e18_3, 1607948225
  ; <2 x i8>, rotate and shift above the bit width
  %a19 = load volatile <2 x i8>, ptr @a19
  %b19 = load volatile <2 x i8>, ptr @b19
  %c19 = load volatile <2 x i8>, ptr @c19
  %r19 = call <2 x i8> @llvm.fshl.v2i8(<2 x i8> %a19, <2 x i8> %b19, <2 x i8> %c19)
  %e19_0 = extractelement <2 x i8> %r19, i32 0
  %ok19_0 = icmp eq i8 %e19_0, 3
  %e19_1 = extractelement <2 x i8> %r19, i32 1
  %ok19_1 = icmp eq i8 %e19_1, 74
  %all0 = and i1 %ok0, %ok1
  %all1 = and i1 %all0, %ok2
  %all2 = and i1 %all1, %ok3
  %all3 = and i1 %all2, %ok4
  %all4 = and i1 %all3, %ok5
  %all5 = and i1 %all4, %ok6
  %all6 = and i1 %all5, %ok7
  %all7 = and i1 %all6, %ok8
  %all8 = and i1 %all7, %ok9
  %all9 = and i1 %all8, %ok10
  %all10 = and i1 %all9, %ok11
  %all11 = and i1 %all10, %ok12
  %all12 = and i1 %all11, %ok13
  %all13 = and i1 %all12, %ok14
  %all14 = and i1 %all13, %ok15
  %all15 = and i1 %all14, %ok16
  %all16 = and i1 %all15, %ok17
  %all17 = and i1 %all16, %ok18_0
  %all18 = and i1 %all17, %ok18_1
  %all19 = and i1 %all18, %ok18_2
  %all20 = and i1 %all19, %ok18_3
  %all21 = and i1 %all20, %ok19_0
  %all22 = and i1 %all21, %ok19_1
  br i1 %all22, label %done, label %failed

failed:
  call void @__VERIFIER_assert_fail(ptr @message, ptr @message, i32 0)
  unreachable

done:
  ret i32 0
}
