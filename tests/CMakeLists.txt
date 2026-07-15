add_executable(mdp_tests
  test_protocol.cpp
  test_parser.cpp
  test_sequence.cpp
  test_spsc.cpp
  test_book.cpp
  test_consumers.cpp
  test_instrumentation.cpp
  test_simulator.cpp
  test_replay.cpp
  test_fuzz_parser.cpp
  test_handler_integration.cpp
  test_coverage_extras.cpp
)
target_link_libraries(mdp_tests PRIVATE mdp_core GTest::gtest_main)
target_include_directories(mdp_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if(MDP_ENABLE_SANITIZERS)
  target_compile_options(mdp_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(mdp_tests PRIVATE -fsanitize=address,undefined)
endif()

include(GoogleTest)
gtest_discover_tests(mdp_tests)
