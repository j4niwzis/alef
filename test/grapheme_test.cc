// Grapheme clusters, by the rules of UAX #29: checked against the test file
// the Unicode Consortium publishes beside the data, and against themselves --
// the same boundaries found forwards, backwards, at every byte, through
// std::views::reverse and in every UTF.
import std;
import alef.utf;
import alef.grapheme;
import gtest;

#include "gtest/gtest-macros.h"

#include "constexpr_test.h"
#include "data.h"
#include "once.h"

namespace {

// GraphemeBreakTest.txt, of the version the tables are read from.
#if defined(ALEF_CONSTEXPR_TESTS)
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc23-extensions"
#endif
constexpr char grapheme_break_test_bytes[] = {
#embed "../ucd/auxiliary/GraphemeBreakTest.txt"
};
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
constexpr std::string_view grapheme_break_test() {
  return {grapheme_break_test_bytes, sizeof grapheme_break_test_bytes};
}
#else
std::string_view grapheme_break_test() {
  return alef::test::ucd_file("auxiliary/GraphemeBreakTest.txt");
}
#endif

// One line of it: the text as UTF-8, and where the boundaries are in it.
struct example {
  std::string_view line;
  std::u8string text;
  std::vector<std::size_t> boundaries;
};

constexpr char32_t hex_value(std::string_view digits) {
  char32_t value = 0;
  for (const char digit : digits)
    value = value * 16 + static_cast<char32_t>(
        digit <= '9' ? digit - '0' : (digit | 0x20) - 'a' + 10);
  return value;
}

// A line is code points in hex, with U+00F7 where there is a boundary and
// U+00D7 where there is none, and a comment after '#'.
constexpr std::vector<example> examples() {
  constexpr std::string_view boundary = "\xC3\xB7";
  constexpr std::string_view none = "\xC3\x97";
  const std::string_view file = grapheme_break_test();
  std::vector<example> out;
  for (std::size_t start = 0; start < file.size();) {
    std::size_t end = file.find('\n', start);
    if (end == std::string_view::npos)
      end = file.size();
    const std::string_view data =
        file.substr(start, end - start).substr(0, file.substr(start, end - start).find('#'));
    start = end + 1;
    example one{data, {}, {}};
    for (std::size_t at = data.find_first_not_of(" \t");
         at != std::string_view::npos; at = data.find_first_not_of(" \t", at)) {
      const std::size_t stop = std::min(data.find_first_of(" \t", at), data.size());
      const std::string_view token = data.substr(at, stop - at);
      at = stop;
      if (token == boundary) {
        one.boundaries.push_back(one.text.size());
      } else if (token != none) {
        const char32_t code_point = hex_value(token);
        for (const char8_t unit : std::u32string_view(&code_point, 1) | alef::as_utf8)
          one.text.push_back(unit);
      }
    }
    if (!one.boundaries.empty())
      out.push_back(std::move(one));
  }
  return out;
}

// The boundaries of some text, found each way there is.
template <class Unit>
constexpr std::vector<std::size_t> boundaries(std::basic_string_view<Unit> text) {
  std::vector<std::size_t> found{0};
  for (const auto cluster : text | alef::graphemes)
    found.push_back(static_cast<std::size_t>(cluster.end() - text.begin()));
  return found;
}

constexpr std::vector<std::size_t> forwards(std::u8string_view text) {
  return boundaries(text);
}

// The boundaries `adaptor` finds in `source` -- `text`, or a way of reading
// it -- counted from what each cluster gives out; none, if what they gave out
// is not the text.
template <class Unit, class Source, class Adaptor>
constexpr std::vector<std::size_t> counted(std::basic_string_view<Unit> text,
                                           Source source, Adaptor adaptor) {
  std::vector<std::size_t> found{0};
  std::basic_string<Unit> again;
  for (auto&& cluster : std::move(source) | adaptor) {
    for (const Unit unit : cluster)
      again.push_back(unit);
    found.push_back(again.size());
  }
  if (again != text)
    found.clear();
  return found;
}

// Every way there is of reading text once finds what reading it more than
// once does: lazily; owned, by default and when asked; and owned in no room
// at all, on the heap, or in almost none.
template <class Unit>
constexpr bool the_same_read_once(std::basic_string_view<Unit> text) {
  const std::vector<std::size_t> expected = boundaries(text);
  return counted(text, once(text), alef::lazy_graphemes) == expected &&
         counted(text, once(text), alef::graphemes) == expected &&
         counted(text, text, alef::graphemes(alef::owning<>)) == expected &&
         counted(text, once(text), alef::graphemes(alef::owning<0>)) == expected &&
         counted(text, text, alef::graphemes(alef::owning<1>)) == expected &&
         counted(text, once(text), alef::graphemes(alef::owning_in_bytes<4>)) ==
             expected;
}

constexpr std::vector<std::size_t> backwards(std::u8string_view text) {
  std::vector<std::size_t> found{text.size()};
  for (auto at = text.end(); at != text.begin();) {
    at = alef::prev_grapheme_boundary(text.begin(), at);
    found.push_back(static_cast<std::size_t>(at - text.begin()));
  }
  std::ranges::reverse(found);
  return found;
}

constexpr std::vector<std::size_t> everywhere(std::u8string_view text) {
  std::vector<std::size_t> found;
  for (std::size_t at = 0; at <= text.size(); ++at)
    if (alef::is_grapheme_boundary(text.begin(), text.begin() + at, text.end()))
      found.push_back(at);
  return found;
}

constexpr std::vector<std::size_t> reversed(std::u8string_view text) {
  std::vector<std::size_t> found{text.size()};
  for (const auto cluster : text | alef::graphemes | std::views::reverse)
    found.push_back(static_cast<std::size_t>(cluster.begin() - text.begin()));
  std::ranges::reverse(found);
  return found;
}

// How many code points each cluster of some text is, in whatever UTF.
template <class Text>
constexpr std::vector<std::ptrdiff_t> sizes(const Text& text) {
  std::vector<std::ptrdiff_t> out;
  for (const auto cluster : text | alef::graphemes)
    out.push_back(std::ranges::distance(cluster | alef::as_utf32));
  return out;
}

constexpr bool same_in_utf16_and_utf32(std::u8string_view text) {
  const auto in_utf16 = text | alef::as_utf16 | std::ranges::to<std::u16string>();
  const auto in_utf32 = text | alef::as_utf32 | std::ranges::to<std::u32string>();
  return sizes(in_utf16) == sizes(text) && sizes(in_utf32) == sizes(text);
}

// The lines of the file on which `find` does not find the boundaries.
template <class Find>
constexpr std::string lines_where(Find find) {
  const std::vector<example> all = examples();
  std::string wrong = all.empty() ? "no lines read from the file\n" : "";
  for (const example& one : all)
    if (!find(one))
      wrong += std::string(one.line) + "\n";
  return wrong;
}

constexpr std::string hex(std::u8string_view text) {
  constexpr std::string_view digits = "0123456789ABCDEF";
  std::string out;
  for (const char8_t unit : text) {
    if (!out.empty())
      out += ' ';
    out += digits[unit >> 4];
    out += digits[unit & 0xF];
  }
  return out;
}

struct numbers {
  std::uint64_t state;
  constexpr std::uint32_t operator()() {
    state = state * 6364136223846793005u + 1442695040888963407u;
    return static_cast<std::uint32_t>(state >> 33);
  }
};

}  // namespace

