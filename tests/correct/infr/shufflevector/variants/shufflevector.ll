target datalayout = "e-m:e-p:64:64-i64:64-i128:128-n8:16:32:64-S128"

; shufflevector with several element types, masks that pick from both
; operands, a result wider than the operands, a splat against a poison
; operand, and a mask with poison lanes (whose values are not checked).
; Operands come from volatile loads so that nothing folds before
; interpretation.

@message = private constant [5 x i8] c"test\00"
@x = global i32 0
@y = global i32 0
@a4 = global <4 x i32> <i32 10, i32 11, i32 12, i32 13>
@b4 = global <4 x i32> <i32 20, i32 21, i32 22, i32 23>
@a2 = global <2 x i32> <i32 1, i32 2>
@b2 = global <2 x i32> <i32 3, i32 4>
@q2 = global <2 x i64> <i64 81985529216486895, i64 -9223372036854775807>
@p2 = global <2 x ptr> <ptr @x, ptr @y>
@f4 = global <4 x float> <float 1.5, float -2.25, float 0.0, float 1.0e10>
@d2 = global <2 x double> <double 3.5, double -0.125>

declare void @__VERIFIER_assert_fail(ptr, ptr, i32)

define i32 @main() {
  ; <4 x i32> picking from both operands
  %a4 = load volatile <4 x i32>, ptr @a4
  %b4 = load volatile <4 x i32>, ptr @b4
  %s4 = shufflevector <4 x i32> %a4, <4 x i32> %b4, <4 x i32> <i32 7, i32 0, i32 5, i32 2>
  %s4_0 = extractelement <4 x i32> %s4, i32 0
  %ok0 = icmp eq i32 %s4_0, 23
  %s4_1 = extractelement <4 x i32> %s4, i32 1
  %ok1 = icmp eq i32 %s4_1, 10
  %s4_2 = extractelement <4 x i32> %s4, i32 2
  %ok2 = icmp eq i32 %s4_2, 21
  %s4_3 = extractelement <4 x i32> %s4, i32 3
  %ok3 = icmp eq i32 %s4_3, 12
  ; <2 x i32> operands widened into a <4 x i32> result
  %a2 = load volatile <2 x i32>, ptr @a2
  %b2 = load volatile <2 x i32>, ptr @b2
  %w4 = shufflevector <2 x i32> %a2, <2 x i32> %b2, <4 x i32> <i32 1, i32 3, i32 0, i32 2>
  %w4_0 = extractelement <4 x i32> %w4, i32 0
  %ok4 = icmp eq i32 %w4_0, 2
  %w4_1 = extractelement <4 x i32> %w4, i32 1
  %ok5 = icmp eq i32 %w4_1, 4
  %w4_2 = extractelement <4 x i32> %w4, i32 2
  %ok6 = icmp eq i32 %w4_2, 1
  %w4_3 = extractelement <4 x i32> %w4, i32 3
  %ok7 = icmp eq i32 %w4_3, 3
  ; <2 x i64> splat of lane 1 against a poison operand
  %q2 = load volatile <2 x i64>, ptr @q2
  %sq = shufflevector <2 x i64> %q2, <2 x i64> poison, <2 x i32> <i32 1, i32 1>
  %sq_0 = extractelement <2 x i64> %sq, i32 0
  %ok8 = icmp eq i64 %sq_0, -9223372036854775807
  %sq_1 = extractelement <2 x i64> %sq, i32 1
  %ok9 = icmp eq i64 %sq_1, -9223372036854775807
  ; <2 x ptr> swapped
  %p2 = load volatile <2 x ptr>, ptr @p2
  %sp = shufflevector <2 x ptr> %p2, <2 x ptr> poison, <2 x i32> <i32 1, i32 0>
  %sp_0 = extractelement <2 x ptr> %sp, i32 0
  %ok10 = icmp eq ptr %sp_0, @y
  %sp_1 = extractelement <2 x ptr> %sp, i32 1
  %ok11 = icmp eq ptr %sp_1, @x
  ; <4 x float> reversed
  %f4 = load volatile <4 x float>, ptr @f4
  %sf = shufflevector <4 x float> %f4, <4 x float> poison, <4 x i32> <i32 3, i32 2, i32 1, i32 0>
  %sf_0 = extractelement <4 x float> %sf, i32 0
  %ok12 = fcmp oeq float %sf_0, 1.0e10
  %sf_1 = extractelement <4 x float> %sf, i32 1
  %ok13 = fcmp oeq float %sf_1, 0.0
  %sf_2 = extractelement <4 x float> %sf, i32 2
  %ok14 = fcmp oeq float %sf_2, -2.25
  %sf_3 = extractelement <4 x float> %sf, i32 3
  %ok15 = fcmp oeq float %sf_3, 1.5
  ; <2 x double> with a poison mask lane; only lane 1 is defined
  %d2 = load volatile <2 x double>, ptr @d2
  %sd = shufflevector <2 x double> %d2, <2 x double> poison, <2 x i32> <i32 poison, i32 0>
  %sd_1 = extractelement <2 x double> %sd, i32 1
  %ok16 = fcmp oeq double %sd_1, 3.5

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
  br i1 %all15, label %done, label %failed

failed:
  call void @__VERIFIER_assert_fail(ptr @message, ptr @message, i32 0)
  unreachable

done:
  ret i32 0
}
