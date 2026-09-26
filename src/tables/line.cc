// SPDX-License-Identifier: AGPL-3.0-only
// The properties line breaking is decided by (UAX #14), as one table read
// from the UCD while this interface is compiled: Line_Break from
// LineBreak.txt, and what some rules ask besides -- whether a code point is
// East Asian (East_Asian_Width F, W or H), Extended_Pictographic, unassigned,
// initial or final punctuation, or a combining mark -- two bytes a code point in a table of
// two stages.
//
// A module of its own that imports nothing of the library but the reader of
// the UCD, so that it is compiled again when its files or that reader change,
// and not when anything else does.
export module alef.tables.line;

import std;
import alef.ucd;

export namespace alef {

// The Line_Break property. Unknown (XX) first: it is what a code point the
// file does not list is.
enum class line_break : std::uint8_t {
  xx, bk, cr, lf, cm, nl, sg, wj, zw, gl, sp, zwj, b2, ba, bb, hy, cb, cl, cp,
  ex, in, ns, op, qu, is, nu, po, pr, sy, ai, al, cj, eb, em, h2, h3, hl, id,
  jl, jv, jt, ri, sa, ak, ap, as, vf, vi, hh,
};

}  // namespace alef

namespace alef::tables::line_data {

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc23-extensions"
#endif
constexpr char line_break_bytes[] = {
#embed "../../ucd/LineBreak.txt"
};
constexpr char east_asian_bytes[] = {
#embed "../../ucd/EastAsianWidth.txt"
};
constexpr char emoji_bytes[] = {
#embed "../../ucd/emoji/emoji-data.txt"
};
constexpr char category_bytes[] = {
#embed "../../ucd/extracted/DerivedGeneralCategory.txt"
};
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

constexpr ucd::file line_break_file{line_break_bytes, sizeof line_break_bytes};
constexpr ucd::file east_asian_file{east_asian_bytes, sizeof east_asian_bytes};
constexpr ucd::file emoji_file{emoji_bytes, sizeof emoji_bytes};
constexpr ucd::file category_file{category_bytes, sizeof category_bytes};

constexpr std::optional<line_break> class_of(const ucd::line& one) {
  using enum line_break;
  if (one.count != 1)
    return std::nullopt;
  constexpr std::pair<std::string_view, line_break> names[] = {
      {"XX", xx}, {"BK", bk}, {"CR", cr}, {"LF", lf}, {"CM", cm}, {"NL", nl},
      {"SG", sg}, {"WJ", wj}, {"ZW", zw}, {"GL", gl}, {"SP", sp}, {"ZWJ", zwj},
      {"B2", b2}, {"BA", ba}, {"BB", bb}, {"HY", hy}, {"CB", cb}, {"CL", cl},
      {"CP", cp}, {"EX", ex}, {"IN", in}, {"NS", ns}, {"OP", op}, {"QU", qu},
      {"IS", is}, {"NU", nu}, {"PO", po}, {"PR", pr}, {"SY", sy}, {"AI", ai},
      {"AL", al}, {"CJ", cj}, {"EB", eb}, {"EM", em}, {"H2", h2}, {"H3", h3},
      {"HL", hl}, {"ID", id}, {"JL", jl}, {"JV", jv}, {"JT", jt}, {"RI", ri},
      {"SA", sa}, {"AK", ak}, {"AP", ap}, {"AS", as}, {"VF", vf}, {"VI", vi},
      {"HH", hh},
  };
  for (const auto& [name, value] : names)
    if (one.fields[0] == name)
      return value;
  return std::nullopt;
}

constexpr std::optional<bool> east_asian(const ucd::line& one) {
  if (one.count == 1 && (one.fields[0] == "F" || one.fields[0] == "W" || one.fields[0] == "H"))
    return true;
  return std::nullopt;
}
constexpr std::optional<bool> pictographic(const ucd::line& one) {
  if (one.count == 1 && one.fields[0] == "Extended_Pictographic")
    return true;
  return std::nullopt;
}
constexpr std::optional<bool> unassigned(const ucd::line& one) {
  if (one.count == 1 && one.fields[0] == "Cn")
    return true;
  return std::nullopt;
}
constexpr std::optional<bool> initial_punctuation(const ucd::line& one) {
  if (one.count == 1 && one.fields[0] == "Pi")
    return true;
  return std::nullopt;
}
constexpr std::optional<bool> combining_mark(const ucd::line& one) {
  if (one.count == 1 && (one.fields[0] == "Mn" || one.fields[0] == "Mc"))
    return true;
  return std::nullopt;
}
constexpr std::optional<bool> final_punctuation(const ucd::line& one) {
  if (one.count == 1 && one.fields[0] == "Pf")
    return true;
  return std::nullopt;
}

// Two bytes: the class in the low six bits, and above them a bit each.
inline constexpr std::uint16_t east_asian_bit = 0x0100;
inline constexpr std::uint16_t pictographic_bit = 0x0200;
inline constexpr std::uint16_t unassigned_bit = 0x0400;
inline constexpr std::uint16_t initial_bit = 0x0800;
inline constexpr std::uint16_t final_bit = 0x1000;
inline constexpr std::uint16_t mark_bit = 0x2000;

template <class Ranges, class Bits>
constexpr std::vector<ucd::run<std::uint16_t>> runs_of(const Ranges& ranges, Bits bits) {
  std::vector<ucd::run<std::uint16_t>> out;
  for (const auto& one : ranges)
    out.push_back({one.first, one.last, static_cast<std::uint16_t>(bits(one.value))});
  return out;
}

constexpr std::vector<ucd::run<std::uint16_t>> runs() {
  const auto bit = [](std::uint16_t value) { return [value](bool) { return value; }; };
  return ucd::merged<std::uint16_t>({
      runs_of(ucd::property<line_break, line_break_file, &class_of>,
              [](line_break value) { return static_cast<unsigned>(value); }),
      runs_of(ucd::property<bool, east_asian_file, &east_asian>, bit(east_asian_bit)),
      runs_of(ucd::property<bool, emoji_file, &pictographic>, bit(pictographic_bit)),
      runs_of(ucd::property<bool, category_file, &unassigned>, bit(unassigned_bit)),
      runs_of(ucd::property<bool, category_file, &initial_punctuation>, bit(initial_bit)),
      runs_of(ucd::property<bool, category_file, &final_punctuation>, bit(final_bit)),
      runs_of(ucd::property<bool, category_file, &combining_mark>, bit(mark_bit)),
  });
}

inline constexpr std::size_t blocks =
    ucd::lay_out(runs()).blocks.size() / ucd::block_size;
inline constexpr auto table =
    ucd::two_stage<std::uint16_t, blocks>(ucd::lay_out(runs()));

}  // namespace alef::tables::line_data

export namespace alef::tables {

// What the rules ask of a code point, in two bytes.
struct line_properties {
  std::uint16_t bits = 0;

  constexpr alef::line_break line_break() const noexcept {
    return static_cast<alef::line_break>(bits & 0x3F);
  }
  constexpr bool east_asian() const noexcept { return (bits & line_data::east_asian_bit) != 0; }
  constexpr bool pictographic() const noexcept { return (bits & line_data::pictographic_bit) != 0; }
  constexpr bool unassigned() const noexcept { return (bits & line_data::unassigned_bit) != 0; }
  constexpr bool initial_punctuation() const noexcept { return (bits & line_data::initial_bit) != 0; }
  constexpr bool final_punctuation() const noexcept { return (bits & line_data::final_bit) != 0; }
  // Mn or Mc, which is what makes a character of class SA a combining mark.
  constexpr bool combining_mark() const noexcept { return (bits & line_data::mark_bit) != 0; }
};

constexpr line_properties line_properties_of(char32_t code_point) noexcept {
  return {line_data::table[code_point]};
}

}  // namespace alef::tables
