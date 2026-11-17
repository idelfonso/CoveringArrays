#include <covarray/array.hpp>
#include <covarray/io.hpp>

#include <sstream>
#include <stdexcept>

#include <gtest/gtest.h>

namespace {

TEST(ReadArray, ParsesTheCanonicalHeaderAndRows) {
  std::istringstream in("3 2 2^2 2\n0 1\n1 0\n1 1\n");
  const covarray::array a = covarray::read_array(in);
  EXPECT_EQ(a.rows(), 3u);
  EXPECT_EQ(a.columns(), 2u);
  EXPECT_EQ(a.order(), 2u);
  EXPECT_EQ(a.strength(), 2u);
  EXPECT_EQ(a(0, 0), 0);
  EXPECT_EQ(a(0, 1), 1);
  EXPECT_EQ(a(2, 1), 1);
}

TEST(ReadArray, WriteReadRoundTrips) {
  covarray::array original(4, 3, 5, 2);
  original(0, 0) = 1;
  original(1, 1) = 4;
  original(2, 2) = 5;  // the wildcard for v = 5
  original(3, 0) = 2;

  std::ostringstream out;
  covarray::write_array(out, original);

  std::istringstream in(out.str());
  const covarray::array round_tripped = covarray::read_array(in);

  EXPECT_EQ(round_tripped.rows(), original.rows());
  EXPECT_EQ(round_tripped.columns(), original.columns());
  EXPECT_EQ(round_tripped.order(), original.order());
  EXPECT_EQ(round_tripped.strength(), original.strength());
  for (std::size_t r = 0; r < original.rows(); ++r) {
    for (std::size_t c = 0; c < original.columns(); ++c) {
      EXPECT_EQ(round_tripped(r, c), original(r, c)) << "row " << r << ", column " << c;
    }
  }
}

TEST(ReadArray, HashCommentsAreStrippedAnywhereOnTheLine) {
  std::istringstream in("# a leading full-line comment\n"
                        "2 2 2^2 1   # trailing note on the header\n"
                        "0 1 # row 0\n"
                        "1 0\n");
  const covarray::array a = covarray::read_array(in);
  EXPECT_EQ(a.rows(), 2u);
  EXPECT_EQ(a(0, 0), 0);
  EXPECT_EQ(a(0, 1), 1);
}

TEST(ReadArray, LowerAndUpperCCommentsAreSkippedOnlyAsAFullLinePrefix) {
  std::istringstream in("c a valid comment, c followed by a blank\n"
                        "C another valid comment\n"
                        "2 2 2^2 1\n"
                        "0 1\n"
                        "1 0\n");
  const covarray::array a = covarray::read_array(in);
  EXPECT_EQ(a.rows(), 2u);
  EXPECT_EQ(a(1, 0), 1);
}

TEST(ReadArray, CImmediatelyFollowedByANonBlankIsNotAComment) {
  // "Cthis is another covering array" has no blank right after the 'C', so it
  // is data, not a comment — and it is not a valid header, so parsing fails.
  std::istringstream in("Cthis is not a comment\n2 2 2^2 1\n0 1\n1 0\n");
  EXPECT_THROW((void)covarray::read_array(in), std::runtime_error);
}

TEST(ReadArray, BlankLinesAreSkipped) {
  std::istringstream in("\n\n2 2 2^2 1\n\n0 1\n\n1 0\n\n");
  const covarray::array a = covarray::read_array(in);
  EXPECT_EQ(a.rows(), 2u);
}

TEST(ReadArray, MismatchedExponentIsAFormatError) {
  std::istringstream in("2 3 2^2 1\n0 1 0\n1 0 1\n");
  EXPECT_THROW((void)covarray::read_array(in), std::runtime_error);
}

TEST(ReadArray, MalformedVToKTokenIsAFormatError) {
  std::istringstream in("2 2 twelve 1\n0 1\n1 0\n");
  EXPECT_THROW((void)covarray::read_array(in), std::runtime_error);
}

TEST(ReadArray, NegativeSymbolIsRejected) {
  std::istringstream in("1 2 2^2 1\n-1 0\n");
  EXPECT_THROW((void)covarray::read_array(in), std::invalid_argument);
}

TEST(ReadArray, SymbolAboveVIsRejected) {
  // v = 2 means valid symbols are 0, 1 (real) or 2 (wildcard); 3 is not.
  std::istringstream in("1 2 2^2 1\n3 0\n");
  EXPECT_THROW((void)covarray::read_array(in), std::invalid_argument);
}

TEST(ReadArray, WildcardSymbolIsAccepted) {
  std::istringstream in("1 2 2^2 1\n2 0\n");
  const covarray::array a = covarray::read_array(in);
  EXPECT_EQ(a(0, 0), a.wildcard());
}

TEST(ReadArray, TruncatedFileIsAnEndOfInputError) {
  std::istringstream in("2 2 2^2 1\n0 1\n");
  EXPECT_THROW((void)covarray::read_array(in), std::runtime_error);
}

TEST(WriteArray, ProducesTheDocumentedHeaderShape) {
  const covarray::array a(3886, 12, 6, 4);
  std::ostringstream out;
  covarray::write_array(out, a);

  std::istringstream check(out.str());
  std::string header;
  std::getline(check, header);
  EXPECT_EQ(header, "3886 12 6^12 4");
}

}  // namespace