// Every line that is not a comment begins with a boundary, and every one of
// them is read.
CONSTEXPR_TEST(GraphemeBreakTest, EveryLineIsRead) {
  const std::string_view file = grapheme_break_test();
  std::size_t lines = file.starts_with("\xC3\xB7") ? 1 : 0;
  for (std::size_t at = file.find("\n\xC3\xB7"); at != std::string_view::npos;
       at = file.find("\n\xC3\xB7", at + 1))
    ++lines;
  CONSTEXPR_EXPECT_TRUE(lines > 0);
  CONSTEXPR_EXPECT_EQ(examples().size(), lines);
}

CONSTEXPR_TEST(GraphemeBreakTest, EveryLineForwards) {
  CONSTEXPR_EXPECT_EQ(lines_where([](const example& one) {
                        return forwards(one.text) == one.boundaries;
                      }),
                      "");
}

CONSTEXPR_TEST(GraphemeBreakTest, EveryLineBackwards) {
  CONSTEXPR_EXPECT_EQ(lines_where([](const example& one) {
                        return backwards(one.text) == one.boundaries;
                      }),
                      "");
}

CONSTEXPR_TEST(GraphemeBreakTest, EveryLineAtEveryByte) {
  CONSTEXPR_EXPECT_EQ(lines_where([](const example& one) {
                        return everywhere(one.text) == one.boundaries;
                      }),
                      "");
}

