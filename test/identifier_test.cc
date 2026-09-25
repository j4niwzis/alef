// Identifiers, by UAX #31: XID_Start and XID_Continue of every code point
// against DerivedCoreProperties.txt, read on its own, and the default
// identifier syntax by example, in every UTF.
import std;
import alef.utf;
import alef.identifier;
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
constexpr char derived_core_bytes[] = {
#embed "../ucd/DerivedCoreProperties.txt"
};
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
constexpr std::string_view derived_core() {
  return {derived_core_bytes, sizeof derived_core_bytes};
}
#else
std::string_view derived_core() {
  return alef::test::ucd_file("DerivedCoreProperties.txt");
}
#endif

struct span {
  char32_t first;
  char32_t last;
};

constexpr char32_t hex(std::string_view digits) {
  char32_t value = 0;
  for (const char digit : digits)
    if (digit != ' ')
      value = value * 16 + static_cast<char32_t>(digit <= '9' ? digit - '0' : (digit | 0x20) - 'a' + 10);
  return value;
}

constexpr std::string_view trimmed(std::string_view text) {
  const std::size_t from = text.find_first_not_of(' ');
  if (from == std::string_view::npos)
    return {};
  return text.substr(from, text.find_last_not_of(' ') - from + 1);
}

// The ranges the file gives a property, sorted.
constexpr std::vector<span> spans_of(std::string_view name) {
  const std::string_view file = derived_core();
  std::vector<span> out;
  for (std::size_t start = 0; start < file.size();) {
    std::size_t end = file.find('\n', start);
    if (end == std::string_view::npos)
      end = file.size();
    const std::string_view line = file.substr(start, end - start);
    start = end + 1;
    const std::string_view data = line.substr(0, line.find('#'));
    const std::size_t semicolon = data.find(';');
    if (semicolon == std::string_view::npos || trimmed(data.substr(semicolon + 1)) != name)
      continue;
    const std::string_view range = trimmed(data.substr(0, semicolon));
    const std::size_t dots = range.find("..");
    if (dots == std::string_view::npos)
      out.push_back({hex(range), hex(range)});
    else
      out.push_back({hex(range.substr(0, dots)), hex(range.substr(dots + 2))});
  }
  std::ranges::sort(out, {}, &span::first);
  return out;
}

constexpr bool within(const std::vector<span>& all, char32_t code_point) {
  const auto after = std::ranges::upper_bound(all, code_point, {}, &span::first);
  return after != all.begin() && code_point <= std::prev(after)->last;
}

}  // namespace

// Every code point when run; while compiled, where each range begins and
// ends, and the code points on either side.
CONSTEXPR_TEST(Identifiers, OfEveryCodePoint) {
  const std::vector<span> starts = spans_of("XID_Start");
  const std::vector<span> continues = spans_of("XID_Continue");
  CONSTEXPR_EXPECT_FALSE(starts.empty() || continues.empty());
  std::size_t wrong = 0;
  const auto check = [&](char32_t code_point) {
    wrong += alef::is_xid_start(code_point) != within(starts, code_point) ||
             alef::is_xid_continue(code_point) != within(continues, code_point);
  };
  if consteval {
    for (const std::vector<span>* all : {&starts, &continues})
      for (const span& one : *all) {
        check(one.first);
        check(one.last);
        if (one.first > 0)
          check(one.first - 1);
        check(one.last + 1);
      }
  } else {
    for (char32_t code_point = 0; code_point <= 0x10FFFF; ++code_point)
      check(code_point);
  }
  CONSTEXPR_EXPECT_EQ(wrong, std::size_t{0});
}

CONSTEXPR_TEST(Identifiers, ByExample) {
  CONSTEXPR_EXPECT_TRUE(alef::is_identifier(u8"foo_bar1"));
  CONSTEXPR_EXPECT_TRUE(alef::is_identifier(u8"héllo"));
  CONSTEXPR_EXPECT_TRUE(alef::is_identifier(u8"πλ"));
  CONSTEXPR_EXPECT_TRUE(alef::is_identifier(u8"變數"));
  CONSTEXPR_EXPECT_TRUE(alef::is_identifier(u8"á"));      // a mark goes on one
  CONSTEXPR_EXPECT_FALSE(alef::is_identifier(u8"1foo"));        // a digit begins none
  CONSTEXPR_EXPECT_FALSE(alef::is_identifier(u8"_x"));          // nor does '_', by default
  CONSTEXPR_EXPECT_FALSE(alef::is_identifier(u8"a-b"));
  CONSTEXPR_EXPECT_FALSE(alef::is_identifier(u8""));
  CONSTEXPR_EXPECT_FALSE(alef::is_identifier(std::u8string_view(u8"a\xFF", 2)));  // ill-formed
  // In UTF-16 and UTF-32; an Arabic-Indic digit goes on one.
  CONSTEXPR_EXPECT_TRUE(alef::is_identifier(u"Größe"));
  CONSTEXPR_EXPECT_TRUE(alef::is_identifier(U"x٣"));
  // Where an identifier ends, as a lexer asks.
  constexpr std::u8string_view text = u8"abc+1";
  CONSTEXPR_EXPECT_TRUE(alef::identifier_end(text.begin(), text.end()) == text.begin() + 3);
  CONSTEXPR_EXPECT_TRUE(alef::identifier_end(text.begin() + 3, text.end()) == text.begin() + 3);
}
