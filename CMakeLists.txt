cmake_minimum_required(VERSION 4.3.4 FATAL_ERROR)

# alef: Unicode for C++, read from the Unicode Character Database itself.
#
# Nothing here is generated. ucd/ holds the files of the UCD as the Unicode
# Consortium publishes them, #embed puts their bytes into the module, and
# constexpr code reads them while the module interface is compiled. A new
# version of Unicode is new files in ucd/ and nothing else.
include(${CMAKE_CURRENT_LIST_DIR}/cmake/import-std.cmake)
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_EXTENSIONS OFF)

project(alef VERSION 0.1.1 LANGUAGES CXX)

# The bound on the steps of one constant evaluation, for alef and for what
# imports it: enough for the tables. The constexpr tests, which read whole
# test files while compiled, ask for more.
set(ALEF_CONSTEXPR_STEPS 100000000 CACHE STRING
  "clang's -fconstexpr-steps for alef and what imports it")

# splice: the variant and the visiting, alef's one dependency -- with alef's
# step count and constant interpreter, so that the two share one std.
if(NOT COMMAND cme_find_package)
  set(CME_WITHOUT_PROVIDER ON)
  include(${CMAKE_CURRENT_LIST_DIR}/cmake/get_cme.cmake)
endif()
set(SPLICE_CONSTEXPR_STEPS ${ALEF_CONSTEXPR_STEPS} CACHE STRING
  "clang's -fconstexpr-steps for splice, as alef sets it")
cme_declare_port(NAME splice PROVIDES splice
  GITHUB_REPOSITORY j4niwzis/splice
  GIT_TAG 8a7755a335917daf3bbfc54ef8a3a160f8445d15
  LICENSE GPL-3.0-only
  TARGETS splice)
cme_options(splice "SPLICE_TESTS OFF")
cme_find_package(splice REQUIRED)
if(TARGET splice AND NOT DEFINED ALEF_SPLICE_FLAGGED)
  set(ALEF_SPLICE_FLAGGED ON)
  target_compile_options(splice PUBLIC
    $<$<AND:$<COMPILE_LANG_AND_ID:CXX,Clang>,$<VERSION_GREATER_EQUAL:$<CXX_COMPILER_VERSION>,23>>:-fno-experimental-new-constant-interpreter>)
endif()

# One module to a target. A change to a module compiles it again, and what
# imports it, and nothing else -- the tables least of all: each is a module of
# its own that imports nothing of the library but alef.ucd, the reader of the
# UCD, so a table is compiled again when its files or that reader change,
# and not when the code that looks things up in it does.
# ---- One library, and what of it is built ----
#
# alef is one target: libalef.a, with its modules in one file set. Modules
# are what separates it; archives did not need to as well. What is built is
# chosen by component, the way a consumer asks for it:
#
#   cme_find_package(alef COMPONENTS precis idna)
#
# A component brings what it imports -- precis brings normalization, casing,
# properties, identifier, bidi and their tables -- and a build that asks for
# nothing builds all of it, as does alef's own. The components are declared
# below as cme features, so that a consumer's COMPONENTS reach this file as
# CME_FEATURES_alef before anything is declared; ALEF_COMPONENTS says the
# same by hand.

# What each component needs besides itself: other components, then tables.
set(alef_components
  grapheme normalization word sentence identifier properties casing caseless
  confusable punycode precis idna line bidi emoji)
set(alef_needs_grapheme "")
set(alef_needs_normalization "")
set(alef_needs_word "")
set(alef_needs_sentence "")
set(alef_needs_identifier "")
set(alef_needs_properties grapheme)
set(alef_needs_casing word)
set(alef_needs_caseless casing normalization)
set(alef_needs_confusable normalization identifier)
set(alef_needs_punycode "")
set(alef_needs_precis normalization casing properties identifier bidi)
set(alef_needs_idna normalization casing properties punycode precis)
set(alef_needs_line "")
set(alef_needs_bidi "")
set(alef_needs_emoji "")
set(alef_tables_grapheme grapheme)
set(alef_tables_normalization normalization)
set(alef_tables_word word)
set(alef_tables_sentence sentence)
set(alef_tables_identifier identifier)
set(alef_tables_properties general_category east_asian_width script width)
set(alef_tables_casing casing normalization)
set(alef_tables_caseless nfkc_casefold)
set(alef_tables_confusable confusable)
set(alef_tables_punycode "")
set(alef_tables_precis general_category preparation)
set(alef_tables_idna general_category)
set(alef_tables_line line)
set(alef_tables_bidi bidi)
set(alef_tables_emoji emoji)

