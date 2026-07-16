#include "mdp/instrumentation.hpp"

#include <gtest/gtest.h>

TEST(Instrumentation, RecordsPercentiles) {
  mdp::LatencyHist h("test", 1, 1000000, 3);
  for (int i = 1; i <= 1000; ++i) {
    h.record(i);
  }
  EXPECT_EQ(h.count(), 1000);
  EXPECT_GE(h.value_at_percentile(50.0), 400);
  EXPECT_LE(h.value_at_percentile(50.0), 600);
  EXPECT_GE(h.max(), 1000);
}

TEST(Instrumentation, ThroughputCounter) {
  mdp::PipelineStats stats;
  stats.start_ns = 0;
  stats.end_ns = 1'000'000'000ull;
  for (int i = 0; i < 5000; ++i) {
    stats.note_message();
  }
  EXPECT_DOUBLE_EQ(stats.throughput_mps(), 5000.0);
  const std::string table = stats.format_table();
  EXPECT_NE(table.find("wire_to_parse"), std::string::npos);
  EXPECT_NE(table.find("throughput_mps="), std::string::npos);
}

TEST(Instrumentation, NowNsMonotonicEnough) {
  const uint64_t a = mdp::now_ns();
  const uint64_t b = mdp::now_ns();
  EXPECT_GE(b, a);
}
