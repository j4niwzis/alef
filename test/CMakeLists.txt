# The tests: googletest's, as a module.
#
# googletest is j4niwzis/googletest-modules, the fork that builds it as one:
# a test here imports std, and a header that includes the standard library
# textually would give a translation unit two views of it. It comes from
# cmake-everywhere, pinned to a commit, and never from the machine -- a
# googletest installed anywhere is the one that is a header.
#
# cmake-everywhere is asked without being installed as the dependency
# provider: which provider a build has is for whoever builds alef, and only
# the tests need anything fetched.
if(NOT COMMAND cme_find_package)
  set(CME_WITHOUT_PROVIDER ON)
  include(${PROJECT_SOURCE_DIR}/cmake/get_cme.cmake)
endif()
cme_declare_port(NAME googletest-modules PROVIDES GTest
  GITHUB_REPOSITORY j4niwzis/googletest-modules
  GIT_TAG 706920f1d230f0316d271466d8885735be3e300e
  LICENSE BSD-3-Clause
  SYSTEM NEVER
  TARGETS gtest_main)
cme_options(googletest-modules
  "BUILD_GMOCK OFF" "INSTALL_GTEST OFF" "gtest_build_modules ON"
  "gtest_build_tests OFF" "gtest_build_samples OFF")
cme_find_package(GTest REQUIRED)
# A copy from a prefix names it GTest::gtest_main, and one built here
# gtest_main.
if(TARGET GTest::gtest_main)
  set(alef_gtest GTest::gtest_main)
else()
  set(alef_gtest gtest_main)
endif()

# Every CONSTEXPR_TEST run by the compiler as well, as a constant expression.
# CI turns it on. Off by default, because it is the slow half: what the
# program runs in microseconds the compiler runs in seconds.
option(ALEF_CONSTEXPR_TESTS
  "Run every CONSTEXPR_TEST while it is compiled as well" OFF)

get_property(alef_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)

foreach(one IN ITEMS utf grapheme properties normalization)
  add_executable(alef-${one}-test ${one}_test.cc)
  target_link_libraries(alef-${one}-test PRIVATE alef ${alef_gtest})
  add_test(NAME ${one} COMMAND alef-${one}-test)
  # Where a test that only runs reads the files of the UCD from.
  target_compile_definitions(alef-${one}-test PRIVATE
    "ALEF_UCD_DIR=\"${PROJECT_SOURCE_DIR}/ucd\"")
  if(NOT ALEF_CONSTEXPR_TESTS)
    continue()
  endif()

  # And for each test in the file, a target that compiles it alone with its
  # body as a static_assert: what a test that is not a constant expression
  # builds, to show what the compiler says of it. The tests are found by
  # reading the file, which is read again whenever it changes.
  target_compile_definitions(alef-${one}-test PRIVATE
    ALEF_CONSTEXPR_TESTS
    "ALEF_TEST_CMAKE=\"${CMAKE_COMMAND}\""
    "ALEF_TEST_BUILD_DIR=\"${CMAKE_BINARY_DIR}\""
    $<${alef_multi_config}:ALEF_TEST_CONFIG="$<CONFIG>">)
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${one}_test.cc)
  file(STRINGS ${one}_test.cc declared REGEX "^CONSTEXPR_TEST\\(")
  foreach(line IN LISTS declared)
    if(line MATCHES "^CONSTEXPR_TEST\\(([A-Za-z0-9_]+), *([A-Za-z0-9_]+)\\)")
      set(test ${CMAKE_MATCH_1}.${CMAKE_MATCH_2})
      add_library(alef-constexpr-${test} OBJECT EXCLUDE_FROM_ALL ${one}_test.cc)
      target_link_libraries(alef-constexpr-${test} PRIVATE alef ${alef_gtest})
      target_compile_definitions(alef-constexpr-${test} PRIVATE
        ALEF_CONSTEXPR_TESTS "ALEF_CONSTEXPR_DIAGNOSE=\"${test}\"")
    endif()
  endforeach()
endforeach()
