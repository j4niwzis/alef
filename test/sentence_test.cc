// Sentence boundaries, by the rules of UAX #29: checked against the test file the
// Unicode Consortium publishes beside the data, and against themselves -- the
// same boundaries found forwards, backwards, at every byte, through
// std::views::reverse and in every UTF.
import std;
import alef.utf;
import alef.sentence;
import gtest;

#include "gtest/gtest-macros.h"

#include "constexpr_test.h"
#include "data.h"
#include "once.h"

namespace {

// SentenceBreakTest.txt, of the version the tables are read from.
#if defined(ALEF_CONSTEXPR_TESTS)
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc23-extensions"
#endif
constexpr char sentence_break_test_bytes[] = {
#embed "../ucd/auxiliary/SentenceBreakTest.txt"
};
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
constexpr std::string_view sentence_break_test() {
  return {sentence_break_test_bytes, sizeof sentence_break_test_bytes};
}
#else
std::string_view sentence_break_test() {
  return alef::test::ucd_file("auxiliary/SentenceBreakTest.txt");
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
  const std::string_view file = sentence_break_test();
  std::vector<example> out;
  for (std::size_t start = 0; start < file.size();) {
    std::size_t end = file.find('\n', start);
    if (end == std::string_view::npos)
      end = file.size();
    const std::string_view whole = file.substr(start, end - start);
    const std::string_view data = whole.substr(0, whole.find('#'));
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

template <class Unit>
constexpr std::vector<std::size_t> forwards(std::basic_string_view<Unit> text) {
  std::vector<std::size_t> found{0};
  for (const auto piece : text | alef::sentences)
    found.push_back(static_cast<std::size_t>(piece.end() - text.begin()));
  return found;
}

// Where the sentences end, the text read only once.
constexpr std::vector<std::size_t> read_once(std::u8string_view text) {
  std::vector<std::size_t> found{0};
  std::size_t at = 0;
  for (const std::u8string_view piece : once(text) | alef::sentences) {
    at += piece.size();
    found.push_back(at);
  }
  return found;
}

constexpr std::vector<std::size_t> backwards(std::u8string_view text) {
  std::vector<std::size_t> found{text.size()};
  for (auto at = text.end(); at != text.begin();) {
    at = alef::prev_sentence_boundary(text.begin(), at);
    found.push_back(static_cast<std::size_t>(at - text.begin()));
  }
  std::ranges::reverse(found);
  return found;
}

constexpr std::vector<std::size_t> everywhere(std::u8string_view text) {
  std::vector<std::size_t> found;
  for (std::size_t at = 0; at <= text.size(); ++at)
    if (alef::is_sentence_boundary(text.begin(), text.begin() + at, text.end()))
      found.push_back(at);
  return found;
}

constexpr std::vector<std::size_t> reversed(std::u8string_view text) {
  std::vector<std::size_t> found{text.size()};
  for (const auto piece : text | alef::sentences | std::views::reverse)
    found.push_back(static_cast<std::size_t>(piece.begin() - text.begin()));
  std::ranges::reverse(found);
  return found;
}

// How many code points each piece of some text is, in whatever UTF.
template <class Text>
constexpr std::vector<std::ptrdiff_t> sizes(const Text& text) {
  std::vector<std::ptrdiff_t> out;
  for (const auto piece : text | alef::sentences)
    out.push_back(std::ranges::distance(piece | alef::as_utf32));
  return out;
}

constexpr bool same_in_utf16_and_utf32(std::u8string_view text) {
  const auto in_utf16 = std::ranges::to<std::u16string>(text | alef::as_utf16);
  const auto in_utf32 = std::ranges::to<std::u32string>(text | alef::as_utf32);
  return sizes(in_utf16) == sizes(text) && sizes(in_utf32) == sizes(text);
}

// The lines of the file on which `find` does not find the boundaries.
template <class Find>
constexpr std::string lines_where(Find find) {
  const std::vector<example> all = examples();
  std::string wrong = all.empty() ? "no lines read from the file\n" : "";
  for (const example& one : all)
    if (!find(one) && wrong.size() < 4000)
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

template <class Text>
constexpr std::vector<std::u8string> pieces(const Text& text) {
  std::vector<std::u8string> out;
  for (const auto piece : text | alef::sentences)
    out.emplace_back(piece.begin(), piece.end());
  return out;
}

}  // namespace

CONSTEXPR_TEST(SentenceBreakTest, EveryLineForwards) {
  CONSTEXPR_EXPECT_EQ(lines_where([](const example& one) {
                        return forwards(std::u8string_view(one.text)) == one.boundaries;
                      }),
                      "");
}

CONSTEXPR_TEST(SentenceBreakTest, EveryLineBackwards) {
  CONSTEXPR_EXPECT_EQ(lines_where([](const example& one) {
                        return backwards(one.text) == one.boundaries;
                      }),
                      "");
}

CONSTEXPR_TEST(SentenceBreakTest, EveryLineAtEveryByte) {
  CONSTEXPR_EXPECT_EQ(lines_where([](const example& one) {
                        return everywhere(one.text) == one.boundaries;
                      }),
                      "");
}

CONSTEXPR_TEST(SentenceBreakTest, EveryLineThroughReverse) {
  CONSTEXPR_EXPECT_EQ(lines_where([](const example& one) {
                        return reversed(one.text) == one.boundaries;
                      }),
                      "");
}

CONSTEXPR_TEST(SentenceBreakTest, EveryLineReadOnce) {
  CONSTEXPR_EXPECT_EQ(lines_where([](const example& one) {
                        return read_once(one.text) == one.boundaries;
                      }),
                      "");
}

CONSTEXPR_TEST(SentenceBreakTest, EveryLineInUtf16AndUtf32) {
  CONSTEXPR_EXPECT_EQ(lines_where([](const example& one) {
                        return same_in_utf16_and_utf32(one.text);
                      }),
                      "");
}

// Text made of the code points the rules are about, with stray bytes in it,
// comes apart the same way however it is read. While compiled, 150 of them.
CONSTEXPR_TEST(Sentences, RandomTextTheSameEveryWay) {
  constexpr char32_t interesting[] = {
      U'a',          U'Z',          U'\U000005D0', U'1',          U'.',   // Lower, Upper, OLetter, Numeric, ATerm
      U'?',          U'!',          U'\U00003002', U')',          U'"',   // STerm, Close
      U',',          U' ',          U'\t',         U'\r',         U'\n',  // SContinue, Sp, CR, LF
      U'\U00002029', U'\U00000085', U'\U00000301', U'\U000000AD', U'\x01',  // Sep, Extend, Format
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
    const std::vector<std::size_t> found = forwards(std::u8string_view(text));
    if (backwards(text) != found || everywhere(text) != found ||
        reversed(text) != found || read_once(text) != found ||
        !same_in_utf16_and_utf32(text))
      wrong = hex(text);
  }
  CONSTEXPR_EXPECT_EQ(wrong, "");
}

CONSTEXPR_TEST(Sentences, WhatComesApart) {
  using found = std::vector<std::u8string>;
  // A sentence keeps the spaces after it (SB9 to SB11).
  CONSTEXPR_EXPECT_TRUE(pieces(std::u8string_view(u8"This is a test. And another! Is it? Yes.")) ==
                        (found{u8"This is a test. ", u8"And another! ", u8"Is it? ", u8"Yes."}));
  // A full stop before a lower-case word, a number, or between capitals is
  // not the end of a sentence (SB8, SB6, SB7).
  CONSTEXPR_EXPECT_TRUE(pieces(std::u8string_view(u8"etc. and 3.4 U.S.A. is here.")) ==
                        (found{u8"etc. and 3.4 U.S.A. is here."}));
  // Closing punctuation stays with the sentence (SB9), then the spaces.
  CONSTEXPR_EXPECT_TRUE(pieces(std::u8string_view(u8"He said (\"Stop.\") Then left.")) ==
                        (found{u8"He said (\"Stop.\") ", u8"Then left."}));
  // A paragraph separator ends a sentence, a CR LF being one (SB3, SB4).
  CONSTEXPR_EXPECT_TRUE(pieces(std::u8string_view(u8"Line one\r\nLine two\nthree")) ==
                        (found{u8"Line one\r\n", u8"Line two\n", u8"three"}));
  // The ideographic full stop is STerm.
  CONSTEXPR_EXPECT_TRUE(pieces(std::u8string_view(u8"\U00004E2D\U00006587\U00003002\U00004E00\U00003002")) ==
                        (found{u8"\U00004E2D\U00006587\U00003002", u8"\U00004E00\U00003002"}));
  CONSTEXPR_EXPECT_TRUE(alef::sentence_break_of(U'a') == alef::sentence_break::lower);
  CONSTEXPR_EXPECT_TRUE(alef::sentence_break_of(U'A') == alef::sentence_break::upper);
  CONSTEXPR_EXPECT_TRUE(alef::sentence_break_of(U'.') == alef::sentence_break::aterm);
  CONSTEXPR_EXPECT_TRUE(alef::sentence_break_of(U'!') == alef::sentence_break::sterm);
  CONSTEXPR_EXPECT_TRUE(alef::sentence_break_of(U' ') == alef::sentence_break::sp);
}

CONSTEXPR_TEST(Sentences, BoundariesOnIterators) {
  constexpr std::u8string_view text = u8"Hi. Yo.";
  CONSTEXPR_EXPECT_TRUE(alef::next_sentence_boundary(text.begin(), text.end()) == text.begin() + 4);
  CONSTEXPR_EXPECT_TRUE(alef::prev_sentence_boundary(text.begin(), text.end()) == text.begin() + 4);
  CONSTEXPR_EXPECT_TRUE(alef::is_sentence_boundary(text.begin(), text.begin() + 4, text.end()));
  CONSTEXPR_EXPECT_FALSE(alef::is_sentence_boundary(text.begin(), text.begin() + 2, text.end()));
}
