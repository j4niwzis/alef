// The tables case mapping is done by, read from the UCD while this interface
// is compiled: the simple mappings of UnicodeData.txt, the unconditional
// full mappings of SpecialCasing.txt, the folding of CaseFolding.txt, and
// Cased, Case_Ignorable and Soft_Dotted from DerivedCoreProperties.txt and
// PropList.txt, which the contexts of the conditional mappings are made of.
//
// A module of its own that imports nothing of the library but the reader of
// the UCD, so that it is compiled again when its files or that reader change,
// and not when anything else does.
export module alef.tables.casing;

import std;
import alef.ucd;

namespace alef::tables::casing_data {

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc23-extensions"
#endif
constexpr char unicode_data_bytes[] = {
#embed "../../ucd/UnicodeData.txt"
};
constexpr char special_casing_bytes[] = {
#embed "../../ucd/SpecialCasing.txt"
};
constexpr char case_folding_bytes[] = {
#embed "../../ucd/CaseFolding.txt"
};
constexpr char derived_core_bytes[] = {
#embed "../../ucd/DerivedCoreProperties.txt"
};
constexpr char prop_list_bytes[] = {
#embed "../../ucd/PropList.txt"
};
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

constexpr ucd::file unicode_data{unicode_data_bytes, sizeof unicode_data_bytes};
constexpr ucd::file special_casing{special_casing_bytes, sizeof special_casing_bytes};
constexpr ucd::file case_folding{case_folding_bytes, sizeof case_folding_bytes};
constexpr ucd::file derived_core{derived_core_bytes, sizeof derived_core_bytes};
constexpr ucd::file prop_list{prop_list_bytes, sizeof prop_list_bytes};

inline constexpr std::uint8_t cased_bit = 0x01;
inline constexpr std::uint8_t case_ignorable_bit = 0x02;
inline constexpr std::uint8_t soft_dotted_bit = 0x04;
inline constexpr std::uint8_t mapped_bit = 0x08;

// A code point's mappings: the simple ones, one code point each, 0 where it
// maps to itself; and where a full mapping is not the simple one, the number
// of its special, from 1.
struct mapping {
  char32_t code_point = 0;
  char32_t lower = 0;
  char32_t upper = 0;
  char32_t title = 0;
  char32_t fold = 0;
  char32_t turkic_fold = 0;
  std::uint16_t special = 0;
};

// Full mappings, as slices of the pool; a length of 0 is the simple mapping.
struct slice {
  std::uint16_t offset = 0;
  std::uint8_t length = 0;
};
struct special {
  slice lower;
  slice upper;
  slice title;
  slice fold;
};

struct read_out {
  std::vector<mapping> mappings;
  std::vector<special> specials;
  std::vector<char32_t> pool;
  std::vector<std::vector<ucd::run<std::uint8_t>>> flags;
};

constexpr std::vector<char32_t> code_points(std::string_view text) {
  std::vector<char32_t> out;
  for (std::size_t start = text.find_first_not_of(' '); start != std::string_view::npos;) {
    std::size_t stop = text.find(' ', start);
    if (stop == std::string_view::npos)
      stop = text.size();
    out.push_back(ucd::hex(text.substr(start, stop - start)));
    start = text.find_first_not_of(' ', stop);
  }
  return out;
}

// The fields of a line split by ';', trimmed, up to `count` of them.
constexpr std::vector<std::string_view> fields_of(std::string_view row, std::size_t count) {
  std::vector<std::string_view> out;
  std::size_t from = 0;
  while (out.size() < count) {
    const std::size_t semicolon = row.find(';', from);
    out.push_back(ucd::trim(row.substr(from, semicolon - from)));
    if (semicolon == std::string_view::npos)
      break;
    from = semicolon + 1;
  }
  return out;
}

constexpr read_out read() {
  read_out out;
  const auto entry = [&](char32_t code_point) -> mapping& {
    const auto found = std::ranges::lower_bound(out.mappings, code_point, {},
                                                &mapping::code_point);
    if (found != out.mappings.end() && found->code_point == code_point)
      return *found;
    return *out.mappings.insert(found, mapping{code_point});
  };
  const auto special_of = [&](mapping& one) -> special& {
    if (one.special == 0) {
      out.specials.emplace_back();
      one.special = static_cast<std::uint16_t>(out.specials.size());
    }
    return out.specials[one.special - 1];
  };
  const auto kept = [&](const std::vector<char32_t>& full) {
    const slice made{static_cast<std::uint16_t>(out.pool.size()),
                     static_cast<std::uint8_t>(full.size())};
    out.pool.insert(out.pool.end(), full.begin(), full.end());
    return made;
  };

  // UnicodeData.txt: fields 12, 13 and 14 are the simple uppercase,
  // lowercase and titlecase mappings. A line that has none is passed over
  // without its code point being read.
  const ucd::file text = unicode_data;
  for (std::size_t at = 0; at < text.size();) {
    std::size_t end = text.find('\n', at);
    if (end == ucd::file::npos)
      end = text.size();
    const std::string_view row = text.substr(at, end - at);
    at = end + 1;
    std::size_t twelfth = 0;
    for (int field = 0; field < 12 && twelfth != ucd::file::npos; ++field)
      twelfth = row.find(';', twelfth == 0 && field == 0 ? 0 : twelfth + 1);
    if (twelfth == ucd::file::npos)
      continue;
    const std::string_view rest = row.substr(twelfth + 1);
    if (rest == ";;" || rest == ";;\r")
      continue;
    const std::vector<std::string_view> maps = fields_of(rest, 3);
    mapping& one = entry(ucd::hex(row.substr(0, row.find(';'))));
    if (maps.size() > 0 && !maps[0].empty())
      one.upper = ucd::hex(maps[0]);
    if (maps.size() > 1 && !maps[1].empty())
      one.lower = ucd::hex(maps[1]);
    if (maps.size() > 2 && !maps[2].empty())
      one.title = ucd::hex(maps[2]);
  }

  // SpecialCasing.txt: code; lower; title; upper; and a condition, where
  // there is one -- those are said in code, where their contexts are.
  for (std::size_t at = 0; at < special_casing.size();) {
    std::size_t end = special_casing.find('\n', at);
    if (end == ucd::file::npos)
      end = special_casing.size();
    std::string_view row = special_casing.substr(at, end - at);
    at = end + 1;
    if (row.empty() || row.front() == '#')
      continue;
    row = row.substr(0, row.find('#'));
    const std::vector<std::string_view> fields = fields_of(row, 5);
    if (fields.size() < 4 || (fields.size() > 4 && !fields[4].empty()))
      continue;
    mapping& one = entry(ucd::hex(fields[0]));
    special& full = special_of(one);
    full.lower = kept(code_points(fields[1]));
    full.title = kept(code_points(fields[2]));
    full.upper = kept(code_points(fields[3]));
  }

  // CaseFolding.txt: C and S are the simple folding, C and F the full one,
  // and T the Turkic one.
  ucd::each_line(case_folding, [&](const ucd::line& one) {
    if (one.count < 2)
      return;
    const std::string_view status = one.fields[0];
    const std::vector<char32_t> folded = code_points(one.fields[1]);
    mapping& found = entry(one.first());
    if (status == "C" || status == "S")
      found.fold = folded.front();
    if (status == "F")
      special_of(found).fold = kept(folded);
    if (status == "T")
      found.turkic_fold = folded.front();
  });

  // The flags.
  out.flags.resize(4);
  ucd::each_line(derived_core, [&](const ucd::line& one) {
    if (one.count != 1)
      return;
    if (one.fields[0] == "Cased")
      out.flags[0].push_back({one.first(), one.last(), cased_bit});
    else if (one.fields[0] == "Case_Ignorable")
      out.flags[1].push_back({one.first(), one.last(), case_ignorable_bit});
  });
  ucd::each_line(prop_list, [&](const ucd::line& one) {
    if (one.count == 1 && one.fields[0] == "Soft_Dotted")
      out.flags[2].push_back({one.first(), one.last(), soft_dotted_bit});
  });
  for (const mapping& one : out.mappings)
    out.flags[3].push_back({one.code_point, one.code_point, mapped_bit});
  for (auto& list : out.flags)
    std::ranges::sort(list, {}, &ucd::run<std::uint8_t>::first);
  return out;
}

struct sizes {
  std::size_t blocks = 0;
  std::size_t mappings = 0;
  std::size_t specials = 0;
  std::size_t pool = 0;
};

inline constexpr sizes sized = [] {
  const read_out data = read();
  return sizes{ucd::lay_out(ucd::merged(data.flags)).blocks.size() / ucd::block_size,
               data.mappings.size(), data.specials.size(), data.pool.size()};
}();

struct all_tables {
  ucd::two_stage_table<std::uint8_t, sized.blocks> flags;
  std::array<mapping, sized.mappings> mappings;
  std::array<special, sized.specials> specials;
  std::array<char32_t, sized.pool> pool;
};

inline constexpr all_tables table = [] {
  const read_out data = read();
  all_tables out{};
  out.flags = ucd::two_stage<std::uint8_t, sized.blocks>(ucd::lay_out(ucd::merged(data.flags)));
  std::ranges::copy(data.mappings, out.mappings.begin());
  std::ranges::copy(data.specials, out.specials.begin());
  std::ranges::copy(data.pool, out.pool.begin());
  return out;
}();

constexpr const mapping* mapping_of(char32_t code_point) noexcept {
  if ((table.flags[code_point] & mapped_bit) == 0)
    return nullptr;
  const auto found = std::ranges::lower_bound(table.mappings, code_point, {},
                                              &mapping::code_point);
  return found != table.mappings.end() && found->code_point == code_point ? &*found
                                                                          : nullptr;
}

}  // namespace alef::tables::casing_data

