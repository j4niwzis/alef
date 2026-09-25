cmake_minimum_required(VERSION 4.3.4 FATAL_ERROR)

# alef: Unicode for C++, read from the Unicode Character Database itself.
#
# Nothing here is generated. ucd/ holds the files of the UCD as the Unicode
# Consortium publishes them, #embed puts their bytes into the module, and
# constexpr code reads them while the module interface is compiled. A new
# version of Unicode is new files in ucd/ and nothing else.
include(${CMAKE_CURRENT_LIST_DIR}/cmake/import-std.cmake)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_EXTENSIONS OFF)

project(alef VERSION 0.1 LANGUAGES CXX)

add_library(alef)
add_library(alef::alef ALIAS alef)
target_sources(alef
  PUBLIC FILE_SET CXX_MODULES BASE_DIRS src FILES
    src/alef.cc
    src/utf8.cc
    src/ucd.cc
    src/grapheme.cc)
target_compile_features(alef PUBLIC cxx_std_23)

# Reading the UCD is constant evaluation over the whole of a file, far past
# what a compiler lets one constant expression do by default: clang counts
# about eighteen steps a byte, and its default is 1048576.
#
# PUBLIC, because with modules these interfaces are not compiled by this
# target alone. CMake compiles them again for a target whose settings differ,
# and a consumer of an installed alef compiles them itself -- in both cases
# with the usage requirements of alef and not with its private options. So
# what it takes to compile the interfaces is a usage requirement, the way an
# include directory is. What a consumer's own code gets from it is a higher
# ceiling and nothing else.
#
# GCC is not given one yet: what it needs has not been measured.
target_compile_options(alef PUBLIC
  $<$<COMPILE_LANG_AND_ID:CXX,Clang,AppleClang>:-fconstexpr-steps=100000000>)

option(ALEF_TESTS "Build the tests" ${PROJECT_IS_TOP_LEVEL})
if(ALEF_TESTS)
  enable_testing()
  add_subdirectory(test)
endif()
