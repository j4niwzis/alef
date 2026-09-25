// Normalization, against NormalizationTest.txt of the version the tables are
// read from: every line of it in all four forms, with the quick check asked
// of each too; and every code point the file does not list, which is its own
// normalization in each form.
import std;
import alef;
import gtest;

#include "gtest/gtest-macros.h"

#include "constexpr_test.h"
#include "data.h"
#include "once.h"

namespace {

#if defined(ALEF_CONSTEXPR_TESTS)
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc23-extensions"
#endif
constexpr char normalization_test_bytes[] = {
#embed "../ucd/NormalizationTest.txt"
};
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
constexpr std::string_view normalization_test() {
  return {normalization_test_bytes, sizeof normalization_test_bytes};
}
#else
std::string_view normalization_test() {
  return alef::test::ucd_file("NormalizationTest.txt");
}
#endif

constexpr char32_t hex(std::string_view digits) {
  char32_t value = 0;
  for (const char digit : digits)
    value = value * 16 + static_cast<char32_t>(
        digit <= '9' ? digit - '0' : (digit | 0x20) - 'a' + 10);
  return value;
}

// One line: its five columns of code points, and the line as written.
struct row {
  std::array<std::u32string, 5> columns;
  std::string_view line;
};

// The lines of part `number` of the file, every `every`-th of them.
constexpr std::vector<row> part(int number, std::size_t every) {
  const std::string_view file = normalization_test();
  const std::string marker = "@Part" + std::string(1, static_cast<char>('0' + number));
  std::vector<row> out;
  std::size_t at = file.find(marker);
  if (at == std::string_view::npos)
    return out;
  at = file.find('\n', at) + 1;
  std::size_t index = 0;
  while (at < file.size() && file[at] != '@') {
    std::size_t end = file.find('\n', at);
    if (end == std::string_view::npos)
      end = file.size();
    const std::string_view line = file.substr(at, end - at);
    at = end + 1;
    if (line.empty() || line.front() == '#' || index++ % every != 0)
      continue;
    row one{{}, line};
    std::size_t from = 0;
    for (std::u32string& column : one.columns) {
      const std::size_t semicolon = line.find(';', from);
      const std::string_view field = line.substr(from, semicolon - from);
      for (std::size_t start = field.find_first_not_of(' ');
           start != std::string_view::npos;) {
        std::size_t stop = field.find(' ', start);
        if (stop == std::string_view::npos)
          stop = field.size();
        column.push_back(hex(field.substr(start, stop - start)));
        start = field.find_first_not_of(' ', stop);
      }
      from = semicolon + 1;
    }
    out.push_back(std::move(one));
  }
  return out;
}

template <class Form>
constexpr std::u32string in(const std::u32string& text, Form form) {
  return text | form | std::ranges::to<std::u32string>();
}

// What the file says of a line: c2 == NFC(c1) == NFC(c2) == NFC(c3) and
// c4 == NFC(c4) == NFC(c5); c3 == NFD(c1) == NFD(c2) == NFD(c3) and
// c5 == NFD(c4) == NFD(c5); c4 == NFKC of all five; c5 == NFKD of all five.
// And the quick check agrees with normalizing, for each column in each form.
constexpr bool holds(const row& one) {
  const auto& c = one.columns;
  for (const int at : {0, 1, 2})
    if (in(c[at], alef::as_nfc) != c[1] || in(c[at], alef::as_nfd) != c[2])
      return false;
  for (const int at : {3, 4})
    if (in(c[at], alef::as_nfc) != c[3] || in(c[at], alef::as_nfd) != c[4])
      return false;
  for (const auto& text : c)
    if (in(text, alef::as_nfkc) != c[3] || in(text, alef::as_nfkd) != c[4])
      return false;
  for (const auto& text : c)
    if (alef::is_nfc(text) != (in(text, alef::as_nfc) == text) ||
        alef::is_nfd(text) != (in(text, alef::as_nfd) == text) ||
        alef::is_nfkc(text) != (in(text, alef::as_nfkc) == text) ||
        alef::is_nfkd(text) != (in(text, alef::as_nfkd) == text))
      return false;
  return true;
}

// The lines of a part that do not hold, as written.
constexpr std::string wrong_in(int number, std::size_t every) {
  const std::vector<row> rows = part(number, every);
  std::string wrong = rows.empty() ? "no lines read\n" : "";
  for (const row& one : rows)
    if (!holds(one) && wrong.size() < 2000)
      wrong += std::string(one.line) + "\n";
  return wrong;
}

constexpr std::string hex_of(char32_t code_point) {
  constexpr std::string_view digits = "0123456789ABCDEF";
  std::string out;
  for (int shift = 20; shift >= 0; shift -= 4)
    out += digits[(code_point >> shift) & 0xF];
  return out;
}

}  // namespace

