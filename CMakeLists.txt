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
    src/alef.cppm
    src/utf8.cppm
    src/ucd.cppm
    src/grapheme.cppm)
target_compile_features(alef PUBLIC cxx_std_23)

# No flags: the UCD is read in pieces small enough for a compiler's own
# bounds on constant evaluation (src/ucd.cppm), and #embed is kept quiet where
# it is used. Whoever compiles these interfaces compiles them as they are.

option(ALEF_TESTS "Build the tests" ${PROJECT_IS_TOP_LEVEL})
if(ALEF_TESTS)
  enable_testing()
  add_subdirectory(test)
endif()
