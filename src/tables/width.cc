// How many columns a code point takes where text is set in columns -- a
// terminal, say -- as a table read from the UCD while this interface is
// compiled, from General_Category, East_Asian_Width and Emoji_Presentation:
//
// - none for what is not seen on its own: nonspacing and enclosing marks,
//   format characters but the soft hyphen, controls, line and paragraph
//   separators, and the Hangul jamo that join a syllable after its first;
// - two for what is wide or fullwidth, and for what is presented as emoji;
// - one or two for what is ambiguous, as the caller says;
// - one for everything else.
//
// A module of its own that imports nothing of the library but the reader of
// the UCD, so that it is compiled again when its files or that reader change,
// and not when anything else does.
export module alef.tables.width;

import std;
import alef.ucd;

namespace alef::tables::width_data {

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc23-extensions"
#endif
constexpr char category_bytes[] = {
#embed "../../ucd/extracted/DerivedGeneralCategory.txt"
};
constexpr char east_asian_bytes[] = {
#embed "../../ucd/EastAsianWidth.txt"
};
constexpr char emoji_bytes[] = {
#embed "../../ucd/emoji/emoji-data.txt"
};
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

constexpr ucd::file category_file{category_bytes, sizeof category_bytes};
constexpr ucd::file east_asian_file{east_asian_bytes, sizeof east_asian_bytes};
constexpr ucd::file emoji_file{emoji_bytes, sizeof emoji_bytes};

constexpr std::optional<bool> seen_on_its_own_not(const ucd::line& one) {
  if (one.count != 1 || one.points == "00AD")
    return std::nullopt;
  for (const std::string_view category : {"Mn", "Me", "Cf", "Cc", "Zl", "Zp"})
    if (one.fields[0] == category)
      return true;
  return std::nullopt;
}
constexpr std::optional<bool> wide(const ucd::line& one) {
  if (one.count == 1 && (one.fields[0] == "W" || one.fields[0] == "F"))
    return true;
  return std::nullopt;
}
constexpr std::optional<bool> ambiguous(const ucd::line& one) {
  if (one.count == 1 && one.fields[0] == "A")
    return true;
  return std::nullopt;
}
constexpr std::optional<bool> emoji_presentation(const ucd::line& one) {
  if (one.count == 1 && one.fields[0] == "Emoji_Presentation")
    return true;
  return std::nullopt;
}

template <class Ranges>
constexpr std::vector<ucd::run<std::uint8_t>> runs_of(const Ranges& ranges,
                                                      std::uint8_t bit) {
  std::vector<ucd::run<std::uint8_t>> out;
  for (const auto& one : ranges)
    out.push_back({one.first, one.last, bit});
  return out;
}

// The width, as the table keeps it: 0 is one column, which is most of
// Unicode and so is what a code point in no run is; 1 is none; 2 is two;
// 3 is ambiguous.
inline constexpr std::uint8_t one_column = 0;
inline constexpr std::uint8_t no_column = 1;
inline constexpr std::uint8_t two_columns = 2;
inline constexpr std::uint8_t ambiguous_columns = 3;

constexpr std::vector<ucd::run<std::uint8_t>> runs() {
  const std::vector<ucd::run<std::uint8_t>> merged = ucd::merged<std::uint8_t>({
      runs_of(ucd::property<bool, category_file, &seen_on_its_own_not>, 0x01),
      runs_of(ucd::property<bool, east_asian_file, &wide>, 0x02),
      runs_of(ucd::property<bool, emoji_file, &emoji_presentation>, 0x04),
      runs_of(ucd::property<bool, east_asian_file, &ambiguous>, 0x08),
      // The Hangul jamo that join a syllable after its first: vowels and
      // final consonants.
      {{0x1160, 0x11FF, 0x10}, {0xD7B0, 0xD7FF, 0x10}},
  });
  std::vector<ucd::run<std::uint8_t>> out;
  for (const auto& one : merged) {
    const std::uint8_t width = (one.value & 0x11) != 0   ? no_column
                               : (one.value & 0x06) != 0 ? two_columns
                               : (one.value & 0x08) != 0 ? ambiguous_columns
                                                         : one_column;
    if (width == one_column)
      continue;
    if (!out.empty() && out.back().last + 1 == one.first && out.back().value == width)
      out.back().last = one.last;
    else
      out.push_back({one.first, one.last, width});
  }
  return out;
}

inline constexpr std::size_t blocks =
    ucd::lay_out(runs()).blocks.size() / ucd::block_size;
inline constexpr auto table =
    ucd::two_stage<std::uint8_t, blocks>(ucd::lay_out(runs()));

}  // namespace alef::tables::width_data

export namespace alef::tables {

// 0, 1 or 2 columns; or -1, which is ambiguous: one or two as the text is
// East Asian or not.
constexpr int columns_of(char32_t code_point) noexcept {
  switch (width_data::table[code_point]) {
    case width_data::no_column:
      return 0;
    case width_data::two_columns:
      return 2;
    case width_data::ambiguous_columns:
      return -1;
    default:
      return 1;
  }
}

}  // namespace alef::tables
