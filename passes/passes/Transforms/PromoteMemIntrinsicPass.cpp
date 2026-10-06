/*
 * GenMC -- Generic Model Checking.
 *
 * This project is dual-licensed under the Apache License 2.0 and the MIT License.
 * You may choose to use, distribute, or modify this software under either license.
 *
 * Apache License 2.0:
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * MIT License:
 *     https://opensource.org/licenses/MIT
 */

#include "PromoteMemIntrinsicPass.hpp"
#include "genmc/Support/Error.hpp"

#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/ADT/Twine.h>
#include <llvm/Analysis/ValueTracking.h>
#include <llvm/Config/llvm-config.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/ConstantRange.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DebugInfo.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/InstrTypes.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/PassManager.h>
#include <llvm/IR/Type.h>
#include <llvm/Support/Alignment.h>
#include <llvm/Support/Casting.h>

#include <algorithm>
#include <bit>
#include <cstdint>
#include <ranges>
#include <utility>
#include <vector>

/* With opaque pointers, no ptr->int->ptr cast is needed */
#define CAST_PTR_TO_TYPE_THROUGH_INT(builder, ptr, dstTy, dataLayout) ptr

#define CONSTEXPR_GET_FIELD_TYPE(MI, Field) MI->get##Field()->getType()

/* Helper macro that moves constant expressions into their own instructions */
#define LOWER_CONSTEXPR(builder, MI, Field)                                                        \
	if (auto *constExpr = dyn_cast<ConstantExpr>(MI->get##Field())) {                          \
		Value *newGEP = builder.Insert(constExpr->getAsInstruction());                     \
		[[maybe_unused]] Type *destType = CONSTEXPR_GET_FIELD_TYPE(MI, Field);             \
		Value *castedGEP = CAST_PTR_TO_TYPE_THROUGH_INT(                                   \
			builder, newGEP, destType,                                                 \
			MI->getParent()->getParent()->getParent()->getDataLayout());               \
		MI->set##Field(castedGEP);                                                         \
	}

using namespace llvm;

static void emitMemoryBoundsFailure(IRBuilder<> &builder, StringRef message);

/* Expand short, bounded byte operations before SROA. Constant byte offsets
 * let SROA turn copies/comparisons of local scalar object representations into
 * shifts and masks, avoiding artificial mixed-size memory accesses. */
static auto lowerBoundedByteOperation(CallInst *call, bool compare) -> bool
{
	auto *length = call->getArgOperand(2);
	auto maximum = computeConstantRange(length, false).getUnsignedMax();
	auto needsBoundsCheck = false;
	if (maximum.ugt(32)) {
		/* A direct, fixed-size alloca also bounds every valid byte access.
		 * Diagnose an oversized request rather than silently truncating it. */
		for (auto index : {0U, 1U}) {
			auto *allocation = dyn_cast<AllocaInst>(
				call->getArgOperand(index)->stripPointerCasts());
			if (!allocation)
				continue;
			auto size =
				allocation->getAllocationSize(call->getModule()->getDataLayout());
			if (size && !size->isScalable() && size->getFixedValue() <= 32) {
				maximum = APInt(maximum.getBitWidth(), size->getFixedValue());
				needsBoundsCheck = true;
				break;
			}
		}
		if (!needsBoundsCheck)
			return false;
	}
	auto count = maximum.getZExtValue();
	auto *function = call->getFunction();
	auto &context = call->getContext();
	auto *before = call->getParent();
	auto *after = before->splitBasicBlock(call->getIterator(), "bytes.done");
	before->getTerminator()->eraseFromParent();
	IRBuilder<> builder(before);
	builder.SetCurrentDebugLocation(call->getDebugLoc());
	if (needsBoundsCheck) {
		auto *valid = BasicBlock::Create(context, "bytes.valid", function, after);
		auto *invalid = BasicBlock::Create(context, "bytes.invalid", function, after);
		builder.CreateCondBr(
			builder.CreateICmpULE(length, ConstantInt::get(context, maximum)), valid,
			invalid);
		builder.SetInsertPoint(invalid);
		emitMemoryBoundsFailure(builder, "byte operation exceeds local object");
		builder.SetInsertPoint(valid);
	}
	PHINode *result = nullptr;
	if (compare)
		result = PHINode::Create(call->getType(), count * 2 + 1, "bytes.result",
					 after->begin());
	auto *byteType = builder.getInt8Ty();
	auto *zero = ConstantInt::get(call->getType(), 0);
	for (uint64_t offset = 0; offset < count; ++offset) {
		auto *access = BasicBlock::Create(context, "bytes.access", function, after);
		auto *next = BasicBlock::Create(context, "bytes.next", function, after);
		auto *index = ConstantInt::get(length->getType(), offset);
		if (result)
			result->addIncoming(zero, builder.GetInsertBlock());
		builder.CreateCondBr(builder.CreateICmpUGT(length, index), access, after);
		builder.SetInsertPoint(access);
		auto *left = builder.CreateGEP(byteType, call->getArgOperand(0), index);
		auto *right = builder.CreateGEP(byteType, call->getArgOperand(1), index);
		auto *rhs = builder.CreateAlignedLoad(byteType, right, Align(1));
		if (compare) {
			auto *lhs = builder.CreateAlignedLoad(byteType, left, Align(1));
			auto *difference =
				builder.CreateSub(builder.CreateZExt(lhs, call->getType()),
						  builder.CreateZExt(rhs, call->getType()));
			result->addIncoming(difference, access);
			builder.CreateCondBr(builder.CreateICmpNE(lhs, rhs), after, next);
		} else {
			auto *copy = cast<MemCpyInst>(call);
			rhs->setVolatile(copy->isVolatile());
			auto *store = builder.CreateAlignedStore(rhs, left, Align(1));
			store->setVolatile(copy->isVolatile());
			builder.CreateBr(next);
		}
		builder.SetInsertPoint(next);
	}
	if (result) {
		result->addIncoming(zero, builder.GetInsertBlock());
		call->replaceAllUsesWith(result);
	}
	builder.CreateBr(after);
	call->eraseFromParent();
	return true;
}

static void emitMemoryBoundsFailure(IRBuilder<> &builder, StringRef message)
{
	auto failure = builder.GetInsertBlock()->getModule()->getOrInsertFunction(
		"__VERIFIER_assert_fail", builder.getVoidTy(), builder.getPtrTy(),
		builder.getPtrTy(), builder.getInt32Ty());
	builder.CreateCall(
		failure,
		{builder.CreateGlobalString(message, "__genmc_byte_bounds"),
		 builder.CreateGlobalString("<memory intrinsic>", "__genmc_byte_file"),
		 builder.getInt32(0)});
	builder.CreateUnreachable();
}

/**
 * Lower a call to: __memcpy_chk(void * dest, const void * src, size_t len, size_t destlen);
 *
 * Reference implementation (see __memcpy_chk source):
 * 	if dstlen < len
 * 		__chk_fail ();
 * 	return memcpy(dest, src, len);
 *
 * __memcpy_chk spec:
 * http://refspecs.linux-foundation.org/LSB_4.0.0/LSB-Core-generic/LSB-Core-generic/libc---memcpy-chk-1.html
 * __memcpy_chk source:
 * https://sourceware.org/git/?p=glibc.git;a=blob;f=debug/memcpy_chk.c;h=6f8628ab945c5e8d2738f7382238c1e1006afe41;hb=HEAD
 * memcpy spec: https://pubs.opengroup.org/onlinepubs/9699919799/functions/memcpy.html
 */
static void lowerFortifiedMemCpy(CallInst *CI, Function &F)
{
	/* Get arguments */
	auto *dst_ptr = CI->getArgOperand(0);
	auto *src_ptr = CI->getArgOperand(1);
	auto *len = CI->getArgOperand(2);
	auto *dst_len = CI->getArgOperand(3);

	/* Split the basic block at the call to __memcpy_chk in bb (before call-instr.) and contBB
	 * (rest of the block) */
	auto *bb = CI->getParent();
	auto *contBB =
		bb->splitBasicBlock(CI); /* Creates a dummy terminator for bb -> remove later */

	/* Insert the llvm.memcpy intrinsic at the start of contBB */
	IRBuilder<> contBuilder(contBB, contBB->begin());
	contBuilder.CreateMemCpy(dst_ptr, Align(1), src_ptr, Align(1), len);
	CI->replaceAllUsesWith(
		dst_ptr); /* memcpy(...) returns the dst_ptr back, see: memcpy Spec */

	/* Create a new basic block "memcpy__chk_fail" for inside the IF => aborts using an
	 * unreachable-instruction */
	auto *failBB = BasicBlock::Create(F.getContext(), "memcpy__chk_fail", &F);
	IRBuilder<> failBuilder(failBB);
	failBuilder.SetCurrentDebugLocation(CI->getDebugLoc());
	emitMemoryBoundsFailure(failBuilder, "memcpy exceeds destination size");

	/* Compare arguments: dstlen < len */
	auto *dummyTerminator = bb->getTerminator();
	IRBuilder<> termBuilder(dummyTerminator);
	auto *cmp = termBuilder.CreateICmpULT(dst_len, len);

	/* Replace the dummy terminator: We jump to failBB if the condition holds */
	dummyTerminator->eraseFromParent();
	termBuilder.SetInsertPoint(bb);
	termBuilder.CreateCondBr(cmp, /* True */ failBB, /* False */ contBB);
}

/**
 * We collect and lower all calls to the fortified version of memcpy: __memcpy_chk
 * The fortified version checks for buffer overflows before performing the memcpy.
 * @see lowerFortifiedMemCpy for implementation details
 */
static auto lowerFortifiedCalls(Function &F) -> bool
{
	auto modified = false;
	SmallVector<CallInst *, 8> fortifiedCalls;

	/* Collect call-instructions to __memcpy_chk */
	for (auto &I : instructions(F)) {
		auto *ci = dyn_cast<CallInst>(&I);
		if (!ci)
			continue;

		auto *calledFun = dyn_cast<Function>(ci->getCalledOperand());
		if (!calledFun || calledFun->getName() != "__memcpy_chk")
			continue;

		fortifiedCalls.push_back(ci);
		modified = true;
	}

	for (auto *ci : fortifiedCalls)
		lowerFortifiedMemCpy(ci, F);
	for (auto *ci : fortifiedCalls)
		ci->eraseFromParent();
	return modified;
}

static auto isPromotableMemIntrinsicOperand(Value *op) -> bool
{
	/* Constant to capture MemSet too */
	return isa<Constant>(op) || isa<AllocaInst>(op) || isa<GetElementPtrInst>(op);
}

static auto getPromotionGEPType(Value *op) -> Type *
{
	VERIFY(isPromotableMemIntrinsicOperand(op));
	if (auto *v = dyn_cast<GlobalVariable>(op))
		return v->getValueType();
	if (auto *ai = dyn_cast<AllocaInst>(op))
		return ai->getAllocatedType();
	if (auto *gepi = dyn_cast<GetElementPtrInst>(op))
		return gepi->getResultElementType();
	UNREACHABLE();
}

static void promoteMemCpyBytes(IRBuilder<> &builder, Value *dst, Value *src, uint64_t begin,
			       uint64_t end, bool isVolatile);
static void promoteMemSetBytes(IRBuilder<> &builder, Value *dst, uint8_t byte, uint64_t begin,
			       uint64_t end, Align align, bool isVolatile);

static void promoteMemCpy(IRBuilder<> &builder, Value *dst, Value *src,
			  const std::vector<Value *> &args, Type *typ, uint64_t length,
			  uint64_t &copiedLen, bool isVolatile)
{
	if (copiedLen == length)
		return;

	const auto &layout = builder.GetInsertBlock()->getModule()->getDataLayout();
	auto offset = static_cast<uint64_t>(
		layout.getIndexedOffsetInType(getPromotionGEPType(dst), args));
	/* Padding counts towards the copied prefix, but its contents are
	 * unspecified: skip it rather than read possibly uninitialized bytes or
	 * re-write the tail of a field accessed with its allocation size. */
	copiedLen = std::min(offset, length);
	if (copiedLen == length)
		return;

	auto len = layout.getTypeStoreSize(typ).getFixedValue();
	/* Keep supported integer widths for partial scalars too. Other widths,
	 * such as i24, can have padded allocation sizes, which the interpreter
	 * would use as access widths. Copy those prefixes bytewise instead. */
	if (len > length - copiedLen) {
		len = length - copiedLen;
		typ = IntegerType::get(builder.getContext(), len * 8);
		if (!std::has_single_bit(len) || len > sizeof(uint64_t) ||
		    layout.getTypeAllocSize(typ) != len) {
			promoteMemCpyBytes(builder, dst, src, copiedLen, length, isVolatile);
			copiedLen = length;
			return;
		}
	}

	auto *srcGEP =
		builder.CreateInBoundsGEP(getPromotionGEPType(src), src, args, "memcpy.src.gep");
	auto *dstGEP =
		builder.CreateInBoundsGEP(getPromotionGEPType(dst), dst, args, "memcpy.dst.gep");

	copiedLen += len;
	auto *srcLoad = builder.CreateAlignedLoad(typ, srcGEP, Align(1), "memcpy.src.load");
	srcLoad->setVolatile(isVolatile);
	builder.CreateAlignedStore(srcLoad, dstGEP, Align(1))->setVolatile(isVolatile);
}

static void promoteMemCpyBytes(IRBuilder<> &builder, Value *dst, Value *src, uint64_t begin,
			       uint64_t end, bool isVolatile)
{
	for (auto offset = begin; offset < end; ++offset) {
		auto *index = builder.getInt64(offset);
		auto *srcGEP = builder.CreateGEP(builder.getInt8Ty(), src, index);
		auto *dstGEP = builder.CreateGEP(builder.getInt8Ty(), dst, index);
		auto *value = builder.CreateAlignedLoad(builder.getInt8Ty(), srcGEP, Align(1));
		value->setVolatile(isVolatile);
		builder.CreateAlignedStore(value, dstGEP, Align(1))->setVolatile(isVolatile);
	}
}

static auto splatByte(unsigned bits, uint8_t byte) -> APInt
{
	return bits < 8 ? APInt(8, byte).trunc(bits) : APInt::getSplat(bits, APInt(8, byte));
}

static void promoteMemSet(IRBuilder<> &builder, Value *dst, uint8_t byte,
			  const std::vector<Value *> &args, Type *typ, uint64_t length, Align align,
			  bool isVolatile)
{
	VERIFY(typ->isIntegerTy() || typ->isPointerTy());

	const auto &DL = builder.GetInsertBlock()->getModule()->getDataLayout();
	auto offset =
		static_cast<uint64_t>(DL.getIndexedOffsetInType(getPromotionGEPType(dst), args));
	if (offset >= length)
		return;

	/* A range ending inside a scalar only sets its prefix */
	if (DL.getTypeStoreSize(typ).getFixedValue() > length - offset) {
		promoteMemSetBytes(builder, dst, byte, offset, length, align, isVolatile);
		return;
	}

	auto sizeInBits = typ->isIntegerTy() ? typ->getIntegerBitWidth()
					     : DL.getPointerTypeSizeInBits(typ);
	Value *val = Constant::getIntegerValue(typ, splatByte(sizeInBits, byte));

	Value *dstGEP =
		builder.CreateInBoundsGEP(getPromotionGEPType(dst), dst, args, "memset.dst.gep");
	builder.CreateStore(val, dstGEP)->setVolatile(isVolatile);
}

/* Sets [BEGIN, END) of DST, which is ALIGN-aligned, with the widest stores
 * (up to 64 bits) that are aligned and fit in the remaining range. Programs
 * typically access such untyped ranges as aligned words, and the interpreter
 * cannot compose a load from narrower writes. */
static void promoteMemSetBytes(IRBuilder<> &builder, Value *dst, uint8_t byte, uint64_t begin,
			       uint64_t end, Align align, bool isVolatile)
{
	for (auto offset = begin; offset < end;) {
		auto width = std::min({uint64_t{sizeof(uint64_t)}, std::bit_floor(end - offset),
				       commonAlignment(align, offset).value()});
		auto *typ = builder.getIntNTy(width * 8);
		auto *dstGEP = builder.CreateGEP(builder.getInt8Ty(), dst, builder.getInt64(offset),
						 "memset.dst.gep");
		builder.CreateAlignedStore(ConstantInt::get(typ, splatByte(width * 8, byte)),
					   dstGEP, Align(width))
			->setVolatile(isVolatile);
		offset += width;
	}
}

template <typename F>
static void promoteMemIntrinsic(Type *typ, std::vector<Value *> &args, F &&promoteFun)
{
	auto *i32Ty = IntegerType::getInt32Ty(typ->getContext());

	if (!isa<StructType>(typ) && !isa<ArrayType>(typ) && !isa<VectorType>(typ)) {
		std::forward<F>(promoteFun)(typ, args);
		return;
	}

	if (auto *arrayType = dyn_cast<ArrayType>(typ)) {
#ifdef LLVM_HAS_GLOBALOBJECT_GET_METADATA
		auto numElems = arrayType->getNumElements();
#else
		auto numElems = arrayType->getArrayNumElements();
#endif
		for (auto i = 0U; i < numElems; i++) {
			args.push_back(Constant::getIntegerValue(i32Ty, APInt(32, i)));
			promoteMemIntrinsic(arrayType->getElementType(), args, promoteFun);
			args.pop_back();
		}
	} else if (auto *structType = dyn_cast<StructType>(typ)) {
		for (auto i = 0U; i < structType->getNumElements(); ++i) {
			args.push_back(Constant::getIntegerValue(i32Ty, APInt(32, i)));
			promoteMemIntrinsic(structType->getElementType(i), args, promoteFun);
			args.pop_back();
		}
	} else {
		UNREACHABLE();
	}
}

static auto canPromoteMemIntrinsic(MemIntrinsic *MI) -> bool
{
	/* Skip if length is not a constant */
	auto *length = dyn_cast<ConstantInt>(MI->getLength());
	if (!length) {
		WARN_ONCE("memintr-length",
			  "Cannot promote non-constant-length mem intrinsic! Skipping...\n");
		return false;
	}

	/*
	 * For memcpy(), make sure source and dest live in the same address space
	 * (This also makes sure we are not copying from genmc's space to userspace)
	 */
	auto *MCI = dyn_cast<MemCpyInst>(MI);
	VERIFY(!MCI || MCI->getSourceAddressSpace() == MCI->getDestAddressSpace());
	if (MCI && !(isPromotableMemIntrinsicOperand(MCI->getDest()) ||
		     isPromotableMemIntrinsicOperand(MCI->getSource()))) {
		WARN_ONCE("memintr-opaque", "Cannot promote memcpy() due to both src and dst being "
					    "opaque! Skipping...\n");
		return false;
	}

	auto *MSI = dyn_cast<MemSetInst>(MI);
	if (MSI && !isPromotableMemIntrinsicOperand(MSI->getDest())) {
		WARN_ONCE("memintr-dst",
			  "Cannot promote memset() due to dst being opaque! Skipping...\n");
		return false;
	}

	/*
	 * Finally, this is one of the cases we can currently handle.
	 * We produce a warning anyway because, e.g., if a small struct has no atomic
	 * fields, clang might initialize it with memcpy(), and then read it with a
	 * 64bit access, and mixed-size accesses are __bad__ news
	 */
	WARN_ONCE("promote-memintrinsic", "Memory intrinsic found! Attempting to promote it...\n");
	return true;
}

/**
 * Due to LLVM's use of opaque pointers, we infer the types based on the instruction defining the
 * value (Alloca, GEP, GlobalVal => @see getPromotionGEPType()). To allow copying between different
 * (compatible) data-types, we perform a cast either src->dst or dst->src.
 *
 * Note: We assume that the compiler only generates memcpy()s between "compatible" types:
 *  - compatible: struct {i32, i32, i32} and [3 x i32]
 *  - non-compatible:  struct {i32, i64, i32} and [4 x i32]
 * Promotion in the second case would lead to "mixed-sized accesses".
 */
static auto getRecastedOperands(MemCpyInst *MI, IRBuilder<> &builder) -> std::pair<Value *, Value *>
{
	auto *dst = MI->getDest();
	auto *src = MI->getSource();

	auto dataLayout = MI->getParent()->getParent()->getParent()->getDataLayout();
	auto indexSize = dataLayout.getIndexSizeInBits(dst->getType()->getPointerAddressSpace());
	const std::vector<Value *> gepArgs = {Constant::getIntegerValue(
		IntegerType::get(builder.getContext(), indexSize),
		APInt(indexSize, 0))}; // Ex: getelementptr %type, %ptr, i32 0

	/* Case src->dst */
	if (isPromotableMemIntrinsicOperand(dst)) {
		src = CAST_PTR_TO_TYPE_THROUGH_INT(builder, src, dst->getType(), dataLayout);
		return {builder.CreateGEP(getPromotionGEPType(dst), src, ArrayRef(gepArgs)), dst};
	}

	/* Case dst->src */
	VERIFY(isPromotableMemIntrinsicOperand(src));
	dst = CAST_PTR_TO_TYPE_THROUGH_INT(builder, dst, src->getType(), dataLayout);
	return {src, builder.CreateGEP(getPromotionGEPType(src), dst, ArrayRef(gepArgs))};
}

/* Tries to promote a memcpy() instruction.
 * We also allow the promotion of memcpy()s w/ constant arguments:
 *
 * call void @llvm.memcpy.p0.p0.i64(
 *      ptr align 8 getelementptr inbounds (%struct.queue_t, ptr @queue, i64 0, i32 0, i32 1),
 *      ptr align 8 %_3,
 *      i64 8,
 *      i1 false)
 *
 * by moving constant expressions into their own instruction and proceeding as normal. */
static auto tryPromoteMemCpy(MemCpyInst *MI, SmallVector<llvm::MemIntrinsic *, 8> &promoted) -> bool
{
	if (!canPromoteMemIntrinsic(MI))
		return false;

	/* We only copy "len" bytes (3rd arg in llvm.memcpy) */
	auto len = cast<ConstantInt>(MI->getLength())->getZExtValue();

	/* Remove memcpy's with len=0 completely as no-ops (generated by rustc) */
	if (len == 0) {
		WARN_ONCE("memintr-zero-length",
			  "Cannot promote zero-length mem intrinsic! Removing instruction...\n");
		promoted.push_back(MI);
		return true;
	}

	IRBuilder<> builder(MI);
	auto *i64Ty = IntegerType::getInt64Ty(MI->getContext());
	auto *nullInt = Constant::getNullValue(i64Ty);

	/* Check for constexpr arguments */
	LOWER_CONSTEXPR(builder, MI, Source); // NOLINT(misc-const-correctness)
	LOWER_CONSTEXPR(builder, MI, Dest);   // NOLINT(misc-const-correctness)

	/* Recast args to same types for GEP indexing later */
	auto [src, dst] = getRecastedOperands(MI, builder);
	auto *dstTyp = getPromotionGEPType(dst);
	VERIFY(dstTyp);

	/* To ensure we only copy "len" bytes from total */
	auto typeSizeDst = MI->getParent()->getModule()->getDataLayout().getTypeStoreSize(dstTyp);
	VERIFY(typeSizeDst >= len);

	uint64_t copiedLen = 0;
	std::vector<Value *> args = {nullInt};
	promoteMemIntrinsic(dstTyp, args, [&](Type *typ, const std::vector<Value *> &args) {
		promoteMemCpy(builder, dst, src, args, typ, len, copiedLen, MI->isVolatile());
	});
	promoted.push_back(MI);
	return true;
}

static auto tryPromoteMemSet(MemSetInst *MS, SmallVector<MemIntrinsic *, 8> &promoted) -> bool
{
	if (!canPromoteMemIntrinsic(MS))
		return false;

	/* We only set "len" bytes (3rd arg in llvm.memset) */
	auto len = cast<ConstantInt>(MS->getLength())->getZExtValue();
	if (len == 0) {
		WARN_ONCE("memintr-zero-length",
			  "Cannot promote zero-length mem intrinsic! Removing instruction...\n");
		promoted.push_back(MS);
		return true;
	}

	auto *dst = MS->getDest();
	VERIFY(isa<ConstantInt>(MS->getValue()));
	auto byte = static_cast<uint8_t>(cast<ConstantInt>(MS->getValue())->getZExtValue());
	auto align = MS->getDestAlign().valueOrOne();

	auto *i64Ty = IntegerType::getInt64Ty(MS->getContext());
	auto *nullInt = Constant::getNullValue(i64Ty);
	auto *dstTyp = getPromotionGEPType(dst);
	VERIFY(dstTyp);

	IRBuilder<> builder(MS);

	/* The inferred type can be narrower than the range, e.g., for a byte
	 * GEP into a larger object, and then says nothing about its layout */
	const auto &DL = MS->getModule()->getDataLayout();
	if (DL.getTypeStoreSize(dstTyp).getFixedValue() < len) {
		promoteMemSetBytes(builder, dst, byte, 0, len, align, MS->isVolatile());
		promoted.push_back(MS);
		return true;
	}

	std::vector<Value *> args = {nullInt};
	promoteMemIntrinsic(dstTyp, args, [&](Type *typ, const std::vector<Value *> &args) {
		promoteMemSet(builder, dst, byte, args, typ, len, align, MS->isVolatile());
	});
	promoted.push_back(MS);
	return true;
}

static void removePromoted(std::ranges::input_range auto &&promoted)
{
	for (auto *MI : promoted) {
		/* Are MI's operands used anywhere else? */
		auto *dst = dyn_cast<BitCastInst>(MI->getRawDest());
		CastInst *src = nullptr;
		if (auto *MC = dyn_cast<MemCpyInst>(MI))
			src = dyn_cast<BitCastInst>(MC->getRawSource());

		MI->eraseFromParent();
		if (dst && dst->hasNUses(0))
			dst->eraseFromParent();
		if (src && src->hasNUses(0))
			src->eraseFromParent();
	}
}

auto PromoteMemIntrinsicPass::run(Function &F, FunctionAnalysisManager & /*FAM*/)
	-> PreservedAnalyses
{
	/* Locate mem intrinsics of interest */
	SmallVector<llvm::MemIntrinsic *, 8> promoted;
	auto modified = false;

	modified |= lowerFortifiedCalls(F);
	SmallVector<CallInst *, 8> byteOperations;
	for (auto &I : instructions(F)) {
		if (auto *copy = dyn_cast<MemCpyInst>(&I)) {
			if (!isa<ConstantInt>(copy->getLength()))
				byteOperations.push_back(copy);
		} else if (auto *call = dyn_cast<CallInst>(&I)) {
			if (auto *callee = call->getCalledFunction();
			    callee && callee->isDeclaration() &&
			    (callee->getName() == "memcmp" || callee->getName() == "bcmp") &&
			    call->arg_size() == 3 && call->getType()->isIntegerTy() &&
			    call->getArgOperand(2)->getType()->isIntegerTy())
				byteOperations.push_back(call);
		}
	}
	for (auto *call : byteOperations)
		modified |= lowerBoundedByteOperation(call, !isa<MemCpyInst>(call));
	for (auto &I : instructions(F)) {
		if (auto *MI = dyn_cast<MemCpyInst>(&I))
			modified |= tryPromoteMemCpy(MI, promoted);
		if (auto *MS = dyn_cast<MemSetInst>(&I))
			modified |= tryPromoteMemSet(MS, promoted);
	}

	/* Erase promoted intrinsics from the code */
	removePromoted(promoted);
	return modified ? PreservedAnalyses::none() : PreservedAnalyses::all();
}
