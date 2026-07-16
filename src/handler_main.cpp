#include "mdp/feed_handler.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void usage(const char* argv0) {
  std::cerr << "usage: " << argv0
            << " [--host H] [--port P] [--recovery-port R] [--seconds N] [--stats] "
               "[--bbo PATH] [--tape PATH] [--bars PATH]\n";
}

}  // namespace

int main(int argc, char** argv) {
  mdp::HandlerConfig cfg;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    auto need = [&](const char* flag) -> std::string {
      if (i + 1 >= argc) {
        std::cerr << "missing value for " << flag << '\n';
        std::exit(2);
      }
      return argv[++i];
    };
    if (a == "--host") {
      cfg.udp_host = need("--host");
    } else if (a == "--port") {
      cfg.udp_port = static_cast<uint16_t>(std::stoi(need("--port")));
    } else if (a == "--recovery-port") {
      cfg.recovery_port = static_cast<uint16_t>(std::stoi(need("--recovery-port")));
    } else if (a == "--seconds") {
      cfg.run_seconds = std::stoi(need("--seconds"));
    } else if (a == "--bbo") {
      cfg.bbo_path = need("--bbo");
    } else if (a == "--tape") {
      cfg.tape_path = need("--tape");
    } else if (a == "--bars") {
      cfg.bars_path = need("--bars");
    } else if (a == "--stats") {
      cfg.print_stats = true;
    } else if (a == "--help") {
      usage(argv[0]);
      return 0;
    } else {
      usage(argv[0]);
      return 2;
    }
  }
  if (cfg.run_seconds <= 0) {
    cfg.run_seconds = 5;
  }
  mdp::FeedHandler handler(cfg);
  return handler.run();
}
