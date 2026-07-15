#include "mdp/feed_handler.hpp"
#include "mdp/parser.hpp"

#include <gtest/gtest.h>

TEST(Simulator, GenerateSessionDeterministic) {
  mdp::SimulatorConfig cfg;
  cfg.messages = 100;
  cfg.symbols = 3;
  cfg.seed = 42;
  mdp::FeedSimulator sim(cfg);
  const auto a = sim.generate_session();
  const auto b = sim.generate_session();
  ASSERT_EQ(a.size(), 100u);
  ASSERT_EQ(b.size(), 100u);
  EXPECT_EQ(a[0], b[0]);
  EXPECT_EQ(a[50], b[50]);
}

TEST(Simulator, SessionMessagesParse) {
  mdp::SimulatorConfig cfg;
  cfg.messages = 50;
  cfg.seed = 7;
  mdp::FeedSimulator sim(cfg);
  const auto session = sim.generate_session();
  uint64_t expect = 1;
  for (const auto& frame : session) {
    const auto parsed = mdp::parse_message(frame);
    ASSERT_EQ(parsed.status, mdp::ParseStatus::Ok);
    const mdp::Header* h = mdp::message_header(parsed);
    ASSERT_NE(h, nullptr);
    EXPECT_EQ(h->seq, expect);
    ++expect;
  }
}
