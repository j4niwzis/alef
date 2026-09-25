// The files of the UCD a test reads.
//
// Where the tests are run while they are compiled as well -- in CI, with
// ALEF_CONSTEXPR_TESTS -- a test embeds what it reads, since a constant
// expression opens no files. Where they only run, it reads the file from
// ucd/ when it runs, and the compiler never sees it.
//
// Included by a file that imports std.
#pragma once

#if !defined(ALEF_CONSTEXPR_TESTS)
namespace alef::test {

// ucd/<path>, read once and kept.
inline std::string_view ucd_file(std::string_view path) {
  static std::map<std::string, std::string, std::less<>> read;
  if (const auto found = read.find(path); found != read.end())
    return found->second;
  std::ifstream in(std::string(ALEF_UCD_DIR) + "/" + std::string(path),
                   std::ios::binary);
  std::ostringstream all;
  all << in.rdbuf();
  return read.emplace(std::string(path), std::move(all).str()).first->second;
}

}  // namespace alef::test
#endif
