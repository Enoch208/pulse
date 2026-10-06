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
  wire/byte_io_test.cpp
  wire/codec_test.cpp
  wire/stream_decoder_test.cpp)
target_link_libraries(pulse_tests PRIVATE pulse_core pulse_warnings Catch2::Catch2WithMain)

include(Catch)
catch_discover_tests(pulse_tests DISCOVERY_MODE PRE_TEST)
