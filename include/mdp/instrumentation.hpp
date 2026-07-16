#pragma once

// Why: HdrHistogram records high-percentile stage latencies with fixed memory
// so the hot path can stamp timestamps without allocating bucket maps that a
// plain std::map histogram would touch under load.

#include <cstdint>
#include <string>

extern "C" {
struct hdr_histogram;
}

namespace mdp {

class LatencyHist {
 public:
  LatencyHist(const char* name, int64_t min_ns, int64_t max_ns, int sigfigs);
  ~LatencyHist();

  LatencyHist(const LatencyHist&) = delete;
  LatencyHist& operator=(const LatencyHist&) = delete;

  void record(int64_t ns);
  int64_t value_at_percentile(double p) const;
  int64_t max() const;
  int64_t count() const;
  const char* name() const { return name_.c_str(); }

 private:
  std::string name_;
  hdr_histogram* hist_{nullptr};
};

struct PipelineStats {
  LatencyHist wire_to_parse{"wire_to_parse", 1, 1000000000LL, 3};
  LatencyHist parse_to_book{"parse_to_book", 1, 1000000000LL, 3};
  LatencyHist book_to_consumer{"book_to_consumer", 1, 1000000000LL, 3};
  uint64_t messages{0};
  uint64_t start_ns{0};
  uint64_t end_ns{0};

  void note_message() { ++messages; }
  double throughput_mps() const;
  std::string format_table() const;
};

uint64_t now_ns();

}  // namespace mdp
