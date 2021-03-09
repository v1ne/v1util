cmake_minimum_required (VERSION 3.19)
set(CMAKE_CXX_STANDARD 17)
add_compile_options("-Wall" "-Wno-multichar")

if(CMAKE_BUILD_TYPE MATCHES Debug)
add_compile_options("-DDEBUG=1" "-ggdb")
else()
add_compile_options("-fomit-frame-pointer")
endif()

if(UNIX)
# function/delegate require aligned functions in order to steal bits:
add_compile_options("-falign-functions=4")
endif()

file(GLOB_RECURSE src_files "base/*.cpp" "callable/*.cpp" "container/*.cpp" "debug/*.cpp" "dsp/*.cpp" "stl-plus/*.cpp" "src-extra/*.cpp" "stats/*.cpp")

# Doctest makes touble
set_source_files_properties(src-extra/tst_main.cpp PROPERTIES SKIP_UNITY_BUILD_INCLUSION TRUE)

project (v1util)
include_directories ("${PROJECT_SOURCE_DIR}/..")
set(v1util_srcs ${src_files})
list(FILTER v1util_srcs EXCLUDE REGEX "tst_.*\\.cpp")
add_library(v1util ${v1util_srcs})
if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
    AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "10")
      message( FATAL_ERROR "Please upgrade to GCC 10." )
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

add_executable(v1util-tests ${v1util_test_srcs})
add_dependencies(v1util-tests v1util)
target_link_libraries(v1util-tests "-lpthread")
target_link_libraries(v1util-tests v1util)
