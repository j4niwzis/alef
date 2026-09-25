// The bidirectional algorithm, by the rules of UAX #9: against the two test
// files the Unicode Consortium publishes beside the data -- BidiTest.txt, of
// bidi classes, and BidiCharacterTest.txt, of code points -- and by example.
import std;
import alef.utf;
import alef.bidi;
import gtest;

#include "gtest/gtest-macros.h"

#include "constexpr_test.h"
#include "data.h"

namespace {

// The test files, of the version the tables are read from.
#if defined(ALEF_CONSTEXPR_TESTS)
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc23-extensions"
#endif
constexpr char bidi_test_bytes[] = {
#embed "../ucd/BidiTest.txt"
};
constexpr char bidi_character_test_bytes[] = {
#embed "../ucd/BidiCharacterTest.txt"
};
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
constexpr std::string_view bidi_test() {
  return {bidi_test_bytes, sizeof bidi_test_bytes};
}
constexpr std::string_view bidi_character_test() {
  return {bidi_character_test_bytes, sizeof bidi_character_test_bytes};
}
#else
std::string_view bidi_test() {
  return alef::test::ucd_file("BidiTest.txt");
}
std::string_view bidi_character_test() {
  return alef::test::ucd_file("BidiCharacterTest.txt");
}
#endif

using alef::bidi_class;
using alef::bidi_direction;

// What is between the spaces and tabs of some text.
constexpr std::vector<std::string_view> words(std::string_view text) {
  std::vector<std::string_view> out;
  for (std::size_t at = text.find_first_not_of(" \t"); at != std::string_view::npos;
       at = text.find_first_not_of(" \t", at)) {
    const std::size_t stop = std::min(text.find_first_of(" \t", at), text.size());
    out.push_back(text.substr(at, stop - at));
    at = stop;
  }
  return out;
}

constexpr std::size_t decimal(std::string_view digits) {
  std::size_t value = 0;
  for (const char digit : digits)
    value = value * 10 + static_cast<std::size_t>(digit - '0');
  return value;
}

constexpr char32_t hex_value(std::string_view digits) {
  char32_t value = 0;
  for (const char digit : digits)
    value = value * 16 + static_cast<char32_t>(
        digit <= '9' ? digit - '0' : (digit | 0x20) - 'a' + 10);
  return value;
}

// Levels as the files write them: a number, or x where X9 removes it.
constexpr std::vector<std::uint8_t> levels_of(std::string_view text) {
  std::vector<std::uint8_t> out;
  for (const std::string_view word : words(text))
    out.push_back(word == "x" ? alef::removed_level : static_cast<std::uint8_t>(decimal(word)));
  return out;
}

constexpr std::vector<std::size_t> order_of(std::string_view text) {
  std::vector<std::size_t> out;
  for (const std::string_view word : words(text))
    out.push_back(decimal(word));
  return out;
}

constexpr std::optional<bidi_class> class_named(std::string_view name) {
  constexpr std::pair<std::string_view, bidi_class> all[] = {
      {"L", bidi_class::l},     {"R", bidi_class::r},     {"AL", bidi_class::al},
      {"EN", bidi_class::en},   {"ES", bidi_class::es},   {"ET", bidi_class::et},
      {"AN", bidi_class::an},   {"CS", bidi_class::cs},   {"NSM", bidi_class::nsm},
      {"BN", bidi_class::bn},   {"B", bidi_class::b},     {"S", bidi_class::s},
      {"WS", bidi_class::ws},   {"ON", bidi_class::on},   {"LRE", bidi_class::lre},
      {"LRO", bidi_class::lro}, {"RLE", bidi_class::rle}, {"RLO", bidi_class::rlo},
      {"PDF", bidi_class::pdf}, {"LRI", bidi_class::lri}, {"RLI", bidi_class::rli},
      {"FSI", bidi_class::fsi}, {"PDI", bidi_class::pdi},
  };
  for (const auto& [each, value] : all)
    if (each == name)
      return value;
  return std::nullopt;
}

// Each line of a file that is not a comment, without the \r it may end in.
template <class Visit>
constexpr void each_line(std::string_view file, Visit visit) {
  for (std::size_t start = 0; start < file.size();) {
    std::size_t end = file.find('\n', start);
    if (end == std::string_view::npos)
      end = file.size();
    std::string_view line = file.substr(start, end - start);
    start = end + 1;
    if (!line.empty() && line.back() == '\r')
      line.remove_suffix(1);
    if (!line.empty() && line.front() != '#')
      visit(line);
  }
}

constexpr std::string direction_name(bidi_direction direction) {
  return direction == bidi_direction::ltr   ? "ltr"
         : direction == bidi_direction::rtl ? "rtl"
                                            : "auto";
}

// BidiTest.txt, every `every`-th data line, with each paragraph direction its
// bitset lists: the lines whose levels or order come out otherwise than the
// @Levels and @Reorder before them say.
constexpr std::string bidi_test_failures(std::size_t every) {
  std::vector<std::uint8_t> levels;
  std::vector<std::size_t> order;
  std::string wrong;
  std::size_t index = 0;
  std::size_t checked = 0;
  each_line(bidi_test(), [&](std::string_view line) {
    if (line.starts_with("@Levels:")) {
      levels = levels_of(line.substr(8));
      return;
    }
    if (line.starts_with("@Reorder:")) {
      order = order_of(line.substr(9));
      return;
    }
    if (line.front() == '@' || index++ % every != 0)
      return;
    const std::size_t semicolon = line.find(';');
    if (semicolon == std::string_view::npos)
      return;
    std::vector<bidi_class> classes;
    for (const std::string_view word : words(line.substr(0, semicolon))) {
      const std::optional<bidi_class> found = class_named(word);
      if (!found) {
        wrong += "unknown class in: " + std::string(line) + "\n";
        return;
      }
      classes.push_back(*found);
    }
    const std::size_t bits = hex_value(words(line.substr(semicolon + 1)).front());
    constexpr std::pair<std::size_t, bidi_direction> directions[] = {
        {1, bidi_direction::automatic}, {2, bidi_direction::ltr}, {4, bidi_direction::rtl}};
    for (const auto& [bit, direction] : directions) {
      if ((bits & bit) == 0)
        continue;
      ++checked;
      const alef::bidi_paragraph paragraph(classes, direction);
      if ((paragraph.line_levels(0, paragraph.size()) != levels ||
           paragraph.visual_order(0, paragraph.size()) != order) &&
          wrong.size() < 4000)
        wrong += std::string(line) + " (" + direction_name(direction) + ")\n";
    }
  });
  return checked == 0 ? "no lines read from the file\n" : wrong;
}

// BidiCharacterTest.txt, every `every`-th line, in UTF-32, or in UTF-8 where
// `in_utf8`: the lines whose paragraph level, levels or order come out
// otherwise than it says.
constexpr std::string bidi_character_test_failures(std::size_t every, bool in_utf8) {
  std::string wrong;
  std::size_t index = 0;
  std::size_t checked = 0;
  each_line(bidi_character_test(), [&](std::string_view line) {
    if (index++ % every != 0)
      return;
    std::array<std::string_view, 5> fields{};
    std::size_t at = 0;
    for (std::string_view& field : fields) {
      const std::size_t from = std::min(at, line.size());
      const std::size_t stop = std::min(line.find(';', from), line.size());
      field = line.substr(from, stop - from);
      at = stop + 1;
    }
    std::u32string text;
    for (const std::string_view word : words(fields[0]))
      text.push_back(hex_value(word));
    const bidi_direction direction = fields[1] == "0"   ? bidi_direction::ltr
                                     : fields[1] == "1" ? bidi_direction::rtl
                                                        : bidi_direction::automatic;
    ++checked;
    const auto right = [&](const alef::bidi_paragraph& paragraph) {
      return std::size_t{paragraph.level()} == decimal(fields[2]) &&
             paragraph.line_levels(0, paragraph.size()) == levels_of(fields[3]) &&
             paragraph.visual_order(0, paragraph.size()) == order_of(fields[4]);
    };
    const bool passed =
        in_utf8 ? right(alef::bidi_paragraph(
                      text | alef::as_utf8 | std::ranges::to<std::u8string>(), direction))
                : right(alef::bidi_paragraph(std::u32string_view(text), direction));
    if (!passed && wrong.size() < 4000)
      wrong += std::string(line) + "\n";
  });
  return checked == 0 ? "no lines read from the file\n" : wrong;
}

}  // namespace

