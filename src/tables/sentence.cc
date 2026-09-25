// The property sentence boundaries are decided by, as one table read from the
// UCD while this interface is compiled: Sentence_Break from
// SentenceBreakProperty.txt, one byte a code point in a table of two stages.
//
// A module of its own that imports nothing of the library but the reader of
// the UCD, so that it is compiled again when its file or that reader change,
// and not when anything else does.
export module alef.tables.sentence;

import std;
import alef.ucd;

export namespace alef {

// The Sentence_Break property, and a code point's value of it.
enum class sentence_break : std::uint8_t {
  other,
  cr,
  lf,
  extend,
  sep,
  format,
  sp,
  lower,
  upper,
  oletter,
  numeric,
  aterm,
  scontinue,
  sterm,
  close,
};

}  // namespace alef

namespace alef::tables::sentence_data {

// #embed is C23, and in C++ an extension until C++26 has it. Said here rather
// than with a flag, so that whoever compiles this interface -- an importer of
// an installed alef among them -- needs no flag to do it quietly.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc23-extensions"
#endif
constexpr char sentence_break_bytes[] = {
#embed "../../ucd/auxiliary/SentenceBreakProperty.txt"
};
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

constexpr ucd::file sentence_break_file{sentence_break_bytes, sizeof sentence_break_bytes};

constexpr std::optional<sentence_break> sentence_break_of(const ucd::line& one) {
  using enum sentence_break;
  if (one.count != 1)
    return std::nullopt;
  constexpr std::pair<std::string_view, sentence_break> names[] = {
      {"CR", cr},         {"LF", lf},           {"Extend", extend},
      {"Sep", sep},       {"Format", format},   {"Sp", sp},
      {"Lower", lower},   {"Upper", upper},     {"OLetter", oletter},
      {"Numeric", numeric}, {"ATerm", aterm},   {"SContinue", scontinue},
      {"STerm", sterm},   {"Close", close},
  };
  for (const auto& [name, value] : names)
    if (one.fields[0] == name)
      return value;
  return std::nullopt;
}

constexpr auto sentence_breaks =
    ucd::property<sentence_break, sentence_break_file, &sentence_break_of>;

constexpr std::vector<ucd::run<std::uint8_t>> runs() {
  std::vector<ucd::run<std::uint8_t>> out;
  for (const auto& one : sentence_breaks)
    out.push_back({one.first, one.last, static_cast<std::uint8_t>(one.value)});
  return ucd::merged<std::uint8_t>({out});
}

inline constexpr std::size_t blocks =
    ucd::lay_out(runs()).blocks.size() / ucd::block_size;
inline constexpr auto table =
    ucd::two_stage<std::uint8_t, blocks>(ucd::lay_out(runs()));

}  // namespace alef::tables::sentence_data

export namespace alef::tables {

constexpr alef::sentence_break sentence_break_of(char32_t code_point) noexcept {
  return static_cast<alef::sentence_break>(sentence_data::table[code_point]);
}

}  // namespace alef::tables
