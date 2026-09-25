// The East_Asian_Width property, as a table read from the UCD while this
// interface is compiled: from EastAsianWidth.txt, one byte a code point in a
// table of two stages.
//
// A module of its own that imports nothing of the library but the reader of
// the UCD, so that it is compiled again when its file or that reader change,
// and not when anything else does.
export module alef.tables.east_asian_width;

import std;
import alef.ucd;

export namespace alef {

// The East_Asian_Width property (UAX #11). Neutral first: it is what a code
// point the file does not list is.
enum class east_asian_width : std::uint8_t {
  neutral,    // N
  ambiguous,  // A
  halfwidth,  // H
  wide,       // W
  fullwidth,  // F
  narrow,     // Na
};

}  // namespace alef

namespace alef::tables::east_asian_width_data {

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc23-extensions"
#endif
constexpr char width_bytes[] = {
#embed "../../ucd/EastAsianWidth.txt"
};
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

constexpr ucd::file width_file{width_bytes, sizeof width_bytes};

constexpr std::optional<east_asian_width> width_of(const ucd::line& one) {
  using enum east_asian_width;
  if (one.count != 1)
    return std::nullopt;
  constexpr std::pair<std::string_view, east_asian_width> codes[] = {
      {"N", neutral}, {"A", ambiguous}, {"H", halfwidth},
      {"W", wide},    {"F", fullwidth}, {"Na", narrow},
  };
  for (const auto& [code, value] : codes)
    if (one.fields[0] == code)
      return value;
  return std::nullopt;
}

constexpr auto widths = ucd::property<east_asian_width, width_file, &width_of>;

constexpr std::vector<ucd::run<east_asian_width>> runs() {
  std::vector<ucd::run<east_asian_width>> out;
  for (const auto& one : widths)
    if (one.value != east_asian_width::neutral) {
      if (!out.empty() && out.back().last + 1 == one.first &&
          out.back().value == one.value)
        out.back().last = one.last;
      else
        out.push_back({one.first, one.last, one.value});
    }
  return out;
}

inline constexpr std::size_t blocks =
    ucd::lay_out(runs()).blocks.size() / ucd::block_size;
inline constexpr auto table =
    ucd::two_stage<east_asian_width, blocks>(ucd::lay_out(runs()));

}  // namespace alef::tables::east_asian_width_data

export namespace alef::tables {

constexpr east_asian_width east_asian_width_of(char32_t code_point) noexcept {
  return east_asian_width_data::table[code_point];
}

}  // namespace alef::tables
