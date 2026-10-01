#include <gtest/gtest.h>

#include "genmc/Execution/ExecutionState.hpp"

enum class AccessKind { NARead, NAWrite, AtomicRead, AtomicWrite };

class AccessConsistencyTest : public testing::TestWithParam<AccessKind> {
protected:
	void record(const AAccess &access)
	{
		auto pos = Event(0, ++index_);
		View view;
		view.updateIdx(pos);
		switch (GetParam()) {
		case AccessKind::NARead:
			state.onNALoad(pos, access, view);
			break;
		case AccessKind::NAWrite:
			state.onNAStore(pos, access, view, SVal(0));
			break;
		case AccessKind::AtomicRead:
			state.onATLoad(pos, access, view);
			break;
		case AccessKind::AtomicWrite:
			state.onATStore(pos, access, view, false, false);
			break;
		}
	}

	ExecutionState state;
	SAddr base{0x100};

private:
	int index_ = 0;
};

TEST_P(AccessConsistencyTest, AtomicConsistencyIgnoresNonAtomicHistory)
{
	record({base, ASize(4)});
	const bool isNonAtomic = GetParam() == AccessKind::NARead ||
				 GetParam() == AccessKind::NAWrite;
	EXPECT_TRUE(state.isAtomicAccessConsistent({base, ASize(4)}));
	EXPECT_EQ(state.isAtomicAccessConsistent({base + 1, ASize(1)}), isNonAtomic);
	EXPECT_EQ(state.isAtomicAccessConsistent({base, ASize(8)}), isNonAtomic);
}

TEST_P(AccessConsistencyTest, NonAtomicLoadsDependOnWritesOnly)
{
	record({base, ASize(4)});
	const bool isRead = GetParam() == AccessKind::NARead ||
			    GetParam() == AccessKind::AtomicRead;
	const bool canReconstruct = !EMIT_NA_LABELS || isRead;
	EXPECT_TRUE(state.isNALoadConsistent({base, ASize(4)}));
	EXPECT_EQ(state.isNALoadConsistent({base, ASize(1)}), canReconstruct);
	EXPECT_EQ(state.isNALoadConsistent({base + 1, ASize(1)}), canReconstruct);
	EXPECT_EQ(state.isNALoadConsistent({base, ASize(8)}), canReconstruct);
	EXPECT_TRUE(state.isNALoadConsistent({base + 4, ASize(1)}));
	EXPECT_TRUE(state.isNALoadConsistent({base - ASize(1), ASize(1)}));
}

TEST_P(AccessConsistencyTest, SeparateWritesCannotSupplyOneWideLoad)
{
	record({base, ASize(1)});
	record({base + 1, ASize(1)});
	const bool isRead = GetParam() == AccessKind::NARead ||
			    GetParam() == AccessKind::AtomicRead;
	EXPECT_EQ(state.isNALoadConsistent({base, ASize(2)}), !EMIT_NA_LABELS || isRead);
	EXPECT_TRUE(state.isNALoadConsistent({base, ASize(1)}));
	EXPECT_TRUE(state.isNALoadConsistent({base + 1, ASize(1)}));
	state.clear();
	EXPECT_TRUE(state.isNALoadConsistent({base, ASize(8)}));
}

TEST(NonAtomicLoadConsistencyTest, PartialOverwritesRetainOriginalFootprints)
{
	ExecutionState state;
	SAddr base{0x100};
	state.onNAStore(Event(0, 1), {base, ASize(8)}, View{}, SVal(0));
	state.onNAStore(Event(0, 2), {base + 4, ASize(4)}, View{}, SVal(0));
	EXPECT_EQ(state.isNALoadConsistent({base, ASize(4)}), !EMIT_NA_LABELS);
	EXPECT_EQ(state.isNALoadConsistent({base, ASize(8)}), !EMIT_NA_LABELS);
	EXPECT_TRUE(state.isNALoadConsistent({base + 4, ASize(4)}));
	state.onNAStore(Event(0, 3), {base, ASize(8)}, View{}, SVal(0));
	EXPECT_TRUE(state.isNALoadConsistent({base, ASize(8)}));
}

TEST(NonAtomicLoadConsistencyTest, AtomicAndNonAtomicWritesReplaceEachOther)
{
	ExecutionState state;
	SAddr base{0x100};
	state.onNAStore(Event(0, 1), {base, ASize(1)}, View{}, SVal(0));
	state.onNAStore(Event(0, 2), {base + 1, ASize(1)}, View{}, SVal(0));
	EXPECT_TRUE(state.isAtomicAccessConsistent({base, ASize(8)}));
	state.onATStore(Event(0, 3), {base, ASize(8)}, View{}, false, false);
	EXPECT_TRUE(state.isNALoadConsistent({base, ASize(8)}));
	state.onNAStore(Event(0, 4), {base + 1, ASize(1)}, View{}, SVal(0));
	EXPECT_EQ(state.isNALoadConsistent({base, ASize(8)}), !EMIT_NA_LABELS);
	EXPECT_TRUE(state.isNALoadConsistent({base + 1, ASize(1)}));
	state.onATStore(Event(0, 5), {base, ASize(8)}, View{}, false, false);
	EXPECT_TRUE(state.isNALoadConsistent({base, ASize(8)}));
	state.onNAStore(Event(0, 6), {base, ASize(8)}, View{}, SVal(0));
	EXPECT_TRUE(state.isNALoadConsistent({base, ASize(8)}));
}

TEST(NonAtomicLoadConsistencyTest, ExternalFrontendByteReconstructionIsUnchanged)
{
	ExecutionState state;
	SAddr base{0x100};
	for (unsigned i = 0; i < 8; ++i)
		state.onNAStore(Event(0, i + 1), {base + i, ASize(1)}, View{}, SVal(i + 1));
	EXPECT_EQ(state.getNAWriteValue({base, ASize(8)}).get(), UINT64_C(0x0807060504030201));
}

INSTANTIATE_TEST_SUITE_P(AllAccessKinds, AccessConsistencyTest,
			 testing::Values(AccessKind::NARead, AccessKind::NAWrite,
					 AccessKind::AtomicRead, AccessKind::AtomicWrite));
