
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_executable(fuzz_csv_dir fuzz_csv_dir.cpp)
  target_link_libraries(fuzz_csv_dir PRIVATE neonstore_lib)
  target_compile_options(fuzz_csv_dir PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)
  target_link_options(fuzz_csv_dir PRIVATE -fsanitize=fuzzer,address,undefined)
endif()
