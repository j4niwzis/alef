// General_Category, Script, Script_Extensions and East_Asian_Width of every
// code point, against the files the tables are built from, read here a second
// way; the names of every script both ways; and the width of text.
import std;
import alef;
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
constexpr char categories_bytes[] = {
#embed "../ucd/extracted/DerivedGeneralCategory.txt"
};
constexpr char scripts_bytes[] = {
#embed "../ucd/Scripts.txt"
};
constexpr char extensions_bytes[] = {
#embed "../ucd/ScriptExtensions.txt"
};
constexpr char widths_bytes[] = {
#embed "../ucd/EastAsianWidth.txt"
};
constexpr char aliases_bytes[] = {
#embed "../ucd/PropertyValueAliases.txt"
};
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
constexpr std::string_view categories_file() { return {categories_bytes, sizeof categories_bytes}; }
constexpr std::string_view scripts_file() { return {scripts_bytes, sizeof scripts_bytes}; }
constexpr std::string_view extensions_file() { return {extensions_bytes, sizeof extensions_bytes}; }
constexpr std::string_view widths_file() { return {widths_bytes, sizeof widths_bytes}; }
constexpr std::string_view aliases_file() { return {aliases_bytes, sizeof aliases_bytes}; }
#else
std::string_view categories_file() { return alef::test::ucd_file("extracted/DerivedGeneralCategory.txt"); }
std::string_view scripts_file() { return alef::test::ucd_file("Scripts.txt"); }
std::string_view extensions_file() { return alef::test::ucd_file("ScriptExtensions.txt"); }
std::string_view widths_file() { return alef::test::ucd_file("EastAsianWidth.txt"); }
std::string_view aliases_file() { return alef::test::ucd_file("PropertyValueAliases.txt"); }
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

// A line of data: what is before its first ';', and the fields after it,
// trimmed, before any comment.
struct entry {
  std::string_view head;
  std::vector<std::string_view> fields;
};

constexpr std::vector<entry> entries(std::string_view file) {
  std::vector<entry> out;
  for (std::size_t start = 0; start < file.size();) {
    std::size_t end = file.find('\n', start);
    if (end == std::string_view::npos)
      end = file.size();
    std::string_view line = file.substr(start, end - start);
    start = end + 1;
    line = line.substr(0, line.find('#'));
    if (trimmed(line).empty())
      continue;
    entry one;
    std::size_t from = 0;
    for (std::size_t semicolon = line.find(';'); ; semicolon = line.find(';', from)) {
      const std::string_view field = trimmed(line.substr(from, semicolon - from));
      if (from == 0)
        one.head = field;
      else
        one.fields.push_back(field);
      if (semicolon == std::string_view::npos)
        break;
      from = semicolon + 1;
    }
    out.push_back(std::move(one));
  }
  return out;
}

struct span {
  char32_t first = 0;
  char32_t last = 0;
  std::string_view value;
};

constexpr std::vector<span> spans(std::string_view file) {
  std::vector<span> out;
  for (const entry& one : entries(file)) {
    if (one.fields.empty())
      continue;
    const std::size_t dots = one.head.find("..");
    const char32_t first = hex(one.head.substr(0, dots));
    const char32_t last = dots == std::string_view::npos ? first : hex(one.head.substr(dots + 2));
    out.push_back({first, last, one.fields[0]});
  }
  std::ranges::sort(out, {}, &span::first);
  return out;
}

constexpr std::string_view value_at(const std::vector<span>& all, char32_t code_point,
                                    std::string_view otherwise) {
  auto after = std::ranges::upper_bound(all, code_point, {}, &span::first);
  if (after == all.begin())
    return otherwise;
  --after;
  return code_point <= after->last ? after->value : otherwise;
}

constexpr std::string_view code_of(alef::general_category category) {
  constexpr std::string_view codes[] = {
      "Cn", "Lu", "Ll", "Lt", "Lm", "Lo", "Mn", "Mc", "Me", "Nd",
      "Nl", "No", "Pc", "Pd", "Ps", "Pe", "Pi", "Pf", "Po", "Sm",
      "Sc", "Sk", "So", "Zs", "Zl", "Zp", "Cc", "Cf", "Cs", "Co"};
  return codes[static_cast<std::size_t>(category)];
}

constexpr std::string_view code_of(alef::east_asian_width width) {
  constexpr std::string_view codes[] = {"N", "A", "H", "W", "F", "Na"};
  return codes[static_cast<std::size_t>(width)];
}

constexpr std::string hex_of(char32_t code_point) {
  constexpr std::string_view digits = "0123456789ABCDEF";
  std::string out;
  for (int shift = 20; shift >= 0; shift -= 4)
    out += digits[(code_point >> shift) & 0xF];
  return out;
}

}  // namespace

