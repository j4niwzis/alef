// The UTFs: what text in each reads as, what is not well-formed and what it
// reads as instead, and that text comes apart at the same places whether it
// is read forwards or backwards.
import std;
import alef;
import gtest;

#include "gtest/gtest-macros.h"

#include "constexpr_test.h"

namespace {

template <class Text>
constexpr std::u32string utf32(Text&& text) {
  return std::forward<Text>(text) | alef::as_utf32 |
         std::ranges::to<std::u32string>();
}

constexpr std::u32string replaced(std::size_t times) {
  return std::u32string(times, alef::replacement_character);
}

// Code units in hex, to say which text something went wrong on.
template <class Text>
constexpr std::string hex(const Text& text) {
  constexpr std::string_view digits = "0123456789ABCDEF";
  std::string out;
  for (const auto unit : text) {
    const auto value = static_cast<std::uint32_t>(unit);
    if (!out.empty())
      out += ' ';
    for (int shift = int{sizeof unit} * 8 - 4; shift >= 0; shift -= 4)
      out += digits[(value >> shift) & 0xF];
  }
  return out;
}

// Text that can be read only once: an input range and nothing more, as a
// stream is -- and unlike a stream, one the compiler can read.
class once : public std::ranges::view_interface<once> {
 public:
  class iterator {
   public:
    using value_type = char;
    using difference_type = std::ptrdiff_t;

    constexpr iterator(const char* at, const char* last)
        : at_(at), last_(last) {}
    iterator(iterator&&) = default;
    iterator& operator=(iterator&&) = default;

    constexpr char operator*() const { return *at_; }
    constexpr iterator& operator++() {
      ++at_;
      return *this;
    }
    constexpr void operator++(int) { ++at_; }
    friend constexpr bool operator==(const iterator& one,
                                     std::default_sentinel_t) {
      return one.at_ == one.last_;
    }

   private:
    const char* at_;
    const char* last_;
  };

  constexpr explicit once(std::string_view text) : text_(text) {}
  constexpr iterator begin() {
    return {text_.data(), text_.data() + text_.size()};
  }
  constexpr std::default_sentinel_t end() { return {}; }

 private:
  std::string_view text_;
};

// Where each code point begins and what it is: read forwards, and read
// backwards and put back in order.
template <class Text>
constexpr bool same_both_ways(const Text& text) {
  const auto view = text | alef::as_utf32;
  std::vector<std::pair<std::ptrdiff_t, char32_t>> forwards;
  for (auto at = view.begin(); at != view.end(); ++at)
    forwards.emplace_back(at.base() - text.begin(), *at);
  std::vector<std::pair<std::ptrdiff_t, char32_t>> backwards;
  for (auto at = view.end(); at != view.begin();) {
    --at;
    backwards.emplace_back(at.base() - text.begin(), *at);
  }
  std::ranges::reverse(backwards);
  return forwards == backwards;
}

// The bytes that tell the forms of UTF-8 apart, and the code units that tell
// the surrogates apart.
constexpr std::uint8_t bytes_that_matter[] = {
    0x00, 0x41, 0x7F, 0x80, 0x8F, 0x90, 0x9F, 0xA0, 0xBF, 0xC0, 0xC1, 0xC2, 0xDF,
    0xE0, 0xE1, 0xEC, 0xED, 0xEE, 0xEF, 0xF0, 0xF1, 0xF3, 0xF4, 0xF5, 0xFF};
constexpr char16_t units_that_matter[] = {0x0041, 0xD7FF, 0xD800, 0xDBFF,
                                          0xDC00, 0xDFFF, 0xE000, 0xFFFF};

// Numbers anyone can write down: the same ones while compiled and when run.
struct numbers {
  std::uint64_t state;
  constexpr std::uint32_t operator()() {
    state = state * 6364136223846793005u + 1442695040888963407u;
    return static_cast<std::uint32_t>(state >> 33);
  }
};

}  // namespace

// The example in section 3.9 of the Unicode Standard: each maximal subpart of
// an ill-formed sequence is one U+FFFD.
CONSTEXPR_TEST(Utf8, EachMaximalSubpartIsOneReplacement) {
  CONSTEXPR_EXPECT_EQ(
      utf32("\x61\xF1\x80\x80\xE1\x80\xC2\x62\x80\x63\x80\xBF\x64"),
      U"a\U0000FFFD\U0000FFFD\U0000FFFDb\U0000FFFDc\U0000FFFD\U0000FFFDd");
}

