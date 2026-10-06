include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG 317ac1ed4c0bb6e6b91eafc817e05c488feffcb3
  SYSTEM)
FetchContent_MakeAvailable(Catch2)

add_executable(pulse_tests
  book/digest_test.cpp
  book/order_book_test.cpp
  book/order_index_test.cpp
  engine/matching_engine_test.cpp
  feed/feed_handler_test.cpp
  net/socket_test.cpp
  pipeline/spsc_ring_test.cpp
  sim/order_flow_test.cpp
  sim/rng_test.cpp
  stats/latency_histogram_test.cpp
  wire/byte_io_test.cpp
  wire/codec_test.cpp
  wire/stream_decoder_test.cpp)
target_link_libraries(pulse_tests PRIVATE pulse_core pulse_warnings Catch2::Catch2WithMain)

include(Catch)
catch_discover_tests(pulse_tests DISCOVERY_MODE PRE_TEST)

add_test(NAME capture_round_trip
  COMMAND sh -c "\"$<TARGET_FILE:pulse-sim>\" --messages 200000 --instruments 4 --out round_trip.feed && \"$<TARGET_FILE:pulse-replay>\" round_trip.feed"
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