// Every code point when run; while compiled, where each range of each file
// begins and ends, and the code points on either side.
CONSTEXPR_TEST(GeneralProperties, OfEveryCodePoint) {
  const std::vector<span> categories = spans(categories_file());
  const std::vector<span> scripts = spans(scripts_file());
  const std::vector<span> extensions = spans(extensions_file());
  const std::vector<span> widths = spans(widths_file());
  CONSTEXPR_EXPECT_FALSE(categories.empty() || scripts.empty() ||
                         extensions.empty() || widths.empty());
  std::string wrong;
  const auto check = [&](char32_t code_point) {
    bool right =
        code_of(alef::general_category_of(code_point)) ==
            value_at(categories, code_point, "Cn") &&
        code_of(alef::east_asian_width_of(code_point)) ==
            value_at(widths, code_point, "N") &&
        alef::script_of(code_point).name() == value_at(scripts, code_point, "Unknown");
    // The scripts it is used with: the codes the file lists, or its script.
    const std::string_view listed = value_at(extensions, code_point, "");
    std::vector<std::string_view> expected;
    for (std::size_t start = listed.find_first_not_of(' ');
         start != std::string_view::npos;) {
      std::size_t stop = listed.find(' ', start);
      if (stop == std::string_view::npos)
        stop = listed.size();
      expected.push_back(listed.substr(start, stop - start));
      start = listed.find_first_not_of(' ', stop);
    }
    if (expected.empty())
      expected.push_back(alef::script_of(code_point).code());
    std::vector<std::string_view> found;
    for (const alef::script one : alef::script_extensions_of(code_point))
      found.push_back(one.code());
    std::ranges::sort(expected);
    std::ranges::sort(found);
    right = right && found == expected;
    if (!right && wrong.size() < 200)
      wrong += hex_of(code_point) + " ";
  };
  if consteval {
    for (const std::vector<span>* all : {&categories, &scripts, &extensions, &widths})
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
  CONSTEXPR_EXPECT_EQ(wrong, "");
}

// Every script PropertyValueAliases.txt names, found by its code and by its
// name, both ways.
CONSTEXPR_TEST(GeneralProperties, ScriptsByNameAndCode) {
  std::string wrong;
  std::size_t named = 0;
  for (const entry& one : entries(aliases_file())) {
    if (one.head != "sc" || one.fields.size() < 2)
      continue;
    ++named;
    const alef::script by_code(one.fields[0]);
    const alef::script by_name(one.fields[1]);
    if (by_code != by_name || by_code.code() != one.fields[0] ||
        by_code.name() != one.fields[1])
      wrong += std::string(one.fields[0]) + " ";
  }
  CONSTEXPR_EXPECT_EQ(wrong, "");
  CONSTEXPR_EXPECT_EQ(alef::script::count(), named);
  CONSTEXPR_EXPECT_TRUE(alef::script("Latin") == alef::script("Latn"));
  CONSTEXPR_EXPECT_TRUE(alef::script("no such script") == alef::script());
  CONSTEXPR_EXPECT_TRUE(alef::script().name() == "Unknown");
  CONSTEXPR_EXPECT_TRUE(alef::script_of(U'a') == alef::script("Latin"));
  CONSTEXPR_EXPECT_TRUE(alef::script_of(U'\U00000416') == alef::script("Cyrillic"));
  CONSTEXPR_EXPECT_TRUE(alef::script_extensions_of(U'\U00000964').contains(alef::script("Devanagari")));
}

CONSTEXPR_TEST(Width, OfText) {
  CONSTEXPR_EXPECT_EQ(alef::width("abc"), 3u);
  CONSTEXPR_EXPECT_EQ(alef::width(u8"\U000065E5\U0000672C"), 4u);                        // CJK
  CONSTEXPR_EXPECT_EQ(alef::width(u8"e\U00000301"), 1u);                                 // a mark
  CONSTEXPR_EXPECT_EQ(alef::width(u8"\U0001F468\U0000200D\U0001F469\U0000200D\U0001F467"), 2u);
  CONSTEXPR_EXPECT_EQ(alef::width(u8"\U0001F1E6\U0001F1E7"), 2u);                        // a flag
  CONSTEXPR_EXPECT_EQ(alef::width(u8"\U00002764\U0000FE0F"), 2u);                        // as emoji
  CONSTEXPR_EXPECT_EQ(alef::width(u8"\U00002764"), 1u);                                  // as text
  CONSTEXPR_EXPECT_EQ(alef::width(u8"\U0000231A\U0000FE0E"), 1u);                        // asked for text
  CONSTEXPR_EXPECT_EQ(alef::width(u8"\U0000D55C"), 2u);                                  // a syllable
  CONSTEXPR_EXPECT_EQ(alef::width(u8"\U00001112\U00001161\U000011AB"), 2u);              // its jamo
  CONSTEXPR_EXPECT_EQ(alef::width("\x01"), 0u);                                          // a control
  CONSTEXPR_EXPECT_EQ(alef::width(u8"\U000003B1"), 1u);                                  // ambiguous
  CONSTEXPR_EXPECT_EQ(alef::width(u8"\U000003B1", alef::ambiguous_width::wide), 2u);
  CONSTEXPR_EXPECT_EQ(alef::width(u"\U000065E5a"), 3u);                                  // UTF-16
  CONSTEXPR_EXPECT_EQ(alef::width_of(U'\U000000AD'), 1);                                 // soft hyphen
  CONSTEXPR_EXPECT_EQ(alef::width_of(U'\U0000200B'), 0);                                 // zero width space
  CONSTEXPR_EXPECT_EQ(alef::width_of(U'\U00003000'), 2);                                 // ideographic space
}