// A surrogate written out is not UTF-8, and no prefix of it is either; nor is
// anything past U+10FFFF, nor a byte that begins nothing.
CONSTEXPR_TEST(Utf8, WhatIsNotUtf8) {
  CONSTEXPR_EXPECT_EQ(utf32("\xED\xA0\x80"), replaced(3));
  CONSTEXPR_EXPECT_EQ(utf32("\xF4\x90\x80\x80"), replaced(4));
  CONSTEXPR_EXPECT_EQ(utf32("\xC0\xC1\xF5\xFF"), replaced(4));
  CONSTEXPR_EXPECT_FALSE(alef::is_well_formed("\xC0\xAF"));  // an overlong '/'
  CONSTEXPR_EXPECT_TRUE(alef::is_well_formed(u8"h\U000000E9llo"));
}

// A sequence cut short is one replacement, and what cut it short is read as
// what it is.
CONSTEXPR_TEST(Utf8, SequenceCutShort) {
  CONSTEXPR_EXPECT_EQ(utf32("\xE2\x82"), replaced(1));
  CONSTEXPR_EXPECT_EQ(utf32("\xE2\x82" "a"), U"\U0000FFFDa");
}

CONSTEXPR_TEST(Utf16, UnpairedSurrogates) {
  const std::u16string halves{u'a', char16_t(0xD800), u'b', char16_t(0xDC00),
                              char16_t(0xD83C)};
  CONSTEXPR_EXPECT_EQ(utf32(halves), U"a\U0000FFFDb\U0000FFFD\U0000FFFD");
}

CONSTEXPR_TEST(Utf32, SurrogatesAndWhatIsPastU10FFFF) {
  const std::u32string values{U'a', char32_t(0xD800), char32_t(0x110000), U'b'};
  CONSTEXPR_EXPECT_EQ(values | alef::as_utf8 | std::ranges::to<std::u8string>(),
                      u8"a\U0000FFFD\U0000FFFDb");
}

CONSTEXPR_TEST(Utf, FromEachToEach) {
  CONSTEXPR_EXPECT_EQ(utf32(u8"a\U000000E9\U0001F389"), U"a\U000000E9\U0001F389");
  CONSTEXPR_EXPECT_EQ(U"a\U000000E9\U0001F389" | alef::as_utf8 |
                          std::ranges::to<std::u8string>(),
                      u8"a\U000000E9\U0001F389");
  CONSTEXPR_EXPECT_EQ(u8"a\U0001F389" | alef::as_utf16 |
                          std::ranges::to<std::u16string>(),
                      u"a\U0001F389");
  CONSTEXPR_EXPECT_EQ(std::ranges::distance(u8"a\U000000E9\U0001F389" | alef::as_utf16), 4);
  CONSTEXPR_EXPECT_EQ(std::ranges::distance("" | alef::as_utf32), 0);
}

// Every scalar value from UTF-32 to UTF-8 to UTF-16 and back. While compiled,
// every 509th of them and the ones at the edges: all of them is more than a
// constant expression can afford.
CONSTEXPR_TEST(Utf, EveryScalarValueThereAndBack) {
  char32_t step = 1;
  std::u32string scalars;
  if consteval {
    step = 509;
    scalars = U"\U0000007F\U00000080\U000007FF\U00000800\U0000D7FF\U0000E000"
              U"\U0000FFFF\U00010000\U0010FFFF";
  }
  for (char32_t one = 0; one <= 0x10FFFF; one += step)
    if (one < 0xD800 || one > 0xDFFF)
      scalars.push_back(one);
  const auto in_utf8 = scalars | alef::as_utf8 | std::ranges::to<std::u8string>();
  const auto in_utf16 = in_utf8 | alef::as_utf16 | std::ranges::to<std::u16string>();
  CONSTEXPR_EXPECT_TRUE(utf32(in_utf16) == scalars);
  CONSTEXPR_EXPECT_TRUE(alef::is_well_formed(in_utf8));
  CONSTEXPR_EXPECT_TRUE(alef::is_well_formed(in_utf16));
}

