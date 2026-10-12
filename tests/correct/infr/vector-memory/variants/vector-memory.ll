target datalayout = "e-m:e-p:64:64-i64:64-i128:128-n8:16:32:64-S128"

; Whole-vector loads and stores of several element types and widths.
; Each lane is checked against its exact expected value, both when a
; vector is loaded from a static initializer, when it is read back after
; a vector store, when a vector store is observed through scalar loads,
; and when a vector load observes scalar stores. Most accesses are
; volatile so that nothing folds before interpretation.

@message = private constant [5 x i8] c"test\00"
@x = global i32 0
@y = global i32 0
@i4 = global <4 x i32> <i32 1, i32 -2, i32 305419896, i32 2147483647>
@i2b = global <2 x i8> <i8 -91, i8 60>
@i2q = global <2 x i64> <i64 81985529216486895, i64 -9223372036854775807>
@p2 = global <2 x ptr> <ptr @x, ptr @y>
@f4 = global <4 x float> <float 1.5, float -2.25, float 0.0, float 1.0e10>
@lane = global i32 7
@s4 = global <4 x i32> zeroinitializer
@s2b = global <2 x i8> zeroinitializer
@s2q = global <2 x i64> zeroinitializer
@s2p = global <2 x ptr> zeroinitializer
@s4f = global <4 x float> zeroinitializer
@t2q = global <2 x i64> zeroinitializer
@c2p = global <2 x ptr> <ptr @x, ptr @y>
@z2p = global <2 x ptr> <ptr @x, ptr @y>
@arr = global [2 x i32] zeroinitializer

declare void @__VERIFIER_assert_fail(ptr, ptr, i32)

