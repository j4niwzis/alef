// Line breaking, by the rules of UAX #14: against the test file the Unicode
// Consortium publishes beside the data, in every UTF, and by example.
import std;
import alef.utf;
import alef.line;
import gtest;

#include "gtest/gtest-macros.h"

#include "constexpr_test.h"
#include "data.h"

namespace {

// LineBreakTest.txt, of the version the tables are read from.
#if defined(ALEF_CONSTEXPR_TESTS)
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc23-extensions"
#endif
constexpr char line_break_test_bytes[] = {
#embed "../ucd/auxiliary/LineBreakTest.txt"
};
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
constexpr std::string_view line_break_test() {
  return {line_break_test_bytes, sizeof line_break_test_bytes};
}
#else
std::string_view line_break_test() {
  return alef::test::ucd_file("auxiliary/LineBreakTest.txt");
}
#endif

constexpr char32_t hex_value(std::string_view digits) {
  char32_t value = 0;
  for (const char digit : digits)
    value = value * 16 + static_cast<char32_t>(
        digit <= '9' ? digit - '0' : (digit | 0x20) - 'a' + 10);
  return value;
}

// One line: the text as UTF-8, and where it may be broken in it.
struct example {
  std::string_view line;
  std::u8string text;
  std::vector<std::size_t> breaks;
};

// A line is code points in hex, with U+00F7 where a break is allowed and
// U+00D7 where it is not; every `every`-th of them.
constexpr std::vector<example> examples(std::size_t every) {
  constexpr std::string_view allowed = "\xC3\xB7";
  constexpr std::string_view not_allowed = "\xC3\x97";
  const std::string_view file = line_break_test();
  std::vector<example> out;
  std::size_t index = 0;
  for (std::size_t start = 0; start < file.size();) {
    std::size_t end = file.find('\n', start);
    if (end == std::string_view::npos)
      end = file.size();
    const std::string_view whole = file.substr(start, end - start);
    start = end + 1;
    if (whole.empty() || whole.front() == '#' || index++ % every != 0)
      continue;
    const std::string_view data = whole.substr(0, whole.find('#'));
    example one{data, {}, {}};
    for (std::size_t at = data.find_first_not_of(" \t");
         at != std::string_view::npos; at = data.find_first_not_of(" \t", at)) {
      const std::size_t stop = std::min(data.find_first_of(" \t", at), data.size());
      const std::string_view token = data.substr(at, stop - at);
      at = stop;
      if (token == allowed) {
        one.breaks.push_back(one.text.size());
      } else if (token != not_allowed) {
        const char32_t code_point = hex_value(token);
        for (const char8_t unit : std::u32string_view(&code_point, 1) | alef::as_utf8)
          one.text.push_back(unit);
      }
    }
    if (!one.breaks.empty())
      out.push_back(std::move(one));
  }
  return out;
}

// Where the pieces of some text end, in code units: never at its start (LB2).
template <class Unit>
constexpr std::vector<std::size_t> breaks(std::basic_string_view<Unit> text) {
  std::vector<std::size_t> found;
  for (const auto piece : text | alef::line_breaks)
    found.push_back(static_cast<std::size_t>(piece.text.end() - text.begin()));
  return found;
}

// How many code points each piece is.
template <class Unit>
constexpr std::vector<std::ptrdiff_t> sizes(std::basic_string_view<Unit> text) {
  std::vector<std::ptrdiff_t> out;
  for (const auto piece : text | alef::line_breaks)
    out.push_back(std::ranges::distance(piece.text | alef::as_utf32));
  return out;
}

template <class Find>
constexpr std::string lines_where(std::size_t every, Find find) {
  const std::vector<example> all = examples(every);
  std::string wrong = all.empty() ? "no lines read from the file\n" : "";
  for (const example& one : all)
    if (!find(one) && wrong.size() < 4000)
      wrong += std::string(one.line) + "\n";
  return wrong;
}

template <class Text>
constexpr std::vector<std::pair<std::u8string, bool>> pieces(const Text& text) {
  std::vector<std::pair<std::u8string, bool>> out;
  for (const auto piece : std::u8string_view(text) | alef::line_breaks)
    out.emplace_back(std::u8string(piece.text.begin(), piece.text.end()), piece.mandatory);
  return out;
}

}  // namespace

// Every line when run; while compiled, every 25th.
CONSTEXPR_TEST(LineBreakTest, EveryLine) {
  std::size_t every = 1;
  if consteval {
    every = 25;
  }
  CONSTEXPR_EXPECT_EQ(lines_where(every, [](const example& one) {
                        return breaks(std::u8string_view(one.text)) == one.breaks;
                      }),
                      "");
}

CONSTEXPR_TEST(LineBreakTest, EveryLineInUtf16AndUtf32) {
  std::size_t every = 1;
  if consteval {
    every = 25;
  }
  CONSTEXPR_EXPECT_EQ(
      lines_where(every,
                  [](const example& one) {
                    const auto in_utf16 = one.text | alef::as_utf16 | std::ranges::to<std::u16string>();
                    const auto in_utf32 = one.text | alef::as_utf32 | std::ranges::to<std::u32string>();
                    const auto expected = sizes(std::u8string_view(one.text));
                    return sizes(std::u16string_view(in_utf16)) == expected &&
                           sizes(std::u32string_view(in_utf32)) == expected;
                  }),
      "");
}

CONSTEXPR_TEST(Lines, ByExample) {
  using pieces_t = std::vector<std::pair<std::u8string, bool>>;
  // A line feed is a break the text asks for; after spaces one is allowed.
  CONSTEXPR_EXPECT_TRUE(pieces(u8"one two\nthree") ==
                        (pieces_t{{u8"one ", false}, {u8"two\n", true}, {u8"three", false}}));
  // Numbers, e.g., parentheses and quotes keep together.
  CONSTEXPR_EXPECT_TRUE(pieces(u8"e.g. $3,456.78 (and) \"so\"") ==
                        (pieces_t{{u8"e.g. ", false}, {u8"$3,456.78 ", false},
                                  {u8"(and) ", false}, {u8"\"so\"", false}}));
  // No break at a no-break space; after a hyphen, one.
  CONSTEXPR_EXPECT_TRUE(pieces(u8"a\U000000A0b well-known") ==
                        (pieces_t{{u8"a\U000000A0b ", false}, {u8"well-", false}, {u8"known", false}}));
  // An emoji with its modifier, and ideographs one by one.
  CONSTEXPR_EXPECT_TRUE(pieces(u8"\U0001F44D\U0001F3FD\U00004E00\U00004E8C") ==
                        (pieces_t{{u8"\U0001F44D\U0001F3FD", false}, {u8"\U00004E00", false},
                                  {u8"\U00004E8C", false}}));
  CONSTEXPR_EXPECT_TRUE(alef::line_break_of(U' ') == alef::line_break::sp);
  CONSTEXPR_EXPECT_TRUE(alef::line_break_of(U'\n') == alef::line_break::lf);
}