if(COMMAND cme_port_feature)
  foreach(component IN LISTS alef_components)
    cme_port_feature(alef ${component} SUMMARY "alef.${component} and what it imports")
  endforeach()
endif()

set(ALEF_COMPONENTS "" CACHE STRING
  "The components of alef to build, as a list; empty for all of them")
set(alef_asked ${ALEF_COMPONENTS})
if(DEFINED CME_FEATURES_alef)
  list(APPEND alef_asked ${CME_FEATURES_alef})
endif()
list(REMOVE_DUPLICATES alef_asked)
foreach(component IN LISTS alef_asked)
  if(NOT component IN_LIST alef_components)
    message(FATAL_ERROR
      "alef has no component ${component}; it has ${alef_components}")
  endif()
endforeach()
if(NOT alef_asked)
  set(alef_asked ${alef_components})
endif()

# What was asked for, and everything it needs.
set(alef_built "")
set(alef_pending ${alef_asked})
while(alef_pending)
  list(POP_FRONT alef_pending component)
  if(component IN_LIST alef_built)
    continue()
  endif()
  list(APPEND alef_built ${component})
  list(APPEND alef_pending ${alef_needs_${component}})
endwhile()
set(alef_tables "")
foreach(component IN LISTS alef_built)
  list(APPEND alef_tables ${alef_tables_${component}})
endforeach()
list(REMOVE_DUPLICATES alef_tables)

set(alef_files src/ucd.cc src/utf.cc)
foreach(table IN LISTS alef_tables)
  list(APPEND alef_files src/tables/${table}.cc)
endforeach()
foreach(component IN LISTS alef_built)
  list(APPEND alef_files src/${component}.cc)
endforeach()
# The module that imports all of it, where all of it is there.
list(LENGTH alef_built alef_count)
list(LENGTH alef_components alef_total)
if(alef_count EQUAL alef_total)
  list(APPEND alef_files src/alef.cc)
endif()
list(JOIN alef_built ", " alef_said)
message(STATUS "alef: ${alef_said}")

add_library(alef)
target_sources(alef PUBLIC FILE_SET CXX_MODULES BASE_DIRS src FILES ${alef_files})
target_compile_features(alef PUBLIC cxx_std_26)
target_compile_options(alef PUBLIC
  $<$<COMPILE_LANG_AND_ID:CXX,Clang,AppleClang>:-fconstexpr-steps=${ALEF_CONSTEXPR_STEPS}>
  $<$<AND:$<COMPILE_LANG_AND_ID:CXX,Clang>,$<VERSION_GREATER_EQUAL:$<CXX_COMPILER_VERSION>,23>>:-fno-experimental-new-constant-interpreter>)
target_link_libraries(alef PUBLIC splice)

# The names the targets used to have, each the one library now: what linked
# alef::precis links alef.
add_library(alef::alef ALIAS alef)
foreach(name IN ITEMS utf ucd ${alef_built})
  add_library(alef::${name} ALIAS alef)
  add_library(alef_${name} ALIAS alef)
endforeach()
foreach(table IN LISTS alef_tables)
  add_library(alef_tables_${table} ALIAS alef)
endforeach()

option(ALEF_TESTS "Build the tests" ${PROJECT_IS_TOP_LEVEL})
if(ALEF_TESTS)
  enable_testing()
  add_subdirectory(test)
endif()

# ---- What this library is, said by the library; and installing it ----
#
# A consumer writes find_package(alef) and this says the rest: where it
# comes from, what it needs, and what it produces -- installed beside the
# library by cmake-everywhere, under share/cmake-everywhere/ports, so that a
# prefix that has alef describes it too.
if(COMMAND cme_declare_port)
  cme_declare_port(
    NAME alef
    PROVIDES alef
    VERSION ${PROJECT_VERSION}
    GITHUB_REPOSITORY j4niwzis/alef
    GIT_TAG main
    LICENSE AGPL-3.0-only
    DEPENDS splice
    TARGETS alef::alef)
endif()
# The library, its module interface units -- a consumer compiles its own
# binary interfaces from them -- and a package: find_package(alef) on a
# machine that has it installed finds it there, by the names it has here
# too, and what it needs again from the ports it came from.
option(ALEF_INSTALL "Install alef and its CMake package" ${PROJECT_IS_TOP_LEVEL})
if(ALEF_INSTALL)
  set(alef_aliases "")
  foreach(name IN ITEMS utf ucd ${alef_built})
    list(APPEND alef_aliases alef::${name} alef_${name})
  endforeach()
  foreach(table IN LISTS alef_tables)
    list(APPEND alef_aliases alef_tables_${table})
  endforeach()
  cme_install_package(alef ALIASES ${alef_aliases})
endif()