// Every line when run; while compiled, every 1000th.
CONSTEXPR_TEST(BidiTest, EveryLine) {
  std::size_t every = 1;
  if consteval {
    every = 1000;
  }
  CONSTEXPR_EXPECT_EQ(bidi_test_failures(every), "");
}

// Every line when run; while compiled, every 300th.
CONSTEXPR_TEST(BidiCharacterTest, EveryLine) {
  std::size_t every = 1;
  if consteval {
    every = 300;
  }
  CONSTEXPR_EXPECT_EQ(bidi_character_test_failures(every, false), "");
}

CONSTEXPR_TEST(BidiCharacterTest, EveryLineInUtf8) {
  std::size_t every = 1;
  if consteval {
    every = 300;
  }
  CONSTEXPR_EXPECT_EQ(bidi_character_test_failures(every, true), "");
}

CONSTEXPR_TEST(Bidi, ByExample) {
  // The classes of a few code points, and what an unassigned one is: R in
  // the Hebrew block, AL in the Arabic one.
  CONSTEXPR_EXPECT_TRUE(alef::bidi_class_of(U'a') == bidi_class::l);
  CONSTEXPR_EXPECT_TRUE(alef::bidi_class_of(U'א') == bidi_class::r);
  CONSTEXPR_EXPECT_TRUE(alef::bidi_class_of(U'ا') == bidi_class::al);
  CONSTEXPR_EXPECT_TRUE(alef::bidi_class_of(U'1') == bidi_class::en);
  CONSTEXPR_EXPECT_TRUE(alef::bidi_class_of(U'١') == bidi_class::an);
  CONSTEXPR_EXPECT_TRUE(alef::bidi_class_of(U'⁧') == bidi_class::rli);
  CONSTEXPR_EXPECT_TRUE(alef::bidi_class_of(U'׿') == bidi_class::r);
  CONSTEXPR_EXPECT_TRUE(alef::bidi_class_of(U'޿') == bidi_class::al);
  // Mirrored glyphs, and a code point without one.
  CONSTEXPR_EXPECT_TRUE(alef::mirrored(U'(') == U')');
  CONSTEXPR_EXPECT_TRUE(alef::mirrored(U'≤') == U'≥');
  CONSTEXPR_EXPECT_TRUE(alef::mirrored(U'a') == U'a');
  // Hebrew in English: the Hebrew is shown right to left.
  const alef::bidi_paragraph mixed(u8"abc אבג");
  CONSTEXPR_EXPECT_TRUE(mixed.level() == 0);
  CONSTEXPR_EXPECT_TRUE(mixed.visual_order(0, mixed.size()) ==
                        (std::vector<std::size_t>{0, 1, 2, 3, 6, 5, 4}));
  // A paragraph that begins in Hebrew goes right to left, and the English
  // in it, and the number after the English, left to right.
  const alef::bidi_paragraph hebrew(u8"אב abc 123");
  CONSTEXPR_EXPECT_TRUE(hebrew.level() == 1);
  CONSTEXPR_EXPECT_TRUE(hebrew.visual_order(0, hebrew.size()) ==
                        (std::vector<std::size_t>{3, 4, 5, 6, 7, 8, 9, 2, 1, 0}));
  // A number in Hebrew keeps its digits left to right.
  const alef::bidi_paragraph number(u8"א 123");
  CONSTEXPR_EXPECT_TRUE(number.visual_order(0, number.size()) ==
                        (std::vector<std::size_t>{2, 3, 4, 1, 0}));
  // A direction asked for is the direction of the paragraph.
  const alef::bidi_paragraph asked(u8"אב", bidi_direction::ltr);
  CONSTEXPR_EXPECT_TRUE(asked.level() == 0);
  CONSTEXPR_EXPECT_TRUE(asked.visual_order(0, asked.size()) == (std::vector<std::size_t>{1, 0}));
  // An embedding is removed by X9, and has no level.
  const alef::bidi_paragraph embedded(u8"a‫b‬c");
  CONSTEXPR_EXPECT_TRUE(embedded.levels()[1] == alef::removed_level);
  CONSTEXPR_EXPECT_TRUE(embedded.levels()[2] == 2);
}

