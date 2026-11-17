#include <covarray/array.hpp>
#include <covarray/coverage.hpp>

#include <cstdio>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include <gtest/gtest.h>

namespace {

// CA(4; 2, 3, 2): every pair of the 3 columns covers all 4 binary tuples.
covarray::array binary_ca_2_3_2() {
  covarray::array a(4, 3, 2, 2);
  const covarray::symbol_type rows[4][3] = {
      {0, 0, 0},
      {0, 1, 1},
      {1, 0, 1},
      {1, 1, 0},
  };
  for (std::size_t r = 0; r < 4; ++r) {
    for (std::size_t c = 0; c < 3; ++c) {
      a(r, c) = rows[r][c];
    }
  }
  return a;
}

TEST(coverage, ValidArrayPasses) {
  const covarray::array a = binary_ca_2_3_2();
  const covarray::verify_result result = covarray::verify_fast(a);
  EXPECT_TRUE(result.is_covering_array);
  EXPECT_EQ(result.combinations_checked, 3u);  // C(3, 2)
  EXPECT_EQ(result.combinations_with_missing, 0u);
  EXPECT_EQ(result.missing_tuples_total, 0u);
  EXPECT_TRUE(result.missing.empty());
}

TEST(coverage, DefaultModeHaltsAtTheFirstDeficientCombination) {
  // Drop the last row of the valid array: columns (0, 1) lose tuple (1, 1).
  covarray::array a(3, 3, 2, 2);
  const covarray::symbol_type rows[3][3] = {
      {0, 0, 0},
      {0, 1, 1},
      {1, 0, 1},
  };
  for (std::size_t r = 0; r < 3; ++r) {
    for (std::size_t c = 0; c < 3; ++c) {
      a(r, c) = rows[r][c];
    }
  }

  const covarray::verify_result result = covarray::verify_fast(a);
  EXPECT_FALSE(result.is_covering_array);
  EXPECT_EQ(result.combinations_checked, 1u);  // halted after the first combo
  EXPECT_EQ(result.combinations_with_missing, 1u);
  EXPECT_GT(result.missing_tuples_total, 0u);
  EXPECT_TRUE(result.missing.empty());  // collect_missing defaults to false
}

TEST(coverage, AllColumnsScansEveryCombinationAndAccumulates) {
  covarray::array a(3, 3, 2, 2);
  const covarray::symbol_type rows[3][3] = {
      {0, 0, 0},
      {0, 1, 1},
      {1, 0, 1},
  };
  for (std::size_t r = 0; r < 3; ++r) {
    for (std::size_t c = 0; c < 3; ++c) {
      a(r, c) = rows[r][c];
    }
  }

  covarray::verify_options options;
  options.check_all_combinations = true;
  const covarray::verify_result result = covarray::verify_fast(a, options);

  EXPECT_FALSE(result.is_covering_array);
  EXPECT_EQ(result.combinations_checked, 3u);  // every C(3, 2), not just the first failure
  EXPECT_GE(result.combinations_with_missing, 1u);
  EXPECT_GT(result.missing_tuples_total, 0u);
}

TEST(coverage, CollectMissingRecordsColumnsAndTuples) {
  covarray::array a(3, 3, 2, 2);
  const covarray::symbol_type rows[3][3] = {
      {0, 0, 0},
      {0, 1, 1},
      {1, 0, 1},
  };
  for (std::size_t r = 0; r < 3; ++r) {
    for (std::size_t c = 0; c < 3; ++c) {
      a(r, c) = rows[r][c];
    }
  }

  covarray::verify_options options;
  options.collect_missing = true;
  const covarray::verify_result result = covarray::verify_fast(a, options);

  ASSERT_FALSE(result.missing.empty());
  const covarray::missing_combination& combo = result.missing.front();
  const std::vector<std::size_t> expected_columns{0, 1};
  EXPECT_EQ(combo.columns, expected_columns);
  ASSERT_EQ(combo.tuples.size(), 1u);
  const covarray::tuple_values expected_tuple{1, 1};
  EXPECT_EQ(combo.tuples.front(), expected_tuple);
}

TEST(coverage, WildcardRowIsIgnored) {
  // Two rows, v = 2: row 0 has a wildcard and so covers nothing; row 1 only
  // covers (0, 0). The pair (1, 1) is never covered by either row.
  covarray::array a(2, 2, 2, 2);
  a(0, 0) = a.wildcard();
  a(0, 1) = a.wildcard();
  a(1, 0) = 0;
  a(1, 1) = 0;

  const covarray::verify_result result = covarray::verify_fast(a);
  EXPECT_FALSE(result.is_covering_array);
  EXPECT_EQ(result.combinations_checked, 1u);
  EXPECT_EQ(result.missing_tuples_total, 3u);
}

TEST(coverage, TupleSpaceOverflowThrows) {
  // 255^30 does not fit in 64-bit std::size_t.
  const covarray::array a(1, 30, 255, 30);
  EXPECT_THROW((void)covarray::verify_fast(a), std::overflow_error);
}

TEST(coverage, TrivialStrengthZeroAlwaysPasses) {
  const covarray::array a(0, 4, 3, 0);
  const covarray::verify_result result = covarray::verify_fast(a);
  EXPECT_TRUE(result.is_covering_array);
  EXPECT_EQ(result.combinations_checked, 0u);
}

TEST(Summarize, ReportsCountsWithNoLocation) {
  covarray::verify_result result;
  result.combinations_checked = 10;
  result.combinations_with_missing = 2;
  result.missing_tuples_total = 5;

  const std::string summary = covarray::summarize(result);
  EXPECT_NE(summary.find('2'), std::string::npos);
  EXPECT_NE(summary.find("10"), std::string::npos);
  EXPECT_NE(summary.find('5'), std::string::npos);
  // No column index or tuple value is named.
  EXPECT_EQ(summary.find("column 0"), std::string::npos);
  EXPECT_EQ(summary.find("tuple ("), std::string::npos);
}

TEST(MissingReportFilename, MatchesTheProjectsHeaderFormat) {
  const covarray::array a(3886, 12, 6, 4);
  EXPECT_EQ(covarray::missing_report_filename(a), "N3886k12v6^12t4.missing");
}

TEST(WriteMissing, ProducesOneBlockPerDeficientCombination) {
  covarray::array a(3, 3, 2, 2);
  const covarray::symbol_type rows[3][3] = {
      {0, 0, 0},
      {0, 1, 1},
      {1, 0, 1},
  };
  for (std::size_t r = 0; r < 3; ++r) {
    for (std::size_t c = 0; c < 3; ++c) {
      a(r, c) = rows[r][c];
    }
  }

  covarray::verify_options options;
  options.check_all_combinations = true;
  options.collect_missing = true;
  const covarray::verify_result result = covarray::verify_fast(a, options);

  const std::string path = "test_write_missing_output.missing";
  covarray::write_missing(path, a, result);

  std::ifstream in(path);
  ASSERT_TRUE(in.is_open());
  std::ostringstream contents;
  contents << in.rdbuf();
  in.close();
  std::remove(path.c_str());

  const std::string text = contents.str();
  EXPECT_NE(text.find("columns 1 2 : 1 missing"), std::string::npos);
  EXPECT_NE(text.find("1 1"), std::string::npos);
  EXPECT_NE(text.find("# total:"), std::string::npos);
}

}  // namespace
