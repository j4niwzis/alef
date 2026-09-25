// The General_Category property, as a table read from the UCD while this
// interface is compiled: from extracted/DerivedGeneralCategory.txt, one byte
// a code point in a table of two stages.
//
// A module of its own that imports nothing of the library but the reader of
// the UCD, so that it is compiled again when its file or that reader change,
// and not when anything else does.
export module alef.tables.general_category;

import std;
import alef.ucd;

export namespace alef {

// The General_Category property. Unassigned first: it is what a code point
// the file does not list is.
enum class general_category : std::uint8_t {
  unassigned,             // Cn
  uppercase_letter,       // Lu
  lowercase_letter,       // Ll
  titlecase_letter,       // Lt
  modifier_letter,        // Lm
  other_letter,           // Lo
  nonspacing_mark,        // Mn
  spacing_mark,           // Mc
  enclosing_mark,         // Me
  decimal_number,         // Nd
  letter_number,          // Nl
  other_number,           // No
  connector_punctuation,  // Pc
  dash_punctuation,       // Pd
  open_punctuation,       // Ps
  close_punctuation,      // Pe
  initial_punctuation,    // Pi
  final_punctuation,      // Pf
  other_punctuation,      // Po
  math_symbol,            // Sm
  currency_symbol,        // Sc
  modifier_symbol,        // Sk
  other_symbol,           // So
  space_separator,        // Zs
  line_separator,         // Zl
  paragraph_separator,    // Zp
  control,                // Cc
  format,                 // Cf
  surrogate,              // Cs
  private_use,            // Co
};

}  // namespace alef

namespace alef::tables::general_category_data {

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc23-extensions"
#endif
constexpr char category_bytes[] = {
#embed "../../ucd/extracted/DerivedGeneralCategory.txt"
};
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

constexpr ucd::file category_file{category_bytes, sizeof category_bytes};

constexpr std::optional<general_category> category_of(const ucd::line& one) {
  using enum general_category;
  if (one.count != 1)
    return std::nullopt;
  constexpr std::pair<std::string_view, general_category> codes[] = {
      {"Cn", unassigned},
      {"Lu", uppercase_letter},
      {"Ll", lowercase_letter},
      {"Lt", titlecase_letter},
      {"Lm", modifier_letter},
      {"Lo", other_letter},
      {"Mn", nonspacing_mark},
      {"Mc", spacing_mark},
      {"Me", enclosing_mark},
      {"Nd", decimal_number},
      {"Nl", letter_number},
      {"No", other_number},
      {"Pc", connector_punctuation},
      {"Pd", dash_punctuation},
      {"Ps", open_punctuation},
      {"Pe", close_punctuation},
      {"Pi", initial_punctuation},
      {"Pf", final_punctuation},
      {"Po", other_punctuation},
      {"Sm", math_symbol},
      {"Sc", currency_symbol},
      {"Sk", modifier_symbol},
      {"So", other_symbol},
      {"Zs", space_separator},
      {"Zl", line_separator},
      {"Zp", paragraph_separator},
      {"Cc", control},
      {"Cf", format},
      {"Cs", surrogate},
      {"Co", private_use},
  };
  for (const auto& [code, value] : codes)
    if (one.fields[0] == code)
      return value;
  return std::nullopt;
}

constexpr auto categories =
    ucd::property<general_category, category_file, &category_of>;

constexpr std::vector<ucd::run<general_category>> runs() {
  std::vector<ucd::run<general_category>> out;
  for (const auto& one : categories)
    if (one.value != general_category::unassigned)
      out.push_back({one.first, one.last, one.value});
  return out;
}

inline constexpr std::size_t blocks =
    ucd::lay_out(runs()).blocks.size() / ucd::block_size;
inline constexpr auto table =
    ucd::two_stage<general_category, blocks>(ucd::lay_out(runs()));

}  // namespace alef::tables::general_category_data

export namespace alef::tables {

constexpr general_category general_category_of(char32_t code_point) noexcept {
  return general_category_data::table[code_point];
}

}  // namespace alef::tables
