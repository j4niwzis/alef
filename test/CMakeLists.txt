add_executable(alef-utf-test utf.cc)
target_link_libraries(alef-utf-test PRIVATE alef)
add_test(NAME utf COMMAND alef-utf-test)

# Against the test file of the same version as the data the tables are read
# from, which is the only thing that says whether the rules are the rules.
add_executable(alef-grapheme-test grapheme.cc)
target_link_libraries(alef-grapheme-test PRIVATE alef)
add_test(NAME grapheme
  COMMAND alef-grapheme-test
    ${PROJECT_SOURCE_DIR}/ucd/auxiliary/GraphemeBreakTest.txt)
