foreach(benchmark bench_book bench_decode bench_index bench_queue)
  add_executable(${benchmark} ${benchmark}.cpp)
  target_link_libraries(${benchmark} PRIVATE pulse_core pulse_warnings)
endforeach()
