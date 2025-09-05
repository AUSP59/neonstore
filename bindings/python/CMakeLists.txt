
cmake_minimum_required(VERSION 3.24)
find_package(pybind11 QUIET)
if (pybind11_FOUND)
  pybind11_add_module(neonstore_py MODULE bindings/python/neonstore_py.cpp)
  target_link_libraries(neonstore_py PRIVATE neonstore_lib)
  set_target_properties(neonstore_py PROPERTIES OUTPUT_NAME "neonstore")
endif()
