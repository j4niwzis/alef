// The properties word boundaries are decided by, as one table read from the
// UCD while this interface is compiled: Word_Break from
// WordBreakProperty.txt and Extended_Pictographic from emoji-data.txt, one
// byte a code point in a table of two stages.
//
// A module of its own that imports nothing of the library but the reader of
// the UCD, so that it is compiled again when its files or that reader
// change, and not when anything else does.
export module alef.tables.word;

import std;
import alef.ucd;

export namespace alef {

// The Word_Break property, and a code point's value of it.
enum class word_break : std::uint8_t {
  other,
  cr,
  lf,
  newline,
  extend,
  zwj,
  regional_indicator,
  format,
  katakana,
  hebrew_letter,
  aletter,
  single_quote,
  double_quote,
  midnumlet,
  midletter,
  midnum,
  numeric,
  extendnumlet,
  wsegspace,
};

}  // namespace alef

namespace alef::tables::word_data {

// #embed is C23, and in C++ an extension until C++26 has it. Said here rather
// than with a flag, so that whoever compiles this interface -- an importer of
// an installed alef among them -- needs no flag to do it quietly.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc23-extensions"
#endif
constexpr char word_break_bytes[] = {
#embed "../../ucd/auxiliary/WordBreakProperty.txt"
};
constexpr char emoji_bytes[] = {
#embed "../../ucd/emoji/emoji-data.txt"
};
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

constexpr ucd::file word_break_file{word_break_bytes, sizeof word_break_bytes};
constexpr ucd::file emoji_file{emoji_bytes, sizeof emoji_bytes};

constexpr std::optional<word_break> word_break_of(const ucd::line& one) {
  using enum word_break;
  if (one.count != 1)
    return std::nullopt;
  constexpr std::pair<std::string_view, word_break> names[] = {
      {"CR", cr},
      {"LF", lf},
      {"Newline", newline},
      {"Extend", extend},
      {"ZWJ", zwj},
      {"Regional_Indicator", regional_indicator},
      {"Format", format},
      {"Katakana", katakana},
      {"Hebrew_Letter", hebrew_letter},
      {"ALetter", aletter},
      {"Single_Quote", single_quote},
      {"Double_Quote", double_quote},
      {"MidNumLet", midnumlet},
      {"MidLetter", midletter},
      {"MidNum", midnum},
      {"Numeric", numeric},
      {"ExtendNumLet", extendnumlet},
      {"WSegSpace", wsegspace},
  };
  for (const auto& [name, value] : names)
    if (one.fields[0] == name)
      return value;
  return std::nullopt;
}

constexpr std::optional<bool> pictographic_of(const ucd::line& one) {
  if (one.count != 1 || one.fields[0] != "Extended_Pictographic")
    return std::nullopt;
  return true;
}

constexpr auto word_breaks =
    ucd::property<word_break, word_break_file, &word_break_of>;
constexpr auto pictographic =
    ucd::property<bool, emoji_file, &pictographic_of>;

// Both in one byte: Word_Break in the low five bits, and whether it is
// Extended_Pictographic in the one above them.
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
      runs_of(word_breaks,
              [](word_break value) { return static_cast<unsigned>(value); }),
      runs_of(pictographic, [](bool) { return 0x20u; }),
  });
}

inline constexpr std::size_t blocks =
    ucd::lay_out(runs()).blocks.size() / ucd::block_size;
inline constexpr auto table =
    ucd::two_stage<std::uint8_t, blocks>(ucd::lay_out(runs()));

}  // namespace alef::tables::word_data

export namespace alef::tables {

// What the rules ask of a code point, in one byte.
struct word_properties {
  std::uint8_t bits = 0;

  constexpr alef::word_break word_break() const noexcept {
    return static_cast<alef::word_break>(bits & 0x1F);
  }
  constexpr bool pictographic() const noexcept { return (bits & 0x20) != 0; }
};

constexpr word_properties word_properties_of(char32_t code_point) noexcept {
  return {word_data::table[code_point]};
}

}  // namespace alef::tables
