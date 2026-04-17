// tests/unit/test_patch_map_canonicalize.cpp
// Tests for PatchMap::canonicalize() — the canonical chain-string formatter.
//
// Contract from spec:
//   • Sort numeric-before-alpha
//   • Numeric members sorted ascending (by integer value)
//   • Alphabetic members sorted ascending, uppercased
//   • Joined with '-'

#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "platform/linux_rpi/endpoints/patch_map.h"

// ─── Basic sorting ───────────────────────────────────────────────────────────

TEST(PatchMapCanonical, AlreadySortedNumeric)
{
    EXPECT_EQ(PatchMap::canonicalize({"1", "2", "3"}), "1-2-3");
}

TEST(PatchMapCanonical, ReverseOrderNumeric)
{
    EXPECT_EQ(PatchMap::canonicalize({"3", "1", "2"}), "1-2-3");
}

TEST(PatchMapCanonical, AlphaBecomeUppercase)
{
    EXPECT_EQ(PatchMap::canonicalize({"c", "a", "b"}), "A-B-C");
}

TEST(PatchMapCanonical, NumericBeforeAlpha)
{
    EXPECT_EQ(PatchMap::canonicalize({"A", "1", "B", "2"}), "1-2-A-B");
}

TEST(PatchMapCanonical, MixedLowerAndUpper)
{
    EXPECT_EQ(PatchMap::canonicalize({"c", "3", "a", "1"}), "1-3-A-C");
}

// ─── Spec example from docs ───────────────────────────────────────────────────

TEST(PatchMapCanonical, SpecExample)
{
    // "1-3-A-C" from pins {"c", "3", "a", "1"} or similar after scan
    EXPECT_EQ(PatchMap::canonicalize({"c", "3", "a", "1"}), "1-3-A-C");
}

// ─── Single element ───────────────────────────────────────────────────────────

TEST(PatchMapCanonical, SingleNumeric)
{
    EXPECT_EQ(PatchMap::canonicalize({"5"}), "5");
}

TEST(PatchMapCanonical, SingleAlpha)
{
    EXPECT_EQ(PatchMap::canonicalize({"x"}), "X");
}

// ─── Empty ────────────────────────────────────────────────────────────────────

TEST(PatchMapCanonical, EmptyReturnsEmpty)
{
    EXPECT_EQ(PatchMap::canonicalize({}), "");
}

// ─── Numeric ordering by value (not lexicographic) ───────────────────────────

TEST(PatchMapCanonical, NumericValueSort)
{
    // "9" < "10" by integer value, but "10" < "9" lexicographically
    EXPECT_EQ(PatchMap::canonicalize({"10", "9", "2"}), "2-9-10");
}

// ─── Uppercase input passthrough ─────────────────────────────────────────────

TEST(PatchMapCanonical, UppercaseInputUnchanged)
{
    EXPECT_EQ(PatchMap::canonicalize({"B", "A"}), "A-B");
}

// ─── All-numeric multi-digit ─────────────────────────────────────────────────

TEST(PatchMapCanonical, MultiDigitNumbers)
{
    EXPECT_EQ(PatchMap::canonicalize({"100", "20", "3"}), "3-20-100");
}
