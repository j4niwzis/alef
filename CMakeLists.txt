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

# One module to a target. A change to a module compiles it again, and what
# imports it, and nothing else -- the tables least of all: each is a module of
# its own that imports nothing of the library but alef.ucd, the reader of the
# UCD, so a table is compiled again when its files or that reader change,
# and not when the code that looks things up in it does.
function(alef_module target file)
  add_library(${target})
  target_sources(${target}
    PUBLIC FILE_SET CXX_MODULES BASE_DIRS src FILES ${file})
  target_compile_features(${target} PUBLIC cxx_std_23)
  if(ARGN)
    target_link_libraries(${target} PUBLIC ${ARGN})
  endif()
endfunction()

alef_module(alef_ucd src/ucd.cc)
alef_module(alef_tables_grapheme src/tables/grapheme.cc alef_ucd)
alef_module(alef_tables_normalization src/tables/normalization.cc alef_ucd)
alef_module(alef_tables_word src/tables/word.cc alef_ucd)
alef_module(alef_utf src/utf.cc)
alef_module(alef_grapheme src/grapheme.cc alef_utf alef_tables_grapheme)
alef_module(alef_normalization src/normalization.cc
  alef_utf alef_tables_normalization)
alef_module(alef_word src/word.cc alef_utf alef_tables_word)
alef_module(alef src/alef.cc alef_utf alef_grapheme alef_normalization alef_word)

add_library(alef::alef ALIAS alef)
add_library(alef::utf ALIAS alef_utf)
add_library(alef::grapheme ALIAS alef_grapheme)
add_library(alef::normalization ALIAS alef_normalization)
add_library(alef::word ALIAS alef_word)

# Reading the UCD is constant evaluation over the whole of a file, far past
# what a compiler lets one constant expression do by default: its default is
# 1048576 steps in clang.
#
# PUBLIC, because with modules these interfaces are not compiled by these
# targets alone. CMake compiles them again for a target whose settings differ,
# and a consumer of an installed alef compiles them itself -- in both cases
# with the usage requirements of alef and not with its private options. So
# what it takes to compile the interfaces is a usage requirement, the way an
# include directory is. What a consumer's own code gets from it is a higher
# ceiling and nothing else. It is said on the reader, and everything that
# reads the UCD links it.
#
# GCC is not given one yet: what it needs has not been measured.
target_compile_options(alef_ucd PUBLIC
  $<$<COMPILE_LANG_AND_ID:CXX,Clang,AppleClang>:-fconstexpr-steps=100000000>)

option(ALEF_TESTS "Build the tests" ${PROJECT_IS_TOP_LEVEL})
if(ALEF_TESTS)
  enable_testing()
  add_subdirectory(test)
endif()