namespace {

// P1: the paragraphs of a text, forwards and, through reverse, backwards.
constexpr std::vector<std::u8string> paragraphs_of(std::u8string_view text) {
  std::vector<std::u8string> found;
  for (const auto piece : text | alef::paragraphs)
    found.emplace_back(piece.begin(), piece.end());
  return found;
}

constexpr std::vector<std::u8string> paragraphs_backwards(std::u8string_view text) {
  std::vector<std::u8string> found;
  for (const auto piece : text | alef::paragraphs | std::views::reverse)
    found.emplace_back(piece.begin(), piece.end());
  std::ranges::reverse(found);
  return found;
}

// The paragraphs of the text read in another UTF, back in UTF-8.
template <class Text>
constexpr std::vector<std::u8string> paragraphs_in(const Text& text) {
  std::vector<std::u8string> found;
  for (const auto piece : std::basic_string_view(text) | alef::paragraphs)
    found.push_back(piece | alef::as_utf8 | std::ranges::to<std::u8string>());
  return found;
}

// Found the same forwards, backwards and in every UTF, and together the text.
constexpr bool paragraphs_the_same_every_way(std::u8string_view text) {
  const std::vector<std::u8string> found = paragraphs_of(text);
  if (paragraphs_backwards(text) != found)
    return false;
  std::u8string together;
  std::vector<std::u8string> decoded;
  for (const std::u8string& piece : found) {
    together += piece;
    decoded.push_back(piece | alef::as_utf8 | std::ranges::to<std::u8string>());
  }
  if (together != text)
    return false;
  const auto in_utf16 = text | alef::as_utf16 | std::ranges::to<std::u16string>();
  const auto in_utf32 = text | alef::as_utf32 | std::ranges::to<std::u32string>();
  return paragraphs_in(in_utf16) == decoded && paragraphs_in(in_utf32) == decoded;
}

}  // namespace

