#include "mdp/instrumentation.hpp"

#include <hdr/hdr_histogram.h>

#include <chrono>
#include <sstream>

namespace mdp {

LatencyHist::LatencyHist(const char* name, int64_t min_ns, int64_t max_ns, int sigfigs)
    : name_(name) {
  hdr_init(min_ns, max_ns, sigfigs, &hist_);
}

LatencyHist::~LatencyHist() {
  if (hist_) {
    hdr_close(hist_);
    hist_ = nullptr;
  }
}

void LatencyHist::record(int64_t ns) {
  if (hist_ && ns > 0) {
    hdr_record_value(hist_, ns);
  }
}

int64_t LatencyHist::value_at_percentile(double p) const {
  if (!hist_) {
    return 0;
  }
  return hdr_value_at_percentile(hist_, p);
}

int64_t LatencyHist::max() const {
  if (!hist_) {
    return 0;
  }
  return hdr_max(hist_);
}

int64_t LatencyHist::count() const {
  if (!hist_) {
    return 0;
  }
  return hist_->total_count;
}

double PipelineStats::throughput_mps() const {
  if (end_ns <= start_ns) {
    return 0.0;
  }
  const double seconds = static_cast<double>(end_ns - start_ns) / 1e9;
  if (seconds <= 0.0) {
    return 0.0;
  }
  return static_cast<double>(messages) / seconds;
}

std::string PipelineStats::format_table() const {
  std::ostringstream oss;
  oss << "stage            p50(ns)   p99(ns)  p99.9(ns)    max(ns)    count\n";
  const LatencyHist* hs[] = {&wire_to_parse, &parse_to_book, &book_to_consumer};
  for (const LatencyHist* h : hs) {
    oss.width(16);
    oss << std::left << h->name();
    oss << ' ';
    oss.width(9);
    oss << std::right << h->value_at_percentile(50.0);
    oss << ' ';
    oss.width(9);
    oss << h->value_at_percentile(99.0);
    oss << ' ';
    oss.width(10);
    oss << h->value_at_percentile(99.9);
    oss << ' ';
    oss.width(10);
    oss << h->max();
    oss << ' ';
    oss.width(8);
    oss << h->count();
    oss << '\n';
  }
  oss << "throughput_mps=" << throughput_mps() << " messages=" << messages << '\n';
  return oss.str();
}

uint64_t now_ns() {
  using clock = std::chrono::steady_clock;
  return static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(clock::now().time_since_epoch())
          .count());
}

}  // namespace mdp
