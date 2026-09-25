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