define i32 @main() {
  ; <4 x i32> from a static initializer
  %i4 = load volatile <4 x i32>, ptr @i4
  %i4_0 = extractelement <4 x i32> %i4, i32 0
  %ok0 = icmp eq i32 %i4_0, 1
  %i4_1 = extractelement <4 x i32> %i4, i32 1
  %ok1 = icmp eq i32 %i4_1, -2
  %i4_2 = extractelement <4 x i32> %i4, i32 2
  %ok2 = icmp eq i32 %i4_2, 305419896
  %i4_3 = extractelement <4 x i32> %i4, i32 3
  %ok3 = icmp eq i32 %i4_3, 2147483647
  ; <2 x i8> from a static initializer
  %i2b = load volatile <2 x i8>, ptr @i2b
  %i2b_0 = extractelement <2 x i8> %i2b, i32 0
  %ok4 = icmp eq i8 %i2b_0, -91
  %i2b_1 = extractelement <2 x i8> %i2b, i32 1
  %ok5 = icmp eq i8 %i2b_1, 60
  ; <2 x i64> from a static initializer (non-volatile)
  %i2q = load <2 x i64>, ptr @i2q
  %i2q_0 = extractelement <2 x i64> %i2q, i32 0
  %ok6 = icmp eq i64 %i2q_0, 81985529216486895
  %i2q_1 = extractelement <2 x i64> %i2q, i32 1
  %ok7 = icmp eq i64 %i2q_1, -9223372036854775807
  ; <2 x ptr> from a static initializer
  %p2 = load volatile <2 x ptr>, ptr @p2
  %p2_0 = extractelement <2 x ptr> %p2, i32 0
  %ok8 = icmp eq ptr %p2_0, @x
  %p2_1 = extractelement <2 x ptr> %p2, i32 1
  %ok9 = icmp eq ptr %p2_1, @y
  ; <4 x float> from a static initializer
  %f4 = load volatile <4 x float>, ptr @f4
  %f4_0 = extractelement <4 x float> %f4, i32 0
  %ok10 = fcmp oeq float %f4_0, 1.5
  %f4_1 = extractelement <4 x float> %f4, i32 1
  %ok11 = fcmp oeq float %f4_1, -2.25
  %f4_2 = extractelement <4 x float> %f4, i32 2
  %ok12 = fcmp oeq float %f4_2, 0.0
  %f4_3 = extractelement <4 x float> %f4, i32 3
  %ok13 = fcmp oeq float %f4_3, 1.0e10

  ; <4 x i32> store, observed through scalar loads and a vector load
  %lane = load volatile i32, ptr @lane
  %v4 = insertelement <4 x i32> <i32 -1, i32 0, i32 -559038737, i32 0>, i32 %lane, i32 1
  store volatile <4 x i32> %v4, ptr @s4
  %s4_0 = load volatile i32, ptr @s4
  %ok14 = icmp eq i32 %s4_0, -1
  %s4_p1 = getelementptr i32, ptr @s4, i64 1
  %s4_1 = load volatile i32, ptr %s4_p1
  %ok15 = icmp eq i32 %s4_1, 7
  %s4_p2 = getelementptr i32, ptr @s4, i64 2
  %s4_2 = load volatile i32, ptr %s4_p2
  %ok16 = icmp eq i32 %s4_2, -559038737
  %s4_p3 = getelementptr i32, ptr @s4, i64 3
  %s4_3 = load volatile i32, ptr %s4_p3
  %ok17 = icmp eq i32 %s4_3, 0
  %r4 = load volatile <4 x i32>, ptr @s4
  %r4_1 = extractelement <4 x i32> %r4, i32 1
  %ok18 = icmp eq i32 %r4_1, 7
  %r4_2 = extractelement <4 x i32> %r4, i32 2
  %ok19 = icmp eq i32 %r4_2, -559038737
  ; <2 x i8> store and reload
  %lane8 = trunc i32 %lane to i8
  %v2b = insertelement <2 x i8> <i8 -128, i8 0>, i8 %lane8, i32 1
  store volatile <2 x i8> %v2b, ptr @s2b
  %r2b = load volatile <2 x i8>, ptr @s2b
  %r2b_0 = extractelement <2 x i8> %r2b, i32 0
  %ok20 = icmp eq i8 %r2b_0, -128
  %r2b_1 = extractelement <2 x i8> %r2b, i32 1
  %ok21 = icmp eq i8 %r2b_1, 7
  ; <2 x i64> store and reload
  %lane64 = sext i32 %lane to i64
  %v2q = insertelement <2 x i64> <i64 0, i64 -81985529216486896>, i64 %lane64, i32 0
  store volatile <2 x i64> %v2q, ptr @s2q
  %r2q = load volatile <2 x i64>, ptr @s2q
  %r2q_0 = extractelement <2 x i64> %r2q, i32 0
  %ok22 = icmp eq i64 %r2q_0, 7
  %r2q_1 = extractelement <2 x i64> %r2q, i32 1
  %ok23 = icmp eq i64 %r2q_1, -81985529216486896
  ; <2 x ptr> store (lanes swapped) and reload
  %v2p0 = insertelement <2 x ptr> poison, ptr %p2_1, i32 0
  %v2p = insertelement <2 x ptr> %v2p0, ptr %p2_0, i32 1
  store volatile <2 x ptr> %v2p, ptr @s2p
  %r2p = load volatile <2 x ptr>, ptr @s2p
  %r2p_0 = extractelement <2 x ptr> %r2p, i32 0
  %ok24 = icmp eq ptr %r2p_0, @y
  %r2p_1 = extractelement <2 x ptr> %r2p, i32 1
  %ok25 = icmp eq ptr %r2p_1, @x
  ; <4 x float> store (lanes reversed) and reload
  %v4f0 = insertelement <4 x float> poison, float %f4_3, i32 0
  %v4f1 = insertelement <4 x float> %v4f0, float %f4_2, i32 1
  %v4f2 = insertelement <4 x float> %v4f1, float %f4_1, i32 2
  %v4f = insertelement <4 x float> %v4f2, float %f4_0, i32 3
  store volatile <4 x float> %v4f, ptr @s4f
  %r4f = load volatile <4 x float>, ptr @s4f
  %r4f_0 = extractelement <4 x float> %r4f, i32 0
  %ok26 = fcmp oeq float %r4f_0, 1.0e10
  %r4f_2 = extractelement <4 x float> %r4f, i32 2
  %ok27 = fcmp oeq float %r4f_2, -2.25
  %r4f_3 = extractelement <4 x float> %r4f, i32 3
  %ok28 = fcmp oeq float %r4f_3, 1.5
  ; scalar stores observed through a <2 x i64> load
  store volatile i64 -6, ptr @t2q
  %t2q_p1 = getelementptr i64, ptr @t2q, i64 1
  store volatile i64 %lane64, ptr %t2q_p1
  %r2t = load volatile <2 x i64>, ptr @t2q
  %r2t_0 = extractelement <2 x i64> %r2t, i32 0
  %ok29 = icmp eq i64 %r2t_0, -6
  %r2t_1 = extractelement <2 x i64> %r2t, i32 1
  %ok30 = icmp eq i64 %r2t_1, 7
  ; constant <2 x ptr> stores (globals, a constant GEP, and zeroinitializer),
  ; observed through a vector load and a scalar load
  store volatile <2 x ptr> <ptr @y, ptr getelementptr (i32, ptr @arr, i64 1)>, ptr @c2p
  %rc2p = load volatile <2 x ptr>, ptr @c2p
  %rc2p_0 = extractelement <2 x ptr> %rc2p, i32 0
  %ok31 = icmp eq ptr %rc2p_0, @y
  %c2p_p1 = getelementptr ptr, ptr @c2p, i64 1
  %rc2p_1 = load volatile ptr, ptr %c2p_p1
  %arr1 = getelementptr i32, ptr @arr, i64 1
  %ok32 = icmp eq ptr %rc2p_1, %arr1
  store volatile <2 x ptr> zeroinitializer, ptr @z2p
  %rz2p = load volatile <2 x ptr>, ptr @z2p
  %rz2p_1 = extractelement <2 x ptr> %rz2p, i32 1
  %ok33 = icmp eq ptr %rz2p_1, null

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
  %all17 = and i1 %all16, %ok18
  %all18 = and i1 %all17, %ok19
  %all19 = and i1 %all18, %ok20
  %all20 = and i1 %all19, %ok21
  %all21 = and i1 %all20, %ok22
  %all22 = and i1 %all21, %ok23
  %all23 = and i1 %all22, %ok24
  %all24 = and i1 %all23, %ok25
  %all25 = and i1 %all24, %ok26
  %all26 = and i1 %all25, %ok27
  %all27 = and i1 %all26, %ok28
  %all28 = and i1 %all27, %ok29
  %all29 = and i1 %all28, %ok30
  %all30 = and i1 %all29, %ok31
  %all31 = and i1 %all30, %ok32
  %all32 = and i1 %all31, %ok33
  br i1 %all32, label %done, label %failed

failed:
  call void @__VERIFIER_assert_fail(ptr @message, ptr @message, i32 0)
  unreachable

done:
  ret i32 0
}