CONSTEXPR_TEST(Utf, TextReadOnlyOnce) {
  CONSTEXPR_EXPECT_EQ(utf32(once("a\xC3\xA9\xE2\x82\xAC\xE2\x82")),
                      U"a\U000000E9\U000020AC\U0000FFFD");
}

// A stream, which is not for the compiler to read.
TEST(Utf, TextReadFromAStream) {
  std::istringstream stream("a\xC3\xA9\xE2\x82\xAC");
  stream >> std::noskipws;
  EXPECT_EQ(utf32(std::views::istream<char>(stream)), U"a\U000000E9\U000020AC");
}

CONSTEXPR_TEST(Utf, TemporaryStringIsKept) {
  CONSTEXPR_EXPECT_EQ(std::ranges::distance(std::string("a\xC3\xA9") | alef::as_utf32), 2);
}

CONSTEXPR_TEST(Utf, AsLazyAsTheText) {
  using over_a_view = decltype(std::string_view() | alef::as_utf32);
  using read_once = decltype(once("") | alef::as_utf32);
  CONSTEXPR_EXPECT_TRUE(std::ranges::bidirectional_range<over_a_view>);
  CONSTEXPR_EXPECT_TRUE(std::ranges::common_range<over_a_view>);
  CONSTEXPR_EXPECT_TRUE(std::ranges::borrowed_range<over_a_view>);
  CONSTEXPR_EXPECT_TRUE(std::ranges::input_range<read_once>);
  CONSTEXPR_EXPECT_FALSE(std::ranges::forward_range<read_once>);
  CONSTEXPR_EXPECT_EQ(*std::ranges::prev((u8"a\U0001F389" | alef::as_utf32).end()),
                      U'\U0001F389');
}

// An iterator says where its code point begins in the text, and whether it
// was read or replaced.
CONSTEXPR_TEST(Utf, IteratorSaysWhereAndWhether) {
  constexpr std::string_view text = "a\xFF\xC3\xA9";
  const auto view = text | alef::as_utf32;
  auto at = view.begin();
  CONSTEXPR_EXPECT_TRUE(at.well_formed());
  ++at;
  CONSTEXPR_EXPECT_FALSE(at.well_formed());
  CONSTEXPR_EXPECT_EQ(*at, alef::replacement_character);
  ++at;
  CONSTEXPR_EXPECT_TRUE(at.well_formed());
  CONSTEXPR_EXPECT_EQ(at.base() - text.begin(), 2);
  CONSTEXPR_EXPECT_EQ(*at, U'\U000000E9');
}

// Every string of two bytes, read both ways. While compiled, the ones made of
// the bytes that matter.
CONSTEXPR_TEST(Utf8, EveryTwoBytesTheSameBothWays) {
  std::string wrong;
  const auto both_ways = [&](unsigned first, unsigned second) {
    const std::string text{static_cast<char>(first), static_cast<char>(second)};
    if (!same_both_ways(text))
      wrong += hex(text) + "; ";
  };
  if consteval {
    for (const unsigned first : bytes_that_matter)
      for (const unsigned second : bytes_that_matter)
        both_ways(first, second);
  } else {
    for (unsigned first = 0; first < 256; ++first)
      for (unsigned second = 0; second < 256; ++second)
        both_ways(first, second);
  }
  CONSTEXPR_EXPECT_EQ(wrong, "");
}

// Longer UTF-8 made of the bytes that matter, and UTF-16 made of the units
// that matter, read both ways.
CONSTEXPR_TEST(Utf, RandomTextTheSameBothWays) {
  numbers next{0x9E3779B97F4A7C15};
  int samples = 200000;
  if consteval {
    samples = 500;
  }
  std::string wrong;
  for (int sample = 0; sample < samples && wrong.empty(); ++sample) {
    std::string text(1 + next() % 9, '\0');
    for (char& byte : text)
      byte = static_cast<char>(bytes_that_matter[next() % std::size(bytes_that_matter)]);
    if (!same_both_ways(text))
      wrong = "UTF-8: " + hex(text);
    std::u16string wide(1 + next() % 6, u'\0');
    for (char16_t& unit : wide)
      unit = units_that_matter[next() % std::size(units_that_matter)];
    if (!same_both_ways(wide))
      wrong = "UTF-16: " + hex(wide);
  }
  CONSTEXPR_EXPECT_EQ(wrong, "");
}