CONSTEXPR_TEST(GraphemeBreakTest, EveryLineThroughReverse) {
  CONSTEXPR_EXPECT_EQ(lines_where([](const example& one) {
                        return reversed(one.text) == one.boundaries;
                      }),
                      "");
}

CONSTEXPR_TEST(GraphemeBreakTest, EveryLineInUtf16AndUtf32) {
  CONSTEXPR_EXPECT_EQ(lines_where([](const example& one) {
                        return same_in_utf16_and_utf32(one.text);
                      }),
                      "");
}

CONSTEXPR_TEST(GraphemeBreakTest, EveryLineReadOnce) {
  CONSTEXPR_EXPECT_EQ(lines_where([](const example& one) {
                        return the_same_read_once(std::u8string_view(one.text));
                      }),
                      "");
}

CONSTEXPR_TEST(GraphemeBreakTest, EveryLineReadOnceInUtf16AndUtf32) {
  CONSTEXPR_EXPECT_EQ(
      lines_where([](const example& one) {
        const auto in_utf16 =
            one.text | alef::as_utf16 | std::ranges::to<std::u16string>();
        const auto in_utf32 =
            one.text | alef::as_utf32 | std::ranges::to<std::u32string>();
        return the_same_read_once(std::u16string_view(in_utf16)) &&
               the_same_read_once(std::u32string_view(in_utf32));
      }),
      "");
}

// Text made of the code points the rules are about, with stray bytes in it,
// comes apart the same way however it is read. While compiled, 150 of them.
CONSTEXPR_TEST(Graphemes, RandomTextTheSameEveryWay) {
  constexpr char32_t interesting[] = {
      U'a',          U'\r',         U'\n',         U'\x01',
      U'\U00000300',                                // Extend
      U'\U0000200D', U'\U0001F1E6', U'\U0001F1E7',  // ZWJ, regional indicators
      U'\U0001F468', U'\U000000A9',                 // pictographs
      U'\U00000600', U'\U00000903',                 // Prepend, SpacingMark
      U'\U00001100', U'\U00001161', U'\U000011A8',  // Hangul jamo
      U'\U0000AC00', U'\U0000AC01',                 // Hangul syllables
      U'\U00000915', U'\U0000094D', U'\U00000937', U'\U0000093C',  // Devanagari
      U'\U0001F3FB',                                // an emoji modifier
  };
  numbers next{0x2545F4914F6CDD1D};
  int samples = 100000;
  if consteval {
    samples = 150;
  }
  std::string wrong;
  for (int sample = 0; sample < samples && wrong.empty(); ++sample) {
    std::u8string text;
    const std::uint32_t length = 1 + next() % 12;
    for (std::uint32_t at = 0; at < length; ++at) {
      if (next() % 16 == 0) {
        text.push_back(static_cast<char8_t>(0x80 + next() % 0x40));  // stray
      } else {
        const char32_t one = interesting[next() % std::size(interesting)];
        for (const char8_t unit : std::u32string_view(&one, 1) | alef::as_utf8)
          text.push_back(unit);
      }
    }
    const std::vector<std::size_t> found = forwards(text);
    if (backwards(text) != found || everywhere(text) != found ||
        reversed(text) != found || !same_in_utf16_and_utf32(text) ||
        !the_same_read_once(std::u8string_view(text)))
      wrong = hex(text);
  }
  CONSTEXPR_EXPECT_EQ(wrong, "");
}

