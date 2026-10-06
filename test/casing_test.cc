// Case, against the files the tables are built from, read here a second way:
// the simple mappings of every code point, every folding of
// CaseFolding.txt, and every unconditional mapping of SpecialCasing.txt;
// and the conditional ones, titlecase, and comparing without case, by
// example.
import std;
import alef.utf;
import alef.casing;
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
constexpr char unicode_data_bytes[] = {
#embed "../ucd/UnicodeData.txt"
};
constexpr char special_casing_bytes[] = {
#embed "../ucd/SpecialCasing.txt"
};
constexpr char case_folding_bytes[] = {
#embed "../ucd/CaseFolding.txt"
};
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
constexpr std::string_view unicode_data() { return {unicode_data_bytes, sizeof unicode_data_bytes}; }
constexpr std::string_view special_casing() { return {special_casing_bytes, sizeof special_casing_bytes}; }
constexpr std::string_view case_folding() { return {case_folding_bytes, sizeof case_folding_bytes}; }
#else
std::string_view unicode_data() { return alef::test::ucd_file("UnicodeData.txt"); }
std::string_view special_casing() { return alef::test::ucd_file("SpecialCasing.txt"); }
std::string_view case_folding() { return alef::test::ucd_file("CaseFolding.txt"); }
#endif

constexpr std::string_view trimmed(std::string_view text) {
  const std::size_t first = text.find_first_not_of(" \t");
  if (first == std::string_view::npos)
    return {};
  return text.substr(first, text.find_last_not_of(" \t") - first + 1);
}

constexpr char32_t hex(std::string_view digits) {
  char32_t value = 0;
  for (const char digit : digits)
    value = value * 16 + static_cast<char32_t>(
        digit <= '9' ? digit - '0' : (digit | 0x20) - 'a' + 10);
  return value;
}

constexpr std::u32string code_points(std::string_view text) {
  std::u32string out;
  for (std::size_t start = text.find_first_not_of(' '); start != std::string_view::npos;) {
    std::size_t stop = text.find(' ', start);
    if (stop == std::string_view::npos)
      stop = text.size();
    out.push_back(hex(text.substr(start, stop - start)));
    start = text.find_first_not_of(' ', stop);
  }
  return out;
}

// The lines of a file, split by ';' and trimmed, before any comment.
constexpr std::vector<std::vector<std::string_view>> rows(std::string_view file) {
  std::vector<std::vector<std::string_view>> out;
  for (std::size_t start = 0; start < file.size();) {
    std::size_t end = file.find('\n', start);
    if (end == std::string_view::npos)
      end = file.size();
    std::string_view line = file.substr(start, end - start);
    start = end + 1;
    line = line.substr(0, line.find('#'));
    if (trimmed(line).empty())
      continue;
    std::vector<std::string_view> fields;
    std::size_t from = 0;
    for (std::size_t semicolon = line.find(';');; semicolon = line.find(';', from)) {
      fields.push_back(trimmed(line.substr(from, semicolon - from)));
      if (semicolon == std::string_view::npos)
        break;
      from = semicolon + 1;
    }
    out.push_back(std::move(fields));
  }
  return out;
}

template <class Form>
constexpr std::u32string in(std::u32string_view text, Form form) {
  return std::ranges::to<std::u32string>(text | form);
}

constexpr std::string hex_of(char32_t code_point) {
  constexpr std::string_view digits = "0123456789ABCDEF";
  std::string out;
  for (int shift = 20; shift >= 0; shift -= 4)
    out += digits[(code_point >> shift) & 0xF];
  return out;
}

}  // namespace

// Fields 12, 13 and 14 of UnicodeData.txt; every line when run, every 7th
// while compiled.
CONSTEXPR_TEST(Casing, SimpleMappingsOfEveryCodePoint) {
  std::size_t every = 1;
  if consteval {
    every = 7;
  }
  std::string wrong;
  std::size_t index = 0;
  for (const auto& fields : rows(unicode_data())) {
    if (fields.size() < 15 || index++ % every != 0)
      continue;
    const char32_t code_point = hex(fields[0]);
    const auto or_itself = [&](std::string_view field) {
      return field.empty() ? code_point : hex(field);
    };
    if (alef::to_upper(code_point) != or_itself(fields[12]) ||
        alef::to_lower(code_point) != or_itself(fields[13]) ||
        alef::to_title(code_point) != or_itself(fields[14]))
      if (wrong.size() < 200)
        wrong += hex_of(code_point) + " ";
  }
  CONSTEXPR_EXPECT_EQ(wrong, "");
}

// Every line of CaseFolding.txt: C and S simple, C and F full.
CONSTEXPR_TEST(Casing, EveryFolding) {
  std::string wrong;
  for (const auto& fields : rows(case_folding())) {
    if (fields.size() < 3)
      continue;
    const char32_t code_point = hex(fields[0]);
    const std::u32string folded = code_points(fields[2]);
    const std::string_view status = fields[1];
    const bool right =
        (status == "S" || status == "C" ? alef::fold(code_point) == folded[0] : true) &&
        (status == "F" || status == "C"
             ? in(std::u32string_view(&code_point, 1), alef::as_folded) == folded
             : true) &&
        (status == "T" ? in(std::u32string_view(&code_point, 1),
                            alef::as_folded(alef::casing_language::turkic)) == folded
                       : true);
    if (!right && wrong.size() < 200)
      wrong += hex_of(code_point) + " ";
  }
  CONSTEXPR_EXPECT_EQ(wrong, "");
}

