// SPDX-License-Identifier: AGPL-3.0-only
// NFKC_Casefold: each code point's mapping, read from the NFKC_CF lines of
// DerivedNormalizationProps.txt while this interface is compiled, for
// Unicode 18.0.0. A code point it does not list maps to itself.
//
// A module of its own that imports nothing of the library, so that it is
// compiled again when its file changes, and not when anything else does.
export module alef.tables.nfkc_casefold;

import std;

namespace alef::tables::nfkc_casefold_data {

// #embed is C23, and in C++ an extension until C++26 has it. Said here rather
// than with a flag, so that whoever compiles this interface -- an importer of
// an installed alef among them -- needs no flag to do it quietly.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc23-extensions"
#endif
constexpr char props_bytes[] = {
#embed "../../ucd/DerivedNormalizationProps.txt"
};
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

struct entry {
  char32_t first = 0;
  char32_t last = 0;
  std::uint32_t at = 0;
  std::uint8_t length = 0;
};

struct parsed {
  std::vector<entry> entries;
  std::vector<char32_t> pool;
};

constexpr bool blank(char one) { return one == ' ' || one == '\t' || one == '\r'; }

constexpr std::string_view trimmed(std::string_view text) {
  while (!text.empty() && blank(text.front()))
    text.remove_prefix(1);
  while (!text.empty() && blank(text.back()))
    text.remove_suffix(1);
  return text;
}

constexpr char32_t hex(std::string_view digits) {
  char32_t value = 0;
  for (const char digit : digits)
    value = value * 16 + static_cast<char32_t>(digit <= '9' ? digit - '0' : (digit | 0x20) - 'a' + 10);
  return value;
}

// A line: code point or range ; NFKC_CF ; its mapping, maybe empty # comment.
constexpr parsed parse() {
  const std::string_view file(props_bytes, sizeof props_bytes);
  parsed out;
  for (std::size_t start = 0; start < file.size();) {
    std::size_t end = file.find('\n', start);
    if (end == std::string_view::npos)
      end = file.size();
    std::string_view line = file.substr(start, end - start);
    start = end + 1;
    line = line.substr(0, line.find('#'));
    const std::size_t one = line.find(';');
    if (one == std::string_view::npos)
      continue;
    const std::size_t two = line.find(';', one + 1);
    if (two == std::string_view::npos || trimmed(line.substr(one + 1, two - one - 1)) != "NFKC_CF")
      continue;
    const std::string_view range = trimmed(line.substr(0, one));
    const std::size_t dots = range.find("..");
    entry row;
    row.first = hex(range.substr(0, dots));
    row.last = dots == std::string_view::npos ? row.first : hex(range.substr(dots + 2));
    row.at = static_cast<std::uint32_t>(out.pool.size());
    std::string_view mapping = trimmed(line.substr(two + 1));
    while (!mapping.empty()) {
      std::size_t stop = 0;
      while (stop < mapping.size() && !blank(mapping[stop]))
        ++stop;
      out.pool.push_back(hex(mapping.substr(0, stop)));
      ++row.length;
      mapping = trimmed(mapping.substr(stop));
    }
    out.entries.push_back(row);
  }
  std::ranges::sort(out.entries, {}, &entry::first);
  return out;
}

inline constexpr std::size_t entry_count = parse().entries.size();
inline constexpr std::size_t pool_size = parse().pool.size();

struct laid_out {
  std::array<entry, entry_count> entries{};
  std::array<char32_t, pool_size> pool{};
};

inline constexpr laid_out table = [] {
  laid_out out;
  const parsed all = parse();
  std::ranges::copy(all.entries, out.entries.begin());
  std::ranges::copy(all.pool, out.pool.begin());
  return out;
}();

}  // namespace alef::tables::nfkc_casefold_data

export namespace alef::tables {

// A code point's NFKC_Casefold mapping, or nothing where it maps to itself.
// A mapping may be empty: the code point is removed.
constexpr std::optional<std::span<const char32_t>> nfkc_casefold_of(char32_t code_point) noexcept {
  using nfkc_casefold_data::entry;
  const auto& all = nfkc_casefold_data::table.entries;
  const auto after = std::ranges::upper_bound(all, code_point, {}, &entry::first);
  if (after == all.begin() || code_point > std::prev(after)->last)
    return std::nullopt;
  return std::span<const char32_t>(nfkc_casefold_data::table.pool.data() + std::prev(after)->at,
                                   std::prev(after)->length);
}

}  // namespace alef::tables