export namespace alef::tables {

// Which of the mappings.
enum class casing : std::uint8_t { lower, upper, title, fold };

constexpr bool is_cased(char32_t code_point) noexcept {
  return (casing_data::table.flags[code_point] & casing_data::cased_bit) != 0;
}
constexpr bool is_case_ignorable(char32_t code_point) noexcept {
  return (casing_data::table.flags[code_point] & casing_data::case_ignorable_bit) != 0;
}
constexpr bool is_soft_dotted(char32_t code_point) noexcept {
  return (casing_data::table.flags[code_point] & casing_data::soft_dotted_bit) != 0;
}

// The simple mapping: one code point, itself where it has none.
constexpr char32_t simple(casing which, char32_t code_point) noexcept {
  const casing_data::mapping* one = casing_data::mapping_of(code_point);
  if (one == nullptr)
    return code_point;
  const char32_t mapped = which == casing::lower   ? one->lower
                          : which == casing::upper ? one->upper
                          : which == casing::title ? one->title
                                                   : one->fold;
  return mapped != 0 ? mapped : code_point;
}

// The Turkic folding of the two code points that have one, and the simple
// folding of every other.
constexpr char32_t turkic_fold(char32_t code_point) noexcept {
  const casing_data::mapping* one = casing_data::mapping_of(code_point);
  if (one != nullptr && one->turkic_fold != 0)
    return one->turkic_fold;
  return simple(casing::fold, code_point);
}

// The unconditional full mapping, where it is not the simple one; nothing
// where it is.
constexpr std::u32string_view full(casing which, char32_t code_point) noexcept {
  const casing_data::mapping* one = casing_data::mapping_of(code_point);
  if (one == nullptr || one->special == 0)
    return {};
  const casing_data::special& found = casing_data::table.specials[one->special - 1];
  const casing_data::slice& part = which == casing::lower   ? found.lower
                                   : which == casing::upper ? found.upper
                                   : which == casing::title ? found.title
                                                            : found.fold;
  if (part.length == 0)
    return {};
  return {casing_data::table.pool.data() + part.offset, part.length};
}

}  // namespace alef::tables
