add_executable(mdp_bench
  bench_parser.cpp
  bench_spsc.cpp
  bench_book.cpp
)
target_link_libraries(mdp_bench PRIVATE mdp_core benchmark::benchmark benchmark::benchmark_main)
