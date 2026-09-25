// The properties of every code point, against the files the tables are built
// from, read here a second way: by code that is not the library's, into
// ranges searched as they are, where the library has a table of two stages.
import std;
import alef.grapheme;
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
constexpr char grapheme_break_property_bytes[] = {
#embed "../ucd/auxiliary/GraphemeBreakProperty.txt"
};
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
constexpr std::string_view grapheme_break_property() {
  return {grapheme_break_property_bytes, sizeof grapheme_break_property_bytes};
}
#else
std::string_view grapheme_break_property() {
  return alef::test::ucd_file("auxiliary/GraphemeBreakProperty.txt");
}
#endif

#if defined(ALEF_CONSTEXPR_TESTS)
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc23-extensions"
#endif
constexpr char derived_core_properties_bytes[] = {
#embed "../ucd/DerivedCoreProperties.txt"
};
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
constexpr std::string_view derived_core_properties() {
  return {derived_core_properties_bytes, sizeof derived_core_properties_bytes};
}
#else
std::string_view derived_core_properties() {
  return alef::test::ucd_file("DerivedCoreProperties.txt");
}
#endif

#if defined(ALEF_CONSTEXPR_TESTS)
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc23-extensions"
#endif
constexpr char emoji_data_bytes[] = {
#embed "../ucd/emoji/emoji-data.txt"
};
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
constexpr std::string_view emoji_data() {
  return {emoji_data_bytes, sizeof emoji_data_bytes};
}
#else
std::string_view emoji_data() {
  return alef::test::ucd_file("emoji/emoji-data.txt");
}
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

template <class Value>
struct span {
  char32_t first = 0;
  char32_t last = 0;
  Value value{};
};

// The lines of `file` that `pick` gives a value for, from what follows their
// code points, sorted.
template <class Value, class Pick>
constexpr std::vector<span<Value>> read(std::string_view file, Pick pick) {
  std::vector<span<Value>> out;
  for (std::size_t start = 0; start < file.size();) {
    std::size_t end = file.find('\n', start);
    if (end == std::string_view::npos)
      end = file.size();
    const std::string_view line =
        file.substr(start, end - start).substr(0, file.substr(start, end - start).find('#'));
    start = end + 1;
    const std::size_t semicolon = line.find(';');
    if (semicolon == std::string_view::npos)
      continue;
    const std::string_view points = trimmed(line.substr(0, semicolon));
    const std::size_t dots = points.find("..");
    const char32_t first = hex(points.substr(0, dots));
    const char32_t last =
        dots == std::string_view::npos ? first : hex(points.substr(dots + 2));
    if (const std::optional<Value> value = pick(line.substr(semicolon + 1)))
      out.push_back({first, last, *value});
  }
  std::ranges::sort(out, {}, &span<Value>::first);
  return out;
}

template <class Value>
constexpr Value value_at(const std::vector<span<Value>>& spans,
                         char32_t code_point, Value otherwise) {
  auto after = std::ranges::upper_bound(spans, code_point, {}, &span<Value>::first);
  if (after == spans.begin())
    return otherwise;
  --after;
  return code_point <= after->last ? after->value : otherwise;
}

constexpr std::optional<alef::grapheme_cluster_break> grapheme_break(
    std::string_view fields) {
  using enum alef::grapheme_cluster_break;
  constexpr std::pair<std::string_view, alef::grapheme_cluster_break> names[] = {
      {"CR", cr},         {"LF", lf},
      {"Control", control}, {"Extend", extend},
      {"ZWJ", zwj},       {"Regional_Indicator", regional_indicator},
      {"Prepend", prepend}, {"SpacingMark", spacing_mark},
      {"L", l},           {"V", v},
      {"T", t},           {"LV", lv},
      {"LVT", lvt},
  };
  for (const auto& [name, value] : names)
    if (trimmed(fields) == name)
      return value;
  return std::nullopt;
}

constexpr std::optional<alef::indic_conjunct_break> conjunct_break(
    std::string_view fields) {
  const std::size_t semicolon = fields.find(';');
  if (semicolon == std::string_view::npos ||
      trimmed(fields.substr(0, semicolon)) != "InCB")
    return std::nullopt;
  const std::string_view value = trimmed(fields.substr(semicolon + 1));
  if (value == "Linker")
    return alef::indic_conjunct_break::linker;
  if (value == "Consonant")
    return alef::indic_conjunct_break::consonant;
  if (value == "Extend")
    return alef::indic_conjunct_break::extend;
  return std::nullopt;
}

constexpr std::optional<bool> pictographic(std::string_view fields) {
  if (trimmed(fields) == "Extended_Pictographic")
    return true;
  return std::nullopt;
}

constexpr std::string hex_of(char32_t code_point) {
  constexpr std::string_view digits = "0123456789ABCDEF";
  std::string out;
  for (int shift = 20; shift >= 0; shift -= 4)
    out += digits[(code_point >> shift) & 0xF];
  return out;
}

}  // namespace

// Every code point when run; while compiled, where every range of every
// property begins and ends, and the code points on either side of that.
CONSTEXPR_TEST(Properties, OfEveryCodePoint) {
  const auto grapheme_breaks = read<alef::grapheme_cluster_break>(
      grapheme_break_property(), grapheme_break);
  const auto conjunct_breaks = read<alef::indic_conjunct_break>(
      derived_core_properties(), conjunct_break);
  const auto pictographs = read<bool>(emoji_data(), pictographic);
  CONSTEXPR_EXPECT_FALSE(grapheme_breaks.empty() || conjunct_breaks.empty() ||
                         pictographs.empty());
  std::string wrong;
  const auto check = [&](char32_t code_point) {
    if (alef::grapheme_cluster_break_of(code_point) !=
            value_at(grapheme_breaks, code_point, alef::grapheme_cluster_break::other) ||
        alef::indic_conjunct_break_of(code_point) !=
            value_at(conjunct_breaks, code_point, alef::indic_conjunct_break::none) ||
        alef::is_extended_pictographic(code_point) !=
            value_at(pictographs, code_point, false))
      if (wrong.size() < 200)
        wrong += hex_of(code_point) + " ";
  };
  const auto edges = [&](const auto& spans) {
    for (const auto& one : spans) {
      check(one.first);
      check(one.last);
      if (one.first > 0)
        check(one.first - 1);
      check(one.last + 1);
    }
  };
  if consteval {
    edges(grapheme_breaks);
    edges(conjunct_breaks);
    edges(pictographs);
  } else {
    for (char32_t code_point = 0; code_point <= 0x10FFFF; ++code_point)
      check(code_point);
  }
  CONSTEXPR_EXPECT_EQ(wrong, "");
}

// And past the last of them, nothing.
CONSTEXPR_TEST(Properties, PastU10FFFF) {
  CONSTEXPR_EXPECT_TRUE(alef::grapheme_cluster_break_of(0x110000) ==
                        alef::grapheme_cluster_break::other);
  CONSTEXPR_EXPECT_TRUE(alef::indic_conjunct_break_of(0xFFFFFFFF) ==
                        alef::indic_conjunct_break::none);
  CONSTEXPR_EXPECT_FALSE(alef::is_extended_pictographic(0x7FFFFFFF));
}
