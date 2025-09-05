option(BUILD_SHARED_LIBS "Build shared libraries" ON)
set(BUILD_SHARED_LIBS ON)
cmake_minimum_required(VERSION 3.21)
project(NeonStoreSystem VERSION 1.0.0 LANGUAGES CXX)
option(ENABLE_PIE "Enable PIE for executables (Linux)" ON)
option(ENABLE_GLIBCXX_ASSERTIONS "Enable _GLIBCXX_ASSERTIONS for libstdc++" OFF)

option(ENABLE_REPRODUCIBLE "Enable reproducible builds (ffile-prefix-map, SOURCE_DATE_EPOCH)" ON)

option(BUILD_TESTING "Build tests" ON)
option(ENABLE_WARNINGS_AS_ERRORS "Treat warnings as errors" ON)
option(ENABLE_SANITIZERS "Enable ASan/UBSan in Debug on non-MSVC" ON)
option(BUILD_COVERAGE "Enable code coverage (GCC/Clang, Debug)" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if (ENABLE_GLIBCXX_ASSERTIONS AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  add_compile_definitions(_GLIBCXX_ASSERTIONS)
endif()

# Reproducible builds
if (ENABLE_REPRODUCIBLE)
  if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(-ffile-prefix-map=${CMAKE_SOURCE_DIR}=. -fmacro-prefix-map=${CMAKE_SOURCE_DIR}=.)
  endif()
  # Honor SOURCE_DATE_EPOCH if set (embed build timestamp deterministically)
  if (DEFINED ENV{SOURCE_DATE_EPOCH})
    add_definitions(-DNEONSTORE_BUILD_EPOCH=$ENV{SOURCE_DATE_EPOCH})
  endif()
endif()

if (MSVC)
  add_compile_options(/W4)
  if (ENABLE_WARNINGS_AS_ERRORS)
    add_compile_options(/WX)
  endif()
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
  if (ENABLE_WARNINGS_AS_ERRORS)
    add_compile_options(-Werror)
  endif()
endif()

if (ENABLE_SANITIZERS AND NOT MSVC)
  if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-fsanitize=address,undefined)
    link_libraries(asan ubsan)
  endif()
endif()

if (BUILD_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(--coverage)
    link_libraries(gcov)
  endif()
endif()

include(GNUInstallDirs)

add_library(neonstore_lib
  src/product.cpp
  src/inventory.cpp
  src/order.cpp
  src/csv_storage.cpp
)
target_include_directories(neonstore PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_compile_features(neonstore PUBLIC cxx_std_20)

add_executable(neonstore src/main.cpp)
target_link_libraries(neonstore PRIVATE neonstore_lib)

include(CTest)
if (BUILD_TESTING)
  add_executable(unit_tests
    tests/test_inventory.cpp
    tests/test_storage.cpp
  )
  target_link_libraries(unit_tests PRIVATE neonstore_lib)
  add_test(NAME unit_tests COMMAND unit_tests)
endif()

install(TARGETS neonstore neonstore_lib
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# Packaging
include("${CMAKE_SOURCE_DIR}/packaging/CPackConfig.cmake")


option(ENABLE_FUZZING "Build libFuzzer targets (Clang)" OFF)
option(ENABLE_BENCHMARKS "Build simple benchmarks" ON)

if (ENABLE_FUZZING)
  add_executable(fuzz_csv_products fuzz/fuzz_csv_products.cpp)
  target_link_libraries(fuzz_csv_products PRIVATE neonstore_lib)
  if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(fuzz_csv_products PRIVATE -fsanitize=fuzzer,address,undefined)
    target_link_options(fuzz_csv_products PRIVATE -fsanitize=fuzzer,address,undefined)
  endif()
endif()

if (ENABLE_BENCHMARKS)
  add_executable(bench_inventory benchmarks/bench_inventory.cpp)
  target_link_libraries(bench_inventory PRIVATE neonstore_lib)
endif()



# Hardening and LTO
option(ENABLE_LTO "Enable Link Time Optimization (Release)" ON)
option(ENABLE_HARDENING "Enable security hardening flags" ON)
option(ENABLE_SQLITE "Enable optional SQLite backend" OFF)

if (ENABLE_HARDENING)
  if (MSVC)
    add_compile_options(/guard:cf)
  else()
    add_compile_options(-D_FORTIFY_SOURCE=3 -fstack-protector-strong -fno-plt -fPIE)
    if (CMAKE_SYSTEM_NAME MATCHES "Linux")
      add_link_options(-Wl,-z,relro -Wl,-z,now -pie)
    endif()
  endif()
endif()

if (ENABLE_LTO AND CMAKE_BUILD_TYPE STREQUAL "Release")
  include(CheckIPOSupported)
  check_ipo_supported(RESULT ipo_supported OUTPUT ipo_msg)
  if (ipo_supported)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
  endif()
endif()

# Optional SQLite backend
if (ENABLE_SQLITE)
  find_package(SQLite3)
  if (SQLite3_FOUND)
    add_compile_definitions(NEONSTORE_ENABLE_SQLITE)
    target_sources(neonstore_lib PRIVATE src/storage_sqlite.cpp)
    target_link_libraries(neonstore_lib PRIVATE SQLite::SQLite3)
  endif()
endif()

# Precompiled headers
target_precompile_headers(neonstore_lib PRIVATE include/neonstore/pch.hpp)

include(CMakePackageConfigHelpers)

# Export targets for consumers
install(TARGETS neonstore_lib neonstore
  EXPORT NeonStoreTargets
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(EXPORT NeonStoreTargets
  FILE NeonStoreTargets.cmake
  NAMESPACE NeonStore::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NeonStoreSystem
)

configure_package_config_file(
  ${CMAKE_SOURCE_DIR}/cmake/NeonStoreSystemConfig.cmake.in
  ${CMAKE_BINARY_DIR}/NeonStoreSystemConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NeonStoreSystem
)

write_basic_package_version_file(
  ${CMAKE_BINARY_DIR}/NeonStoreSystemConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)

install(FILES
  ${CMAKE_BINARY_DIR}/NeonStoreSystemConfig.cmake
  ${CMAKE_BINARY_DIR}/NeonStoreSystemConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NeonStoreSystem
)


# Manpage install
install(FILES docs/neonstore.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)


find_package(Doxygen QUIET)
if (DOXYGEN_FOUND)
  add_custom_target(doc
    COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_SOURCE_DIR}/Doxyfile
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Generating API documentation with Doxygen" VERBATIM)
endif()


option(ENABLE_THREADSAFE "Enable thread-safety locks in Inventory" ON)
if (ENABLE_THREADSAFE)
  add_compile_definitions(NEONSTORE_THREADSAFE)
endif()

option(BUILD_CLI "Build CLI app" ON)

# Git SHA for versioning
find_package(Git QUIET)
if (GIT_FOUND)
  execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --short=12 HEAD
                  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                  OUTPUT_VARIABLE GIT_SHA OUTPUT_STRIP_TRAILING_WHITESPACE
                  ERROR_QUIET)
  if (GIT_SHA)
    add_compile_definitions(NEONSTORE_GIT_SHA=\"${GIT_SHA}\")
  endif()
endif()

# Completions install
install(FILES tools/completions/neonstore.bash DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/bash-completion/completions RENAME neonstore)
install(FILES tools/completions/neonstore.zsh  DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/zsh/site-functions RENAME _neonstore)
install(FILES tools/completions/neonstore.fish DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/fish/vendor_completions.d RENAME neonstore.fish)

# Uninstall target
configure_file(${CMAKE_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in ${CMAKE_BINARY_DIR}/cmake_uninstall.cmake IMMEDIATE @ONLY)
add_custom_target(uninstall COMMAND ${CMAKE_COMMAND} -P ${CMAKE_BINARY_DIR}/cmake_uninstall.cmake)



# Symbol visibility (non-MSVC)
if (NOT MSVC)
  target_compile_options(neonstore_lib PRIVATE -fvisibility=hidden -fvisibility-inlines-hidden)
endif()

# Export macro define for library
target_compile_definitions(neonstore_lib PRIVATE NEONSTORE_BUILD)

# Versioned shared library (SONAME on Unix)
set_target_properties(neonstore_lib PROPERTIES
  VERSION ${PROJECT_VERSION}
  SOVERSION ${PROJECT_VERSION_MAJOR}
)


configure_file(${CMAKE_SOURCE_DIR}/packaging/neonstore.pc.in ${CMAKE_BINARY_DIR}/neonstore.pc @ONLY)
install(FILES ${CMAKE_BINARY_DIR}/neonstore.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)


# Discover tests automatically
file(GLOB TEST_SOURCES CONFIGURE_DEPENDS tests/*.cpp)
foreach(t ${TEST_SOURCES})
  get_filename_component(n ${t} NAME_WE)
  add_executable(${n} ${t})
  target_link_libraries(${n} PRIVATE neonstore_lib)
  add_test(NAME ${n} COMMAND ${n})
endforeach()

# CLI E2E on UNIX
if (UNIX)
  add_test(NAME cli_unix COMMAND bash ${CMAKE_SOURCE_DIR}/tests/test_cli_unix.sh $<TARGET_FILE:neonstore>)
endif()

# Reproducible builds: honor SOURCE_DATE_EPOCH
if(DEFINED ENV{SOURCE_DATE_EPOCH})
  add_compile_definitions(SOURCE_DATE_EPOCH=$ENV{SOURCE_DATE_EPOCH})
  if (NOT MSVC)
    add_compile_options(-ffile-prefix-map=${CMAKE_SOURCE_DIR}=.)
  endif()
endif()


# C API library
add_library(neonstore_c SHARED src/c_api.cpp)
target_link_libraries(neonstore_c PRIVATE neonstore_lib)
set_target_properties(neonstore_c PROPERTIES
  VERSION ${PROJECT_VERSION}
  SOVERSION ${PROJECT_VERSION_MAJOR}
  OUTPUT_NAME neonstore_c
)
install(TARGETS neonstore_c
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(FILES include/neonstore/c_api.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/neonstore)

# Optional Python bindings
option(ENABLE_PYTHON "Build optional Python bindings via pybind11" OFF)
if (ENABLE_PYTHON)
  add_subdirectory(bindings/python)
endif()


# ELF symbol versioning for C API on non-MSVC platforms
if (UNIX AND NOT APPLE)
  target_link_options(neonstore_c PRIVATE "-Wl,--version-script=${CMAKE_SOURCE_DIR}/packaging/neonstore.map")
endif()


option(BUILD_STATIC_LIB "Also build a static library" ON)
if (BUILD_STATIC_LIB)
  add_library(neonstore_lib_static STATIC $<TARGET_PROPERTY:neonstore_lib,SOURCES>)
  target_include_directories(neonstore_lib_static PUBLIC ${CMAKE_SOURCE_DIR}/include)
  set_target_properties(neonstore_lib_static PROPERTIES OUTPUT_NAME neonstore_lib_s)
endif()


option(EXTRA_WARNINGS "Enable extra warnings like -Wconversion, -Wshadow" ON)
if (EXTRA_WARNINGS AND NOT MSVC)
  target_compile_options(neonstore_lib PRIVATE -Wconversion -Wsign-conversion -Wshadow -Wdouble-promotion)
endif()


# Install man pages and completions
install(FILES man/neonstore.1 DESTINATION share/man/man1)
install(FILES man/neonstore-config.5 DESTINATION share/man/man5)
install(FILES tools/completions/neonstore.bash DESTINATION share/bash-completion/completions RENAME neonstore)
install(FILES tools/completions/neonstore.zsh DESTINATION share/zsh/site-functions RENAME _neonstore)
install(FILES tools/completions/neonstore.fish DESTINATION share/fish/vendor_completions.d)


# Build info macros
string(TIMESTAMP NEONSTORE_BUILD_TS_UTC "%Y-%m-%dT%H:%M:%SZ" UTC)
set(NEONSTORE_GIT_SHA "unknown")
find_package(Git QUIET)
if (Git_FOUND)
  execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
                  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                  OUTPUT_VARIABLE NEONSTORE_GIT_SHA
                  OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()
add_compile_definitions(
  NEONSTORE_BUILD_TS="${NEONSTORE_BUILD_TS_UTC}"
  NEONSTORE_COMPILER_ID="${CMAKE_CXX_COMPILER_ID}"
  NEONSTORE_BUILD_TYPE="${CMAKE_BUILD_TYPE}"
  NEONSTORE_VERSION="2.21.0"
  NEONSTORE_GIT_SHA="${NEONSTORE_GIT_SHA}"
)


option(SANITIZE_ADDRESS "Enable AddressSanitizer" OFF)
option(SANITIZE_UNDEFINED "Enable UndefinedBehaviorSanitizer" OFF)
if (NOT MSVC)
  if (SANITIZE_ADDRESS)
    add_compile_options(-fsanitize=address)
    add_link_options(-fsanitize=address)
  endif()
  if (SANITIZE_UNDEFINED)
    add_compile_options(-fsanitize=undefined)
    add_link_options(-fsanitize=undefined)
  endif()
endif()

# Respect SOURCE_DATE_EPOCH if present (expose as macro for tooling)
if (DEFINED ENV{SOURCE_DATE_EPOCH})
  add_compile_definitions(NEONSTORE_SOURCE_DATE_EPOCH="$ENV{SOURCE_DATE_EPOCH}")
else()
  add_compile_definitions(NEONSTORE_SOURCE_DATE_EPOCH="")
endif()

# Packaging (CPack minimal TGZ)
set(CPACK_PACKAGE_NAME "neonstore")
set(CPACK_PACKAGE_VENDOR "NeonStore Project")
set(CPACK_PACKAGE_VERSION ${NEONSTORE_VERSION})
set(CPACK_GENERATOR "TGZ")
include(CPack)


option(NEONSTORE_STRICT "Enable strict compiler warnings" ON)
if (NEONSTORE_STRICT)
  if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(-Wall -Wextra -Wpedantic)
  elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_compile_options(/W4)
  endif()
endif()

enable_testing()
configure_file(include/neonstore/version.hpp.in ${CMAKE_CURRENT_BINARY_DIR}/generated/version.hpp @ONLY)


option(NEONSTORE_SANITIZE "Enable Address/UB sanitizers in supported compilers" OFF)
if (NEONSTORE_SANITIZE)
  if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    link_libraries(-fsanitize=address,undefined)
  endif()
endif()


# Reproducible build option: use SOURCE_DATE_EPOCH if defined
option(NEONSTORE_REPRO_BUILD "Use SOURCE_DATE_EPOCH for built_at in `version`" OFF)
if (NEONSTORE_REPRO_BUILD)
  add_definitions(-DNEONSTORE_REPRO_BUILD=1)
  if(DEFINED ENV{SOURCE_DATE_EPOCH})
    add_definitions(-DSOURCE_DATE_EPOCH_STR="$ENV{SOURCE_DATE_EPOCH}")
  endif()
endif()


add_test(NAME cli_config_roundtrip COMMAND neonstore config set demo.key value)
add_test(NAME cli_config_get COMMAND neonstore config get demo.key)
add_test(NAME cli_backup_prune COMMAND neonstore backup prune --dir backups --keep 1)


# Optional hardening flags (best-effort; ignored if unsupported)
option(NEONSTORE_HARDEN "Enable additional hardening flags" OFF)
if (NEONSTORE_HARDEN)
  if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(-fstack-protector-strong -D_FORTIFY_SOURCE=2 -fPIC)
    add_link_options(-Wl,-z,relro -Wl,-z,now -pie)
  endif()
endif()


add_test(NAME cli_seal_cycle COMMAND neonstore seal create)
add_test(NAME cli_health COMMAND neonstore health --output json)


# Treat warnings as errors (optional)
option(NEONSTORE_WERROR "Treat warnings as errors" OFF)
if (NEONSTORE_WERROR)
  if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(-Werror)
  elseif (MSVC)
    add_compile_options(/WX)
  endif()
endif()


add_test(NAME cli_analytics_example COMMAND neonstore analytics summary --dir examples/minidataset --output json)
add_test(NAME cli_csv_validate_example COMMAND neonstore csv validate --dir examples/minidataset --output json)


add_test(NAME cli_query_example COMMAND neonstore query products --dir examples/minidataset --where "price >= 1 && stock >= 0" --limit 2 --output json)
add_test(NAME cli_doctor_example COMMAND neonstore doctor checkup --dir examples/minidataset --output json)

# ---- CPack package metadata ----
set(CPACK_PACKAGE_NAME "neonstoresystem")
set(CPACK_PACKAGE_VENDOR "NeonStoreSystem Authors")
set(CPACK_PACKAGE_CONTACT "security@neonstoresystem.org")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "World-class C++ store management CLI and library")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.md")
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "NeonStoreSystem Authors")
set(CPACK_RPM_PACKAGE_LICENSE "Apache-2.0")
set(CPACK_PACKAGING_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")
set(CPACK_GENERATOR "TGZ;ZIP;DEB;RPM")
include(CPack)

# Windows NSIS generator options
if (WIN32)
  set(CPACK_GENERATOR "${CPACK_GENERATOR};NSIS")
  set(CPACK_NSIS_DISPLAY_NAME "NeonStoreSystem")
  set(CPACK_NSIS_CONTACT "security@neonstoresystem.org")
  set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
endif()

# Install shell completions
if (UNIX)
  include(GNUInstallDirs)
  install(FILES completions/neonstore.bash DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions RENAME neonstore)
  install(FILES completions/neonstore.fish DESTINATION ${CMAKE_INSTALL_DATADIR}/fish/vendor_completions.d)
  install(FILES completions/neonstore.zsh DESTINATION ${CMAKE_INSTALL_DATADIR}/zsh/site-functions RENAME _neonstore)
endif()

# Install manpage if present
if (EXISTS "${CMAKE_SOURCE_DIR}/docs/man/neonstore.1")
  include(GNUInstallDirs)
  install(FILES docs/man/neonstore.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
endif()

# Compiler cache (sccache) support
find_program(SCCACHE sccache)
if (SCCACHE)
  set(CMAKE_C_COMPILER_LAUNCHER ${SCCACHE})
  set(CMAKE_CXX_COMPILER_LAUNCHER ${SCCACHE})
endif()

if (ENABLE_PIE AND UNIX AND NOT APPLE)
  add_compile_options(-fPIE)
  # Apply -pie to executables via global link options; shared libs ignore it
  add_link_options(-pie)
endif()
