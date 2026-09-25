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

# The bound on the steps of one constant evaluation, for alef and for what
# imports it: enough for the tables. The constexpr tests, which read whole
# test files while compiled, ask for more.
set(ALEF_CONSTEXPR_STEPS 100000000 CACHE STRING
  "clang's -fconstexpr-steps for alef and what imports it")

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
  # Said on every module, so that every one of them and everything that
  # imports any of them compiles with the same bound; see below.
  target_compile_options(${target} PUBLIC
    $<$<COMPILE_LANG_AND_ID:CXX,Clang,AppleClang>:-fconstexpr-steps=${ALEF_CONSTEXPR_STEPS}>)
endfunction()

alef_module(alef_ucd src/ucd.cc)
alef_module(alef_tables_grapheme src/tables/grapheme.cc alef_ucd)
alef_module(alef_tables_normalization src/tables/normalization.cc alef_ucd)
alef_module(alef_tables_word src/tables/word.cc alef_ucd)
alef_module(alef_tables_sentence src/tables/sentence.cc alef_ucd)
alef_module(alef_tables_identifier src/tables/identifier.cc alef_ucd)
alef_module(alef_tables_general_category src/tables/general_category.cc alef_ucd)
alef_module(alef_tables_east_asian_width src/tables/east_asian_width.cc alef_ucd)
alef_module(alef_tables_script src/tables/script.cc alef_ucd)
alef_module(alef_tables_width src/tables/width.cc alef_ucd)
alef_module(alef_tables_casing src/tables/casing.cc alef_ucd)
alef_module(alef_tables_line src/tables/line.cc alef_ucd)
alef_module(alef_tables_bidi src/tables/bidi.cc alef_ucd)
alef_module(alef_utf src/utf.cc)
alef_module(alef_grapheme src/grapheme.cc alef_utf alef_tables_grapheme)
alef_module(alef_normalization src/normalization.cc
  alef_utf alef_tables_normalization)
alef_module(alef_word src/word.cc alef_utf alef_tables_word)
alef_module(alef_sentence src/sentence.cc alef_utf alef_tables_sentence)
alef_module(alef_identifier src/identifier.cc alef_utf alef_tables_identifier)
alef_module(alef_properties src/properties.cc alef_utf alef_grapheme
  alef_tables_general_category alef_tables_east_asian_width alef_tables_script
  alef_tables_width)
alef_module(alef_casing src/casing.cc alef_utf alef_word alef_tables_casing
  alef_tables_normalization)
alef_module(alef_line src/line.cc alef_utf alef_tables_line)
alef_module(alef_bidi src/bidi.cc alef_utf alef_tables_bidi)
alef_module(alef src/alef.cc alef_utf alef_grapheme alef_normalization alef_word alef_sentence alef_identifier
  alef_properties alef_casing alef_line alef_bidi)

add_library(alef::alef ALIAS alef)
add_library(alef::utf ALIAS alef_utf)
add_library(alef::grapheme ALIAS alef_grapheme)
add_library(alef::normalization ALIAS alef_normalization)
add_library(alef::word ALIAS alef_word)
add_library(alef::sentence ALIAS alef_sentence)
add_library(alef::identifier ALIAS alef_identifier)
add_library(alef::properties ALIAS alef_properties)
add_library(alef::casing ALIAS alef_casing)
add_library(alef::line ALIAS alef_line)
add_library(alef::bidi ALIAS alef_bidi)

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
# ceiling and nothing else. It is said by alef_module, above, on every module.
#
# GCC is not given one yet: what it needs has not been measured.

option(ALEF_TESTS "Build the tests" ${PROJECT_IS_TOP_LEVEL})
if(ALEF_TESTS)
  enable_testing()
  add_subdirectory(test)
endif()
