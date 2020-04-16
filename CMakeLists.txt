cmake_minimum_required (VERSION 2.6)
set(CMAKE_CXX_STANDARD 17)
add_compile_options("-Wall")

if(CMAKE_BUILD_TYPE MATCHES Debug)
add_compile_options("-DDEBUG=1" "-ggdb")
else()
add_compile_options("-fomit-frame-pointer")
endif()

file(GLOB_RECURSE src_files "base/*.cpp" "callable/*.cpp" "container/*.cpp" "debug/*.cpp" "dsp/*.cpp" "src-extra/*.cpp" "stats/*.cpp")

# Doctest makes touble
set_source_files_properties(src-extra/tst_main.cpp PROPERTIES SKIP_UNITY_BUILD_INCLUSION TRUE)

project (v1util)
include_directories ("${PROJECT_SOURCE_DIR}/..")
set(v1util_srcs ${src_files})
list(FILTER v1util_srcs EXCLUDE REGEX "tst_.*\\.cpp")
add_library(v1util ${v1util_srcs})
if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
    AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "7"
    AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "9")
  target_link_libraries(v1util "-lstdc++fs")
  endif()
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang"
    AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS_EQUAL "9")
  target_link_libraries(v1util "-lstdc++fs")
  endif()
endif()


project (v1util-tests)
include_directories ("${PROJECT_SOURCE_DIR}/..")
include_directories ("${PROJECT_SOURCE_DIR}/third-party")
set(v1util_test_srcs ${src_files})
list(FILTER v1util_test_srcs INCLUDE REGEX "tst_.*\\.cpp")
list(FILTER v1util_test_srcs EXCLUDE REGEX "tst_sig.*\\.cpp") # TODO

add_executable(v1util-tests ${v1util_test_srcs})
add_dependencies(v1util-tests v1util)
target_link_libraries(v1util-tests "-lpthread")
target_link_libraries(v1util-tests v1util)