CONSTEXPR_TEST(Graphemes, WhatAReaderTakesForOneCharacter) {
  CONSTEXPR_EXPECT_EQ(std::ranges::distance(u8"e\U00000301" | alef::graphemes), 1);
  CONSTEXPR_EXPECT_EQ(std::ranges::distance(u"e\U00000301" | alef::graphemes), 1);
  CONSTEXPR_EXPECT_EQ(std::ranges::distance(U"e\U00000301" | alef::graphemes), 1);
  CONSTEXPR_EXPECT_EQ(std::ranges::distance("\r\n" | alef::graphemes), 1);
  // Regional indicators go in pairs: four of them are two clusters.
  CONSTEXPR_EXPECT_EQ(std::ranges::distance(u8"\U0001F1E6\U0001F1E7\U0001F1E6\U0001F1E7" |
                                            alef::graphemes),
                      2);
  // Pictographs joined by ZWJ.
  CONSTEXPR_EXPECT_EQ(std::ranges::distance(u8"\U0001F9D1\U0000200D\U0001F4BB" |
                                            alef::graphemes),
                      1);
  // ksha: consonant, virama, consonant (GB9c).
  CONSTEXPR_EXPECT_EQ(std::ranges::distance(u8"\U00000915\U0000094D\U00000937" |
                                            alef::graphemes),
                      1);
  // The example in the README.
  CONSTEXPR_EXPECT_EQ(
      std::ranges::distance(u8"e\U00000301\U0001F9D1\U0000200D\U0001F4BB"
                            u8"\U00000915\U0000094D\U00000937" |
                            alef::graphemes),
      3);
  CONSTEXPR_EXPECT_EQ(std::ranges::distance(u8"ae\U00000301" | alef::graphemes |
                                            std::views::reverse),
                      2);
}

CONSTEXPR_TEST(Graphemes, BoundariesOnIterators) {
  constexpr std::u8string_view text = u8"ae\U00000301";
  CONSTEXPR_EXPECT_TRUE(alef::next_grapheme_boundary(text.begin(), text.end()) ==
                        text.begin() + 1);
  CONSTEXPR_EXPECT_TRUE(alef::prev_grapheme_boundary(text.begin(), text.end()) ==
                        text.begin() + 1);
  CONSTEXPR_EXPECT_TRUE(alef::is_grapheme_boundary(text.begin(), text.begin() + 1, text.end()));
  CONSTEXPR_EXPECT_FALSE(alef::is_grapheme_boundary(text.begin(), text.begin() + 2, text.end()));
  // Inside U+0301.
  CONSTEXPR_EXPECT_FALSE(alef::is_grapheme_boundary(text.begin(), text.begin() + 3, text.end()));
}

CONSTEXPR_TEST(Graphemes, PropertiesReadFromTheUcd) {
  CONSTEXPR_EXPECT_TRUE(alef::grapheme_cluster_break_of(U'\U00000301') ==
                        alef::grapheme_cluster_break::extend);
  CONSTEXPR_EXPECT_TRUE(alef::indic_conjunct_break_of(U'\U0000094D') ==
                        alef::indic_conjunct_break::linker);
  CONSTEXPR_EXPECT_TRUE(alef::is_extended_pictographic(U'\U0001F389'));
  CONSTEXPR_EXPECT_FALSE(alef::is_extended_pictographic(U'a'));
}

