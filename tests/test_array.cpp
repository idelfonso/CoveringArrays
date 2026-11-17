#include <covarray/array.hpp>

#include <gtest/gtest.h>

namespace {

TEST(Array, DefaultConstructedIsEmpty) {
  const covarray::array a;
  EXPECT_EQ(a.rows(), 0u);
  EXPECT_EQ(a.columns(), 0u);
  EXPECT_EQ(a.order(), 0u);
  EXPECT_EQ(a.strength(), 0u);
  EXPECT_TRUE(a.empty());
}

TEST(Array, ConstructorStoresDimensionsAndZeroInitializes) {
  const covarray::array a(3886, 12, 6, 4);
  EXPECT_EQ(a.rows(), 3886u);
  EXPECT_EQ(a.columns(), 12u);
  EXPECT_EQ(a.order(), 6u);
  EXPECT_EQ(a.strength(), 4u);
  EXPECT_FALSE(a.empty());
  EXPECT_EQ(a.wildcard(), 6);
  for (std::size_t r = 0; r < a.rows(); ++r) {
    for (std::size_t c = 0; c < a.columns(); ++c) {
      EXPECT_EQ(a(r, c), 0);
    }
  }
}

TEST(Array, ElementAccessReadsAndWrites) {
  covarray::array a(2, 3, 4, 2);
  a(0, 0) = 3;
  a(1, 2) = 1;
  EXPECT_EQ(a(0, 0), 3);
  EXPECT_EQ(a(1, 2), 1);
  EXPECT_EQ(a(0, 1), 0);
}

TEST(Array, RowReturnsAContiguousSpan) {
  covarray::array a(2, 3, 4, 2);
  a(1, 0) = 1;
  a(1, 1) = 2;
  a(1, 2) = 3;
  const auto row1 = a.row(1);
  ASSERT_EQ(row1.size(), 3u);
  EXPECT_EQ(row1[0], 1);
  EXPECT_EQ(row1[1], 2);
  EXPECT_EQ(row1[2], 3);
}

TEST(Array, AtThrowsOutOfRange) {
  covarray::array a(2, 3, 4, 2);
  EXPECT_THROW((void)a.at(2, 0), std::out_of_range);
  EXPECT_THROW((void)a.at(0, 3), std::out_of_range);
  EXPECT_NO_THROW((void)a.at(1, 2));
}

TEST(Array, WildcardIsOnePastTheLastRealSymbol) {
  const covarray::array a(1, 1, 6, 1);
  EXPECT_EQ(a.wildcard(), 6);
  EXPECT_TRUE(a.is_wildcard(6));
  EXPECT_FALSE(a.is_wildcard(5));
}

TEST(Array, FindInvalidSymbolIsNulloptForAFreshArray) {
  const covarray::array a(4, 4, 3, 2);
  EXPECT_FALSE(a.find_invalid_symbol().has_value());
  EXPECT_NO_THROW(a.validate_symbols());
}

TEST(Array, FindInvalidSymbolLocatesTheFirstOutOfRangeEntry) {
  covarray::array a(2, 2, 3, 2);
  a(1, 1) = 4;  // valid symbols are 0..3 (3 is the wildcard)
  const auto violation = a.find_invalid_symbol();
  ASSERT_TRUE(violation.has_value());
  EXPECT_EQ(violation->row, 1u);
  EXPECT_EQ(violation->column, 1u);
  EXPECT_EQ(violation->value, 4);
  EXPECT_THROW(a.validate_symbols(), std::invalid_argument);
}

TEST(Array, WildcardSymbolItselfIsValid) {
  covarray::array a(1, 1, 3, 1);
  a(0, 0) = 3;  // the wildcard, not "a symbol too many"
  EXPECT_FALSE(a.find_invalid_symbol().has_value());
}

TEST(CheckDimensions, RejectsOrderOutOfRange) {
  EXPECT_THROW(covarray::check_dimensions(1, 1, 0, 0), std::invalid_argument);
  EXPECT_THROW(covarray::check_dimensions(1, 1, 256, 0), std::invalid_argument);
  EXPECT_NO_THROW(covarray::check_dimensions(1, 1, 255, 0));
}

TEST(CheckDimensions, RejectsStrengthAboveColumnCount) {
  EXPECT_THROW(covarray::check_dimensions(1, 4, 2, 5), std::invalid_argument);
  EXPECT_NO_THROW(covarray::check_dimensions(1, 4, 2, 4));
}

TEST(CheckDimensions, ZeroColumnsRequiresZeroStrength) {
  EXPECT_NO_THROW(covarray::check_dimensions(1, 0, 2, 0));
  EXPECT_THROW(covarray::check_dimensions(1, 0, 2, 1), std::invalid_argument);
}

TEST(CheckDimensions, NIsUnconstrained) {
  EXPECT_NO_THROW(covarray::check_dimensions(0, 4, 2, 2));
  EXPECT_NO_THROW(covarray::check_dimensions(1'000'000, 4, 2, 2));
}

}  // namespace