CONSTEXPR_TEST(Paragraphs, WhatComesApart) {
  using found = std::vector<std::u8string>;
  // Every paragraph separator ends a paragraph and stays with it; a CR LF is
  // one separator. "\x1C" stands alone: a hex escape would swallow the "e".
  CONSTEXPR_EXPECT_TRUE(paragraphs_of(u8"a\nb\r\nc d\x1C" u8"e\u0085f") ==
                        (found{u8"a\n", u8"b\r\n", u8"c ", u8"d\x1C", u8"e\u0085", u8"f"}));
  CONSTEXPR_EXPECT_TRUE(paragraphs_of(u8"\r\r\n\n") == (found{u8"\r", u8"\r\n", u8"\n"}));
  CONSTEXPR_EXPECT_TRUE(paragraphs_of(u8"a\n") == (found{u8"a\n"}));
  CONSTEXPR_EXPECT_TRUE(paragraphs_of(u8"") == found{});
  // A line separator and a tab are not paragraph separators.
  CONSTEXPR_EXPECT_TRUE(paragraphs_of(u8"a b\tc") == (found{u8"a b\tc"}));
}

CONSTEXPR_TEST(Paragraphs, EachGoesItsOwnWay) {
  // P1 before P2: each paragraph finds its own direction.
  std::vector<std::uint8_t> levels;
  for (const auto piece : std::u8string_view(u8"abc\nאבג\n123") | alef::paragraphs)
    levels.push_back(alef::bidi_paragraph(piece).level());
  CONSTEXPR_EXPECT_TRUE(levels == (std::vector<std::uint8_t>{0, 1, 0}));
}

CONSTEXPR_TEST(Paragraphs, BoundariesOnIterators) {
  const std::u8string_view text = u8"ab\r\ncd";
  CONSTEXPR_EXPECT_TRUE(alef::next_paragraph_boundary(text.begin(), text.end()) == text.begin() + 4);
  CONSTEXPR_EXPECT_TRUE(alef::next_paragraph_boundary(text.begin() + 4, text.end()) == text.end());
  CONSTEXPR_EXPECT_TRUE(alef::prev_paragraph_boundary(text.begin(), text.end()) == text.begin() + 4);
  CONSTEXPR_EXPECT_TRUE(alef::prev_paragraph_boundary(text.begin(), text.begin() + 4) == text.begin());
}

CONSTEXPR_TEST(Paragraphs, RandomTextTheSameEveryWay) {
  constexpr std::array<std::u8string_view, 8> pieces{
      u8"a", u8"\r", u8"\n", u8" ", u8"א", u8"\u0085", u8" ", u8"\xFF"};
  std::size_t texts = 20000;
  if consteval {
    texts = 200;
  }
  std::uint64_t state = 0x9E3779B97F4A7C15;
  const auto next = [&state] {
    state = state * 6364136223846793005u + 1442695040888963407u;
    return static_cast<std::size_t>(state >> 33);
  };
  std::size_t wrong = 0;
  for (std::size_t n = 0; n < texts; ++n) {
    std::u8string text;
    for (std::size_t length = next() % 12; length-- > 0;)
      text += pieces[next() % pieces.size()];
    wrong += !paragraphs_the_same_every_way(text);
  }
  CONSTEXPR_EXPECT_EQ(wrong, std::size_t{0});
}