CONSTEXPR_TEST(Graphemes, TemporaryStringIsKept) {
  CONSTEXPR_EXPECT_EQ(std::ranges::distance(std::u8string(u8"e\U00000301x") |
                                            alef::graphemes),
                      2);
}

// Lazily: a cluster not read to its end is read past when the next one is
// asked for, and the view is an input range and no more.
CONSTEXPR_TEST(Graphemes, LazilyOneAtATime) {
  constexpr std::u8string_view text =
      u8"e\U00000301\U0001F9D1\U0000200D\U0001F4BBx";
  using lazy = decltype(once(text) | alef::lazy_graphemes);
  CONSTEXPR_EXPECT_TRUE(std::ranges::input_range<lazy>);
  CONSTEXPR_EXPECT_FALSE(std::ranges::forward_range<lazy>);
  CONSTEXPR_EXPECT_EQ(std::ranges::distance(once(text) | alef::lazy_graphemes), 3);
  std::u8string firsts;
  for (auto&& cluster : once(text) | alef::lazy_graphemes)
    firsts.push_back(*cluster.begin());
  CONSTEXPR_EXPECT_TRUE(firsts == std::u8string{u8'e', char8_t(0xF0), u8'x'});
}

// What each way gives out: pieces of text read more than once; graphemes of
// their own for text read once, or when asked, with room in them for 32
// bytes' worth of the text's UTF unless said otherwise.
CONSTEXPR_TEST(Graphemes, WhatEachWayGivesOut) {
  using piece = std::ranges::subrange<std::ranges::iterator_t<std::u8string_view>>;
  CONSTEXPR_EXPECT_TRUE(std::same_as<
      std::ranges::range_value_t<decltype(std::u8string_view() | alef::graphemes)>,
      piece>);
  CONSTEXPR_EXPECT_TRUE(std::same_as<
      std::ranges::range_reference_t<decltype(once(std::u8string_view()) | alef::graphemes)>,
      const alef::grapheme<char8_t>&>);
  CONSTEXPR_EXPECT_TRUE(std::same_as<
      std::ranges::range_value_t<decltype(std::u8string_view() | alef::graphemes(alef::owning<>))>,
      alef::grapheme<char8_t, 32>>);
  CONSTEXPR_EXPECT_TRUE(std::same_as<
      std::ranges::range_value_t<decltype(std::u16string_view() | alef::graphemes(alef::owning<>))>,
      alef::grapheme<char16_t, 16>>);
  CONSTEXPR_EXPECT_TRUE(std::same_as<
      std::ranges::range_value_t<decltype(std::u32string_view() | alef::graphemes(alef::owning<>))>,
      alef::grapheme<char32_t, 8>>);
  CONSTEXPR_EXPECT_TRUE(std::same_as<
      std::ranges::range_value_t<decltype(alef::graphemes(std::u8string_view(), alef::owning<4>))>,
      alef::grapheme<char8_t, 4>>);
  CONSTEXPR_EXPECT_FALSE(std::ranges::forward_range<
      decltype(std::u8string_view() | alef::graphemes(alef::owning<>))>);
  // Room said in bytes: as many code units as fit, whatever the UTF.
  CONSTEXPR_EXPECT_TRUE(std::same_as<
      std::ranges::range_value_t<decltype(std::u8string_view() | alef::graphemes(alef::owning_in_bytes<64>))>,
      alef::grapheme<char8_t, 64>>);
  CONSTEXPR_EXPECT_TRUE(std::same_as<
      std::ranges::range_value_t<decltype(std::u16string_view() | alef::graphemes(alef::owning_in_bytes<64>))>,
      alef::grapheme<char16_t, 32>>);
  CONSTEXPR_EXPECT_TRUE(std::same_as<
      std::ranges::range_value_t<decltype(std::u32string_view() | alef::graphemes(alef::owning_in_bytes<64>))>,
      alef::grapheme<char32_t, 16>>);
  CONSTEXPR_EXPECT_TRUE(std::same_as<
      std::ranges::range_value_t<decltype(alef::graphemes(std::u16string_view(), alef::owning_in_bytes<7>))>,
      alef::grapheme<char16_t, 3>>);
}