CONSTEXPR_TEST(NormalizationTest, SpecificCases) {
  CONSTEXPR_EXPECT_EQ(wrong_in(0, 1), "");
}

// Every character with a decomposition; while compiled, every 50th.
CONSTEXPR_TEST(NormalizationTest, CharacterByCharacter) {
  std::size_t every = 1;
  if consteval {
    every = 50;
  }
  CONSTEXPR_EXPECT_EQ(wrong_in(1, every), "");
}

// Combining marks in every order; while compiled, every 5th.
CONSTEXPR_TEST(NormalizationTest, CanonicalOrder) {
  std::size_t every = 1;
  if consteval {
    every = 5;
  }
  CONSTEXPR_EXPECT_EQ(wrong_in(2, every), "");
}

CONSTEXPR_TEST(NormalizationTest, PublicReviewIssue29) {
  CONSTEXPR_EXPECT_EQ(wrong_in(3, 1), "");
}

// A code point Part 1 does not list is its own normalization in each form:
// every one when run; while compiled, every 4093rd.
CONSTEXPR_TEST(NormalizationTest, EveryCodePointNotListedIsItself) {
  std::vector<char32_t> listed;
  for (const row& one : part(1, 1))
    if (one.columns[0].size() == 1)
      listed.push_back(one.columns[0][0]);
  std::ranges::sort(listed);
  CONSTEXPR_EXPECT_FALSE(listed.empty());
  char32_t step = 1;
  if consteval {
    step = 4093;
  }
  std::string wrong;
  for (char32_t code_point = 0; code_point <= 0x10FFFF; code_point += step) {
    if ((code_point >= 0xD800 && code_point <= 0xDFFF) ||
        std::ranges::binary_search(listed, code_point))
      continue;
    const std::u32string text(1, code_point);
    if ((in(text, alef::as_nfc) != text || in(text, alef::as_nfd) != text ||
         in(text, alef::as_nfkc) != text || in(text, alef::as_nfkd) != text) &&
        wrong.size() < 200)
      wrong += hex_of(code_point) + " ";
  }
  CONSTEXPR_EXPECT_EQ(wrong, "");
}

CONSTEXPR_TEST(Normalization, AnyUtfReadOnceAndBack) {
  const std::u8string accented = u8"e\U00000301";
  CONSTEXPR_EXPECT_TRUE((once(std::u8string_view(accented)) | alef::as_nfc |
                         std::ranges::to<std::u32string>()) == U"\U000000E9");
  CONSTEXPR_EXPECT_TRUE((u8"\U000000E9" | alef::as_nfd | alef::as_utf8 |
                         std::ranges::to<std::u8string>()) == u8"e\U00000301");
  CONSTEXPR_EXPECT_TRUE((u"\U0000FB01" | alef::as_nfkc |
                         std::ranges::to<std::u32string>()) == U"fi");
  CONSTEXPR_EXPECT_TRUE((U"\U0000D55C" | alef::as_nfd |
                         std::ranges::to<std::u32string>()) ==
                        U"\U00001112\U00001161\U000011AB");
  CONSTEXPR_EXPECT_TRUE((U"\U00001112\U00001161\U000011AB" | alef::as_nfc |
                         std::ranges::to<std::u32string>()) == U"\U0000D55C");
  // Reordered, then composed past the cedilla, whose class is lower.
  CONSTEXPR_EXPECT_TRUE((U"a\U00000301\U00000327" | alef::as_nfc |
                         std::ranges::to<std::u32string>()) ==
                        U"\U000000E1\U00000327");
  CONSTEXPR_EXPECT_FALSE(alef::is_nfc("\xFF"));
}

CONSTEXPR_TEST(Normalization, CanonicalCombiningClass) {
  CONSTEXPR_EXPECT_EQ(alef::canonical_combining_class(U'a'), 0);
  CONSTEXPR_EXPECT_EQ(alef::canonical_combining_class(U'\U00000301'), 230);
  CONSTEXPR_EXPECT_EQ(alef::canonical_combining_class(U'\U00000327'), 202);
  CONSTEXPR_EXPECT_EQ(alef::canonical_combining_class(U'\U0000094D'), 9);
}
