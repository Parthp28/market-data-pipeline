#include "mdp/feed_handler.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void usage(const char* argv0) {
  std::cerr << "usage: " << argv0
            << " [--host H] [--port P] [--recovery-port R] [--messages N] [--rate R] "
               "[--loss F] [--reorder F] [--seed S] [--quiet]\n";
}

}  // namespace

int main(int argc, char** argv) {
  mdp::SimulatorConfig cfg;
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
    } else if (a == "--messages") {
      cfg.messages = std::stoi(need("--messages"));
    } else if (a == "--rate") {
      cfg.rate_per_sec = std::stoi(need("--rate"));
    } else if (a == "--loss") {
      cfg.loss_rate = std::stod(need("--loss"));
    } else if (a == "--reorder") {
      cfg.reorder_rate = std::stod(need("--reorder"));
    } else if (a == "--seed") {
      cfg.seed = static_cast<uint64_t>(std::stoull(need("--seed")));
    } else if (a == "--symbols") {
      cfg.symbols = std::stoi(need("--symbols"));
    } else if (a == "--quiet") {
      cfg.quiet = true;
    } else if (a == "--help") {
      usage(argv[0]);
      return 0;
    } else {
      usage(argv[0]);
      return 2;
    }
  }
  mdp::FeedSimulator sim(cfg);
  return sim.run();
}
