// SPDX-License-Identifier: AGPL-3.0-only
// The properties the bidirectional algorithm (UAX #9) is decided by, as
// tables read from the UCD while this interface is compiled: Bidi_Class from
// extracted/DerivedBidiClass.txt, with the defaults its @missing lines give
// the code points it does not list; the paired brackets of BidiBrackets.txt;
// and the mirrored glyphs of BidiMirroring.txt.
//
// A module of its own that imports nothing of the library but the reader of
// the UCD, so that it is compiled again when its files or that reader change,
// and not when anything else does.
export module alef.tables.bidi;

import std;
import alef.ucd;

export namespace alef {

// The Bidi_Class property. Left_To_Right first: it is what a code point is
// that neither the file nor its defaults say otherwise of.
enum class bidi_class : std::uint8_t {
  l, r, al, en, es, et, an, cs, nsm, bn, b, s, ws, on,
  lre, lro, rle, rlo, pdf, lri, rli, fsi, pdi,
};

}  // namespace alef

namespace alef::tables::bidi_data {

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc23-extensions"
#endif
constexpr char classes_bytes[] = {
#embed "../../ucd/extracted/DerivedBidiClass.txt"
};
constexpr char brackets_bytes[] = {
#embed "../../ucd/BidiBrackets.txt"
};
constexpr char mirroring_bytes[] = {
#embed "../../ucd/BidiMirroring.txt"
};
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

constexpr ucd::file classes_file{classes_bytes, sizeof classes_bytes};
constexpr ucd::file brackets_file{brackets_bytes, sizeof brackets_bytes};
constexpr ucd::file mirroring_file{mirroring_bytes, sizeof mirroring_bytes};

// The short names of the values, as the data lines have them, and the long
// ones, as the @missing lines have them.
struct name {
  std::string_view abbreviation;
  std::string_view full;
  bidi_class value;
};
inline constexpr name names[] = {
    {"L", "Left_To_Right", bidi_class::l},
    {"R", "Right_To_Left", bidi_class::r},
    {"AL", "Arabic_Letter", bidi_class::al},
    {"EN", "European_Number", bidi_class::en},
    {"ES", "European_Separator", bidi_class::es},
    {"ET", "European_Terminator", bidi_class::et},
    {"AN", "Arabic_Number", bidi_class::an},
    {"CS", "Common_Separator", bidi_class::cs},
    {"NSM", "Nonspacing_Mark", bidi_class::nsm},
    {"BN", "Boundary_Neutral", bidi_class::bn},
    {"B", "Paragraph_Separator", bidi_class::b},
    {"S", "Segment_Separator", bidi_class::s},
    {"WS", "White_Space", bidi_class::ws},
    {"ON", "Other_Neutral", bidi_class::on},
    {"LRE", "Left_To_Right_Embedding", bidi_class::lre},
    {"LRO", "Left_To_Right_Override", bidi_class::lro},
    {"RLE", "Right_To_Left_Embedding", bidi_class::rle},
    {"RLO", "Right_To_Left_Override", bidi_class::rlo},
    {"PDF", "Pop_Directional_Format", bidi_class::pdf},
    {"LRI", "Left_To_Right_Isolate", bidi_class::lri},
    {"RLI", "Right_To_Left_Isolate", bidi_class::rli},
    {"FSI", "First_Strong_Isolate", bidi_class::fsi},
    {"PDI", "Pop_Directional_Isolate", bidi_class::pdi},
};

constexpr std::optional<bidi_class> class_of(const ucd::line& one) {
  if (one.count != 1)
    return std::nullopt;
  for (const name& each : names)
    if (one.fields[0] == each.abbreviation)
      return each.value;
  return std::nullopt;
}

// The @missing lines but the first, which says L of everything: the defaults
// of ranges, which the code points of them the data lines do not list take.
constexpr std::vector<ucd::run<bidi_class>> defaults() {
  std::vector<ucd::run<bidi_class>> out;
  constexpr std::string_view marker = "# @missing:";
  for (std::size_t at = classes_file.find(marker); at != ucd::file::npos;
       at = classes_file.find(marker, at + 1)) {
    std::size_t end = classes_file.find('\n', at);
    if (end == ucd::file::npos)
      end = classes_file.size();
    const std::string_view line = classes_file.substr(at + marker.size(), end - at - marker.size());
    const std::size_t semicolon = line.find(';');
    const std::string_view points = ucd::trim(line.substr(0, semicolon));
    const std::string_view value = ucd::trim(line.substr(semicolon + 1));
    const std::size_t dots = points.find("..");
    const char32_t first = ucd::hex(points.substr(0, dots));
    const char32_t last = ucd::hex(points.substr(dots + 2));
    for (const name& each : names)
      if (value == each.full && !(first == 0 && last == 0x10FFFF))
        out.push_back({first, last, each.value});
  }
  std::ranges::sort(out, {}, &ucd::run<bidi_class>::first);
  return out;
}

// Every code point's class: what the data lines say, and in the gaps between
// them, what the defaults say.
constexpr std::vector<ucd::run<std::uint8_t>> class_runs() {
  const auto& listed = ucd::property<bidi_class, classes_file, &class_of>;
  std::vector<ucd::run<std::uint8_t>> out;
  for (const auto& one : listed)
    if (one.value != bidi_class::l)
      out.push_back({one.first, one.last, static_cast<std::uint8_t>(one.value)});
  std::size_t at = 0;
  for (const auto& range : defaults()) {
    char32_t from = range.first;
    while (at < listed.size() && listed[at].last < from)
      ++at;
    for (std::size_t one = at; one < listed.size() && listed[one].first <= range.last; ++one) {
      if (listed[one].first > from)
        out.push_back({from, listed[one].first - 1, static_cast<std::uint8_t>(range.value)});
      from = std::max(from, static_cast<char32_t>(listed[one].last + 1));
    }
    if (from <= range.last)
      out.push_back({from, range.last, static_cast<std::uint8_t>(range.value)});
  }
  std::ranges::sort(out, {}, &ucd::run<std::uint8_t>::first);
  return out;
}

// BidiBrackets.txt: code; its pair; o or c. BidiMirroring.txt: code; its
// mirrored glyph.
constexpr std::optional<char32_t> pair_of(const ucd::line& one) {
  if (one.count < 2)
    return std::nullopt;
  return ucd::hex(one.fields[0]);
}
constexpr std::optional<bool> opening(const ucd::line& one) {
  if (one.count >= 2 && one.fields[1] == "o")
    return true;
  return std::nullopt;
}
constexpr std::optional<bool> closing(const ucd::line& one) {
  if (one.count >= 2 && one.fields[1] == "c")
    return true;
  return std::nullopt;
}
constexpr std::optional<char32_t> mirror_of(const ucd::line& one) {
  if (one.count < 1)
    return std::nullopt;
  return ucd::hex(one.fields[0]);
}

constexpr auto pairs = ucd::property<char32_t, brackets_file, &pair_of>;
constexpr auto mirrors = ucd::property<char32_t, mirroring_file, &mirror_of>;

inline constexpr std::uint8_t opening_bit = 0x20;
inline constexpr std::uint8_t closing_bit = 0x40;
inline constexpr std::uint8_t mirrored_bit = 0x80;

template <class Ranges>
constexpr std::vector<ucd::run<std::uint8_t>> bits_of(const Ranges& ranges, std::uint8_t bit) {
  std::vector<ucd::run<std::uint8_t>> out;
  for (const auto& one : ranges)
    out.push_back({one.first, one.last, bit});
  return out;
}

constexpr std::vector<ucd::run<std::uint8_t>> runs() {
  return ucd::merged<std::uint8_t>({
      class_runs(),
      bits_of(ucd::property<bool, brackets_file, &opening>, opening_bit),
      bits_of(ucd::property<bool, brackets_file, &closing>, closing_bit),
      bits_of(mirrors, mirrored_bit),
  });
}

inline constexpr std::size_t blocks =
    ucd::lay_out(runs()).blocks.size() / ucd::block_size;
inline constexpr auto table =
    ucd::two_stage<std::uint8_t, blocks>(ucd::lay_out(runs()));

}  // namespace alef::tables::bidi_data

