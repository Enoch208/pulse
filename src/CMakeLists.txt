add_library(pulse_core STATIC
  pulse/book/order_index.cpp
  pulse/wire/codec.cpp)
target_include_directories(pulse_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(pulse_core PUBLIC cxx_std_20)
target_link_libraries(pulse_core PRIVATE pulse_warnings)
