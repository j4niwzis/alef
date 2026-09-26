// SPDX-License-Identifier: AGPL-3.0-only
// The properties grapheme clusters are decided by, as one table read from
// the UCD while this interface is compiled: Grapheme_Cluster_Break from
// GraphemeBreakProperty.txt, Indic_Conjunct_Break from
// DerivedCoreProperties.txt, Extended_Pictographic from emoji-data.txt, one
// byte a code point in a table of two stages.
//
// A module of its own that imports nothing of the library but the reader of
// the UCD, so that it is compiled again when its files or that reader
// change, and not when anything else does.
export module alef.tables.grapheme;

import std;
import alef.ucd;

export namespace alef {

// The Grapheme_Cluster_Break property, and a code point's value of it.
enum class grapheme_cluster_break : std::uint8_t {
  other,
  cr,
  lf,
  control,
  extend,
  zwj,
  regional_indicator,
  prepend,
  spacing_mark,
  l,
  v,
  t,
  lv,
  lvt,
};

// The Indic_Conjunct_Break property.
enum class indic_conjunct_break : std::uint8_t {
  none,
  linker,
  consonant,
  extend,
};

}  // namespace alef

namespace alef::tables::grapheme_data {

// #embed is C23, and in C++ an extension until C++26 has it. Said here rather
// than with a flag, so that whoever compiles this interface -- an importer of
// an installed alef among them -- needs no flag to do it quietly.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc23-extensions"
#endif
constexpr char grapheme_break_bytes[] = {
#embed "../../ucd/auxiliary/GraphemeBreakProperty.txt"
};
constexpr char derived_core_bytes[] = {
#embed "../../ucd/DerivedCoreProperties.txt"
};
constexpr char emoji_bytes[] = {
#embed "../../ucd/emoji/emoji-data.txt"
};
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

constexpr ucd::file grapheme_break_file{grapheme_break_bytes,
                                        sizeof grapheme_break_bytes};
constexpr ucd::file derived_core_file{derived_core_bytes,
                                      sizeof derived_core_bytes};
constexpr ucd::file emoji_file{emoji_bytes, sizeof emoji_bytes};

constexpr std::optional<grapheme_cluster_break> grapheme_break_of(
    const ucd::line& one) {
  using enum grapheme_cluster_break;
  if (one.count != 1)
    return std::nullopt;
  constexpr std::pair<std::string_view, grapheme_cluster_break> names[] = {
      {"CR", cr},
      {"LF", lf},
      {"Control", control},
      {"Extend", extend},
      {"ZWJ", zwj},
      {"Regional_Indicator", regional_indicator},
      {"Prepend", prepend},
      {"SpacingMark", spacing_mark},
      {"L", l},
      {"V", v},
      {"T", t},
      {"LV", lv},
      {"LVT", lvt},
  };
  for (const auto& [name, value] : names)
    if (one.fields[0] == name)
      return value;
  return std::nullopt;
}

constexpr std::optional<indic_conjunct_break> conjunct_break_of(
    const ucd::line& one) {
  using enum indic_conjunct_break;
  if (one.count != 2 || one.fields[0] != "InCB")
    return std::nullopt;
  if (one.fields[1] == "Linker")
    return linker;
  if (one.fields[1] == "Consonant")
    return consonant;
  if (one.fields[1] == "Extend")
    return extend;
  return std::nullopt;
}

constexpr std::optional<bool> pictographic_of(const ucd::line& one) {
  if (one.count != 1 || one.fields[0] != "Extended_Pictographic")
    return std::nullopt;
  return true;
}

constexpr auto grapheme_break =
    ucd::property<grapheme_cluster_break, grapheme_break_file,
                  &grapheme_break_of>;
constexpr auto conjunct_break =
    ucd::property<indic_conjunct_break, derived_core_file,
                  &conjunct_break_of>;
constexpr auto pictographic =
    ucd::property<bool, emoji_file, &pictographic_of>;

// The three in one byte: Grapheme_Cluster_Break in the low four bits,
// Indic_Conjunct_Break in the two above them, and whether it is
// Extended_Pictographic in the one above those. Zero is Other, None, and not.
template <class Ranges, class Bits>
constexpr std::vector<ucd::run<std::uint8_t>> runs_of(const Ranges& ranges,
                                                      Bits bits) {
  std::vector<ucd::run<std::uint8_t>> out;
  for (const auto& one : ranges)
    out.push_back({one.first, one.last, static_cast<std::uint8_t>(bits(one.value))});
  return out;
}

constexpr std::vector<ucd::run<std::uint8_t>> runs() {
  return ucd::merged<std::uint8_t>({
      runs_of(grapheme_break,
              [](grapheme_cluster_break value) { return static_cast<unsigned>(value); }),
      runs_of(conjunct_break,
              [](indic_conjunct_break value) { return static_cast<unsigned>(value) << 4; }),
      runs_of(pictographic, [](bool) { return 0x40u; }),
  });
}

// For Unicode 18.0.0, 111 distinct blocks are all 4352 of it: an index of a
// byte each, and 32 KB in all.
inline constexpr std::size_t blocks =
    ucd::lay_out(runs()).blocks.size() / ucd::block_size;
inline constexpr auto table =
    ucd::two_stage<std::uint8_t, blocks>(ucd::lay_out(runs()));

}  // namespace alef::tables::grapheme_data

export namespace alef::tables {

// What the rules ask of a code point, all of it in one byte.
struct grapheme_properties {
  std::uint8_t bits = 0;

  constexpr grapheme_cluster_break grapheme_break() const noexcept {
    return static_cast<grapheme_cluster_break>(bits & 0x0F);
  }
  constexpr indic_conjunct_break conjunct_break() const noexcept {
    return static_cast<indic_conjunct_break>((bits >> 4) & 0x03);
  }
  constexpr bool pictographic() const noexcept { return (bits & 0x40) != 0; }
};

constexpr grapheme_properties grapheme_properties_of(char32_t code_point) noexcept {
  return {grapheme_data::table[code_point]};
}

}  // namespace alef::tables
