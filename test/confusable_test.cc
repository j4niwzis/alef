// Confusables, by UTS #39: every mapping of confusables.txt, read on its
// own, makes a source and its prototype confusable, and by example.
import std;
import alef.utf;
import alef.identifier;
import alef.confusable;
import gtest;

#include "gtest/gtest-macros.h"

#include "constexpr_test.h"
#include "data.h"

namespace {

#if defined(ALEF_CONSTEXPR_TESTS)
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc23-extensions"
#endif
constexpr char confusables_bytes[] = {
#embed "../ucd/security/confusables.txt"
};
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
constexpr std::string_view confusables() {
  return {confusables_bytes, sizeof confusables_bytes};
}
#else
std::string_view confusables() {
  return alef::test::ucd_file("security/confusables.txt");
}
#endif

constexpr char32_t hex(std::string_view digits) {
  char32_t value = 0;
  for (const char digit : digits)
    if (digit != ' ' && digit != '\t')
      value = value * 16 + static_cast<char32_t>(digit <= '9' ? digit - '0' : (digit | 0x20) - 'a' + 10);
  return value;
}

// Every mapping: the source, and its prototype.
constexpr std::vector<std::pair<std::u32string, std::u32string>> mappings() {
  std::string_view file = confusables();
  if (file.starts_with("\xEF\xBB\xBF"))
    file.remove_prefix(3);
  std::vector<std::pair<std::u32string, std::u32string>> out;
  for (std::size_t start = 0; start < file.size();) {
    std::size_t end = file.find('\n', start);
    if (end == std::string_view::npos)
      end = file.size();
    std::string_view line = file.substr(start, end - start);
    start = end + 1;
    line = line.substr(0, line.find('#'));
    const std::size_t one = line.find(';');
    const std::size_t two = one == std::string_view::npos ? one : line.find(';', one + 1);
    if (two == std::string_view::npos)
      continue;
    std::u32string target;
    std::string_view rest = line.substr(one + 1, two - one - 1);
    for (std::size_t at = 0; at < rest.size();) {
      while (at < rest.size() && (rest[at] == ' ' || rest[at] == '\t'))
        ++at;
      std::size_t stop = at;
      while (stop < rest.size() && rest[stop] != ' ' && rest[stop] != '\t')
        ++stop;
      if (stop > at)
        target.push_back(hex(rest.substr(at, stop - at)));
      at = stop;
    }
    out.emplace_back(std::u32string(1, hex(line.substr(0, one))), target);
  }
  return out;
}

}  // namespace

// Every line when run; while compiled, every 97th.
CONSTEXPR_TEST(Confusables, EveryMappingIsConfusable) {
  std::size_t every = 1;
  if consteval {
    every = 97;
  }
  const auto all = mappings();
  CONSTEXPR_EXPECT_TRUE(all.size() > 6000);
  std::size_t wrong = 0;
  for (std::size_t at = 0; at < all.size(); at += every)
    wrong += !alef::confusable(all[at].first, all[at].second);
  CONSTEXPR_EXPECT_EQ(wrong, std::size_t{0});
}

CONSTEXPR_TEST(Confusables, ByExample) {
  CONSTEXPR_EXPECT_TRUE(alef::confusable(u8"paypal", u8"pаypal"));  // Cyrillic a
  CONSTEXPR_EXPECT_TRUE(alef::confusable(u8"scope", u8"ѕсоре"));
  CONSTEXPR_EXPECT_TRUE(alef::confusable(u8"rn", u8"m"));
  CONSTEXPR_EXPECT_TRUE(alef::confusable(u8"l", u8"1"));
  CONSTEXPR_EXPECT_TRUE(alef::confusable(u8"a­", u8"a"));  // a soft hyphen is ignorable
  CONSTEXPR_EXPECT_FALSE(alef::confusable(u8"abc", u8"abd"));
  CONSTEXPR_EXPECT_TRUE(alef::confusable(u"paypal", U"pаypal"));  // across UTFs
  CONSTEXPR_EXPECT_TRUE(alef::is_default_ignorable(U'­'));
  CONSTEXPR_EXPECT_FALSE(alef::is_default_ignorable(U'a'));
}
