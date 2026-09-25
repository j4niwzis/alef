// The properties identifiers are made of, as one table read from the UCD
// while this interface is compiled: XID_Start, XID_Continue and
// Default_Ignorable_Code_Point from DerivedCoreProperties.txt, one byte a code point in a table of two stages.
//
// A module of its own that imports nothing of the library but the reader of
// the UCD, so that it is compiled again when its file or that reader change,
// and not when anything else does.
export module alef.tables.identifier;

import std;
import alef.ucd;

namespace alef::tables::identifier_data {

// #embed is C23, and in C++ an extension until C++26 has it. Said here rather
// than with a flag, so that whoever compiles this interface -- an importer of
// an installed alef among them -- needs no flag to do it quietly.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc23-extensions"
#endif
constexpr char derived_core_bytes[] = {
#embed "../../ucd/DerivedCoreProperties.txt"
};
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

constexpr ucd::file derived_core_file{derived_core_bytes, sizeof derived_core_bytes};

constexpr std::optional<bool> xid_start_of(const ucd::line& one) {
  if (one.count != 1 || one.fields[0] != "XID_Start")
    return std::nullopt;
  return true;
}
constexpr std::optional<bool> xid_continue_of(const ucd::line& one) {
  if (one.count != 1 || one.fields[0] != "XID_Continue")
    return std::nullopt;
  return true;
}

constexpr std::optional<bool> ignorable_of(const ucd::line& one) {
  if (one.count != 1 || one.fields[0] != "Default_Ignorable_Code_Point")
    return std::nullopt;
  return true;
}

constexpr auto ignorable = ucd::property<bool, derived_core_file, &ignorable_of>;
constexpr auto xid_start = ucd::property<bool, derived_core_file, &xid_start_of>;
constexpr auto xid_continue = ucd::property<bool, derived_core_file, &xid_continue_of>;

// In one byte: XID_Start in bit 0, XID_Continue in bit 1,
// Default_Ignorable_Code_Point in bit 2.
constexpr std::vector<ucd::run<std::uint8_t>> runs() {
  std::vector<ucd::run<std::uint8_t>> starts;
  std::vector<ucd::run<std::uint8_t>> continues;
  for (const auto& one : xid_start)
    starts.push_back({one.first, one.last, 1});
  for (const auto& one : xid_continue)
    continues.push_back({one.first, one.last, 2});
  std::vector<ucd::run<std::uint8_t>> ignorables;
  for (const auto& one : ignorable)
    ignorables.push_back({one.first, one.last, 4});
  return ucd::merged<std::uint8_t>({starts, continues, ignorables});
}

inline constexpr std::size_t blocks =
    ucd::lay_out(runs()).blocks.size() / ucd::block_size;
inline constexpr auto table =
    ucd::two_stage<std::uint8_t, blocks>(ucd::lay_out(runs()));

}  // namespace alef::tables::identifier_data

export namespace alef::tables {

constexpr bool xid_start(char32_t code_point) noexcept {
  return (identifier_data::table[code_point] & 1) != 0;
}
constexpr bool xid_continue(char32_t code_point) noexcept {
  return (identifier_data::table[code_point] & 2) != 0;
}
constexpr bool default_ignorable(char32_t code_point) noexcept {
  return (identifier_data::table[code_point] & 4) != 0;
}

}  // namespace alef::tables