// Every unconditional line of SpecialCasing.txt: code; lower; title; upper.
CONSTEXPR_TEST(Casing, EveryUnconditionalSpecialCasing) {
  std::string wrong;
  std::size_t lines = 0;
  for (const auto& fields : rows(special_casing())) {
    if (fields.size() < 4 || (fields.size() > 4 && !fields[4].empty()))
      continue;
    ++lines;
    const char32_t code_point = hex(fields[0]);
    const std::u32string_view one(&code_point, 1);
    if (in(one, alef::as_lower) != code_points(fields[1]) ||
        in(one, alef::as_upper) != code_points(fields[3]) ||
        (alef::is_cased(code_point) && in(one, alef::as_title) != code_points(fields[2])))
      wrong += hex_of(code_point) + " ";
  }
  CONSTEXPR_EXPECT_TRUE(lines > 90);
  CONSTEXPR_EXPECT_EQ(wrong, "");
}

CONSTEXPR_TEST(Casing, FinalSigma) {
  // Final where a cased letter is before it and none after it.
  CONSTEXPR_EXPECT_TRUE(in(U"\U0000039F\U00000394\U0000039F\U000003A3", alef::as_lower) ==
                        U"\U000003BF\U000003B4\U000003BF\U000003C2");
  CONSTEXPR_EXPECT_TRUE(in(U"\U000003A3\U00000391", alef::as_lower) == U"\U000003C3\U000003B1");
  CONSTEXPR_EXPECT_TRUE(in(U"\U000003A3", alef::as_lower) == U"\U000003C3");
  // Case-ignorable on either side is passed over; a space is not.
  CONSTEXPR_EXPECT_TRUE(in(U"\U00000391\U000003A3'", alef::as_lower) == U"\U000003B1\U000003C2'");
  CONSTEXPR_EXPECT_TRUE(in(U"\U00000391\U000003A3'\U00000391", alef::as_lower) ==
                        U"\U000003B1\U000003C3'\U000003B1");
  CONSTEXPR_EXPECT_TRUE(in(U"\U00000391\U000003A3 \U00000391", alef::as_lower) ==
                        U"\U000003B1\U000003C2 \U000003B1");
}

CONSTEXPR_TEST(Casing, TurkishAndLithuanian) {
  constexpr auto turkic = alef::casing_language::turkic;
  constexpr auto lithuanian = alef::casing_language::lithuanian;
  CONSTEXPR_EXPECT_TRUE(in(U"I\U00000130", alef::as_lower(turkic)) == U"\U00000131i");
  CONSTEXPR_EXPECT_TRUE(in(U"I\U00000307", alef::as_lower(turkic)) == U"i");
  CONSTEXPR_EXPECT_TRUE(in(U"i", alef::as_upper(turkic)) == U"\U00000130");
  CONSTEXPR_EXPECT_TRUE(in(U"I", alef::as_lower) == U"i");
  CONSTEXPR_EXPECT_TRUE(in(U"\U00000130", alef::as_lower) == U"i\U00000307");
  CONSTEXPR_EXPECT_TRUE(in(U"I\U00000300", alef::as_lower(lithuanian)) == U"i\U00000307\U00000300");
  CONSTEXPR_EXPECT_TRUE(in(U"\U000000CC", alef::as_lower(lithuanian)) == U"i\U00000307\U00000300");
  CONSTEXPR_EXPECT_TRUE(in(U"i\U00000307", alef::as_upper(lithuanian)) == U"I");
  CONSTEXPR_EXPECT_TRUE(alef::equal_ignoring_case(U"\U00000130", U"i", turkic));
}

CONSTEXPR_TEST(Casing, TitlecaseAndComparing) {
  CONSTEXPR_EXPECT_TRUE(in(U"hello wORLD, it's 2025", alef::as_title) == U"Hello World, It's 2025");
  CONSTEXPR_EXPECT_TRUE(in(U"\U000001C6emal", alef::as_title) == U"\U000001C5emal");  // dz with caron
  CONSTEXPR_EXPECT_TRUE(in(U"\U000000DFa", alef::as_title) == U"Ssa");
  CONSTEXPR_EXPECT_TRUE(in(U"\U000000DF", alef::as_upper) == U"SS");
  CONSTEXPR_EXPECT_TRUE(alef::equal_ignoring_case(u8"Stra\U000000DFe", "STRASSE"));
  CONSTEXPR_EXPECT_FALSE(alef::equal_ignoring_case("abc", "abd"));
  // From UTF-8 read once, back to UTF-8.
  CONSTEXPR_EXPECT_TRUE((std::ranges::to<std::u8string>(once(std::u8string_view(u8"\U00000414\U00000430")) | alef::as_upper |
                         alef::as_utf8)) ==
                        u8"\U00000414\U00000410");
  CONSTEXPR_EXPECT_EQ(alef::to_lower(U'A'), U'a');
  CONSTEXPR_EXPECT_EQ(alef::to_upper(U'\U000000DF'), U'\U000000DF');  // no simple one
  CONSTEXPR_EXPECT_EQ(alef::fold(U'\U000003A3'), U'\U000003C3');
}
