add_library(pulse_core INTERFACE)
target_include_directories(pulse_core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(pulse_core INTERFACE cxx_std_20)
