// SPDX-License-Identifier: AGPL-3.0-only
// The properties string preparation asks of code points, as one table read
// from the UCD while this interface is compiled: Joining_Type from
// extracted/DerivedJoiningType.txt, Hangul_Syllable_Type from
// HangulSyllableType.txt, and whether a code point's decomposition is Wide or
// Narrow, from extracted/DerivedDecompositionType.txt; one byte a code point
// in a table of two stages.
//
// A module of its own that imports nothing of the library but the reader of
// the UCD, so that it is compiled again when its files or that reader
// change, and not when anything else does.
export module alef.tables.preparation;

import std;
import alef.ucd;

export namespace alef {

// The Joining_Type property: how a letter of a cursive script joins.
enum class joining_type : std::uint8_t {
  non_joining,     // U
  join_causing,    // C
  dual_joining,    // D
  right_joining,   // R
  left_joining,    // L
  transparent,     // T
};

}  // namespace alef

namespace alef::tables::preparation_data {

// #embed is C23, and in C++ an extension until C++26 has it. Said here rather
// than with a flag, so that whoever compiles this interface -- an importer of
// an installed alef among them -- needs no flag to do it quietly.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc23-extensions"
#endif
constexpr char joining_bytes[] = {
#embed "../../ucd/extracted/DerivedJoiningType.txt"
};
constexpr char hangul_bytes[] = {
#embed "../../ucd/HangulSyllableType.txt"
};
constexpr char decomposition_bytes[] = {
#embed "../../ucd/extracted/DerivedDecompositionType.txt"
};
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

constexpr ucd::file joining_file{joining_bytes, sizeof joining_bytes};
constexpr ucd::file hangul_file{hangul_bytes, sizeof hangul_bytes};
constexpr ucd::file decomposition_file{decomposition_bytes, sizeof decomposition_bytes};

constexpr std::optional<joining_type> joining_of(const ucd::line& one) {
  using enum joining_type;
  if (one.count != 1)
    return std::nullopt;
  constexpr std::pair<std::string_view, joining_type> names[] = {
      {"C", join_causing}, {"D", dual_joining}, {"R", right_joining},
      {"L", left_joining}, {"T", transparent},
  };
  for (const auto& [name, value] : names)
    if (one.fields[0] == name)
      return value;
  return std::nullopt;
}

// Only what is a conjoining jamo: L, V and T -- not the syllables, LV and LVT.
constexpr std::optional<bool> jamo_of(const ucd::line& one) {
  if (one.count != 1 || !(one.fields[0] == "L" || one.fields[0] == "V" || one.fields[0] == "T"))
    return std::nullopt;
  return true;
}

constexpr std::optional<bool> wide_or_narrow_of(const ucd::line& one) {
  if (one.count != 1 || !(one.fields[0] == "Wide" || one.fields[0] == "Narrow"))
    return std::nullopt;
  return true;
}

constexpr auto joinings = ucd::property<joining_type, joining_file, &joining_of>;
constexpr auto jamo = ucd::property<bool, hangul_file, &jamo_of>;
constexpr auto wide_or_narrow = ucd::property<bool, decomposition_file, &wide_or_narrow_of>;

// In one byte: Joining_Type in the low three bits, a conjoining jamo in bit
// 3, a Wide or Narrow decomposition in bit 4.
constexpr std::vector<ucd::run<std::uint8_t>> runs() {
  std::vector<ucd::run<std::uint8_t>> joins, jamos, widths;
  for (const auto& one : joinings)
    joins.push_back({one.first, one.last, static_cast<std::uint8_t>(one.value)});
  for (const auto& one : jamo)
    jamos.push_back({one.first, one.last, 8});
  for (const auto& one : wide_or_narrow)
    widths.push_back({one.first, one.last, 16});
  return ucd::merged<std::uint8_t>({joins, jamos, widths});
}

inline constexpr std::size_t blocks =
    ucd::lay_out(runs()).blocks.size() / ucd::block_size;
inline constexpr auto table =
    ucd::two_stage<std::uint8_t, blocks>(ucd::lay_out(runs()));

}  // namespace alef::tables::preparation_data

export namespace alef::tables {

constexpr alef::joining_type joining_type_of(char32_t code_point) noexcept {
  return static_cast<alef::joining_type>(preparation_data::table[code_point] & 7);
}
constexpr bool conjoining_jamo(char32_t code_point) noexcept {
  return (preparation_data::table[code_point] & 8) != 0;
}
constexpr bool wide_or_narrow(char32_t code_point) noexcept {
  return (preparation_data::table[code_point] & 16) != 0;
}

}  // namespace alef::tables