CONSTEXPR_TEST(Graphemes, OwnedFromTextReadOnce) {
  constexpr std::u8string_view text =
      u8"e\U00000301\U0001F9D1\U0000200D\U0001F4BBx";
  std::vector<alef::grapheme<char8_t>> clusters;
  for (const auto& cluster : once(text) | alef::graphemes)
    clusters.push_back(cluster);
  CONSTEXPR_EXPECT_EQ(clusters.size(), 3u);
  CONSTEXPR_EXPECT_TRUE(clusters.size() == 3 &&
                        clusters[0] == u8"e\U00000301" &&
                        clusters[1] == u8"\U0001F9D1\U0000200D\U0001F4BB" &&
                        clusters[2] == u8"x");
  CONSTEXPR_EXPECT_TRUE(clusters.size() == 3 && clusters[1].is_inline());
}

// Text that is expensive to read: owned, each code unit of it is read once.
CONSTEXPR_TEST(Graphemes, OwnedReadEachCodeUnitOnce) {
  constexpr std::u8string_view text =
      u8"e\U00000301\U0001F9D1\U0000200D\U0001F4BBx";
  std::size_t reads = 0;
  const auto expensive = text | std::views::transform([&reads](char8_t unit) {
                           ++reads;
                           return unit;
                         });
  std::size_t units = 0;
  for (const auto& cluster : expensive | alef::graphemes(alef::owning<>))
    units += cluster.size();
  CONSTEXPR_EXPECT_EQ(units, text.size());
  CONSTEXPR_EXPECT_EQ(reads, text.size());
}

CONSTEXPR_TEST(Graphemes, AGraphemeOfItsOwn) {
  CONSTEXPR_EXPECT_EQ(alef::grapheme<char8_t>::inline_capacity, 32u);
  CONSTEXPR_EXPECT_EQ(alef::grapheme<char16_t>::inline_capacity, 16u);
  CONSTEXPR_EXPECT_EQ(alef::grapheme<char32_t>::inline_capacity, 8u);
  const alef::grapheme<char8_t, 4> small(u8"e\U00000301");
  CONSTEXPR_EXPECT_TRUE(small.is_inline());
  alef::grapheme<char8_t, 4> big(u8"\U0001F9D1\U0000200D\U0001F4BB");
  CONSTEXPR_EXPECT_FALSE(big.is_inline());
  CONSTEXPR_EXPECT_EQ(big.size(), 11u);
  const alef::grapheme<char8_t, 4> copied = big;
  CONSTEXPR_EXPECT_TRUE(copied == big);
  const alef::grapheme<char8_t, 4> moved = std::move(big);
  CONSTEXPR_EXPECT_TRUE(moved == copied);
  CONSTEXPR_EXPECT_TRUE(big.empty());
  CONSTEXPR_EXPECT_TRUE(small < copied);
  CONSTEXPR_EXPECT_TRUE(std::u8string_view(small) == u8"e\U00000301");
}

// A stream, which is not for the compiler to read: owned by default, and
// lazily when asked.
TEST(Graphemes, TextReadFromAStream) {
  const auto read = [](auto adaptor) {
    std::istringstream stream("e\xCC\x81" "x\r\n");
    stream >> std::noskipws;
    std::vector<std::string> clusters;
    for (auto&& cluster : std::views::istream<char>(stream) | adaptor)
      clusters.push_back(cluster | std::ranges::to<std::string>());
    return clusters;
  };
  const std::vector<std::string> expected{"e\xCC\x81", "x", "\r\n"};
  EXPECT_EQ(read(alef::graphemes), expected);
  EXPECT_EQ(read(alef::lazy_graphemes), expected);
}