CONSTEXPR_TEST(BidiRuns, ByExample) {
  using runs = std::vector<alef::bidi_run>;
  // Hebrew in English: three runs, in the order they were written; the
  // Hebrew one right to left. Offsets are code units: two bytes a letter.
  const alef::bidi_paragraph mixed(u8"abc אבג def");
  CONSTEXPR_EXPECT_TRUE(mixed.runs(0, 14) == (runs{{0, 4, 0}, {4, 10, 1}, {10, 14, 0}}));
  // A line of it that begins at the Hebrew.
  CONSTEXPR_EXPECT_TRUE(mixed.runs(4, 14) == (runs{{4, 10, 1}, {10, 14, 0}}));
  // A right-to-left paragraph: the English, at level 2, is drawn at the
  // left, and the Hebrew with the space after it at the right.
  const alef::bidi_paragraph hebrew(u8"אב abc");
  CONSTEXPR_EXPECT_TRUE(hebrew.level() == 1);
  CONSTEXPR_EXPECT_TRUE(hebrew.runs(0, 8) == (runs{{5, 8, 2}, {0, 5, 1}}));
  CONSTEXPR_EXPECT_TRUE(hebrew.runs(0, 8)[1].right_to_left());
  // In UTF-16 the offsets are of UTF-16.
  const alef::bidi_paragraph utf16(u"abc אבג");
  CONSTEXPR_EXPECT_TRUE(utf16.runs(0, 7) == (runs{{0, 4, 0}, {4, 7, 1}}));
  // What X9 removes makes no run of its own.
  const alef::bidi_paragraph embedded(u8"a‪b‬c");
  for (const alef::bidi_run run : embedded.runs(0, 9))
    CONSTEXPR_EXPECT_TRUE(run.first < run.last);
}

namespace {

// The runs of a whole paragraph of UTF-32, drawn -- the code points of each in
// order, or turned round where it goes right to left, without what X9
// removes -- are the paragraph's visual order.
constexpr bool runs_draw_the_visual_order(std::u32string_view text) {
  const alef::bidi_paragraph paragraph(text);
  std::vector<std::size_t> drawn;
  std::size_t covered = 0;
  for (const alef::bidi_run run : paragraph.runs(0, text.size())) {
    covered += run.last - run.first;
    std::vector<std::size_t> in_run;
    for (std::size_t at = run.first; at < run.last; ++at)
      if (paragraph.levels()[at] != alef::removed_level)
        in_run.push_back(at);
    if (run.right_to_left())
      std::ranges::reverse(in_run);
    drawn.insert(drawn.end(), in_run.begin(), in_run.end());
  }
  return covered == text.size() && drawn == paragraph.visual_order(0, paragraph.size());
}

}  // namespace

// Text made of what the rules are about: strong letters both ways, numbers,
// spaces, brackets and every kind of embedding, override and isolate.
CONSTEXPR_TEST(BidiRuns, RandomTextDrawsTheVisualOrder) {
  constexpr char32_t interesting[] = {
      U'a', U'b', U'\U000005D0', U'\U00000627', U'1', U'\U00000661', U' ', U'(', U')', U'-',
      U'\U0000202A', U'\U0000202B', U'\U0000202C', U'\U0000202D', U'\U0000202E',
      U'\U00002066', U'\U00002067', U'\U00002068', U'\U00002069',
  };
  std::size_t texts = 20000;
  if consteval {
    texts = 150;
  }
  std::uint64_t state = 0x9E3779B97F4A7C15;
  const auto next = [&state] {
    state = state * 6364136223846793005u + 1442695040888963407u;
    return static_cast<std::size_t>(state >> 33);
  };
  std::size_t wrong = 0;
  for (std::size_t n = 0; n < texts; ++n) {
    std::u32string text;
    for (std::size_t length = 1 + next() % 14; length-- > 0;)
      text += interesting[next() % std::size(interesting)];
    wrong += !runs_draw_the_visual_order(text);
  }
  CONSTEXPR_EXPECT_EQ(wrong, std::size_t{0});
}