export namespace alef::tables {

// A code point's class, and whether it is an opening or a closing paired
// bracket, or has a mirrored glyph, in one byte.
struct bidi_properties {
  std::uint8_t bits = 0;

  constexpr bidi_class cls() const noexcept { return static_cast<bidi_class>(bits & 0x1F); }
  constexpr bool opening_bracket() const noexcept { return (bits & bidi_data::opening_bit) != 0; }
  constexpr bool closing_bracket() const noexcept { return (bits & bidi_data::closing_bit) != 0; }
  constexpr bool mirrored() const noexcept { return (bits & bidi_data::mirrored_bit) != 0; }
};

constexpr bidi_properties bidi_properties_of(char32_t code_point) noexcept {
  return {bidi_data::table[code_point]};
}

// The other bracket of a paired bracket; 0 where it is not one.
constexpr char32_t paired_bracket(char32_t code_point) noexcept {
  const bidi_properties properties = bidi_properties_of(code_point);
  if (!properties.opening_bracket() && !properties.closing_bracket())
    return 0;
  return ucd::lookup(bidi_data::pairs, code_point, char32_t{0});
}

// The Bidi_Mirroring_Glyph; 0 where there is none.
constexpr char32_t mirroring_glyph(char32_t code_point) noexcept {
  if (!bidi_properties_of(code_point).mirrored())
    return 0;
  return ucd::lookup(bidi_data::mirrors, code_point, char32_t{0});
}

}  // namespace alef::tables
