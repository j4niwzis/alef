// The tables normalization is done by, read from the UCD while this interface
// is compiled: the Canonical_Combining_Class and the decomposition mappings
// from UnicodeData.txt, and the quick checks and Full_Composition_Exclusion
// from DerivedNormalizationProps.txt.
//
// A module of its own that imports nothing of the library but the reader of
// the UCD, so that it is compiled again when its files or that reader change,
// and not when anything else does.
export module alef.tables.normalization;

import std;
import alef.ucd;

export namespace alef::tables {

// Whether text is in a normalization form, as far as one code point says:
// yes, no, or maybe -- which only normalizing it answers.
enum class quick_check : std::uint8_t { yes, no, maybe };

// NFD_QC, NFKD_QC, NFC_QC and NFKC_QC of one code point, in one byte.
struct quick_checks {
  std::uint8_t bits = 0;

  constexpr quick_check nfd() const noexcept {
    return (bits & 0x01) != 0 ? quick_check::no : quick_check::yes;
  }
  constexpr quick_check nfkd() const noexcept {
    return (bits & 0x02) != 0 ? quick_check::no : quick_check::yes;
  }
  constexpr quick_check nfc() const noexcept {
    return static_cast<quick_check>((bits >> 2) & 0x03);
  }
  constexpr quick_check nfkc() const noexcept {
    return static_cast<quick_check>((bits >> 4) & 0x03);
  }
};

}  // namespace alef::tables

namespace alef::tables::normalization_data {

// #embed is C23, and in C++ an extension until C++26 has it. Said here rather
// than with a flag, so that whoever compiles this interface -- an importer of
// an installed alef among them -- needs no flag to do it quietly.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc23-extensions"
#endif
constexpr char unicode_data_bytes[] = {
#embed "../../ucd/UnicodeData.txt"
};
constexpr char normalization_props_bytes[] = {
#embed "../../ucd/DerivedNormalizationProps.txt"
};
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

constexpr ucd::file unicode_data_file{unicode_data_bytes,
                                      sizeof unicode_data_bytes};
constexpr ucd::file normalization_props_file{normalization_props_bytes,
                                             sizeof normalization_props_bytes};

// A decomposition mapping as UnicodeData.txt gives it: one level deep.
struct mapping {
  char32_t code_point = 0;
  bool compatibility = false;
  std::uint32_t offset = 0;  // into unicode_data::pool
  std::uint8_t length = 0;
};

// What normalization reads of UnicodeData.txt: the combining classes that
// are not 0, and the decomposition mappings, in the order of the file.
struct unicode_data {
  std::vector<ucd::run<std::uint8_t>> classes;
  std::vector<mapping> mappings;
  std::vector<char32_t> pool;
};

constexpr std::uint8_t decimal(std::string_view digits) noexcept {
  unsigned value = 0;
  for (const char digit : digits)
    value = value * 10 + static_cast<unsigned>(digit - '0');
  return static_cast<std::uint8_t>(value);
}

// A line is fifteen fields split by ';': the code point, its name, its
// category, its combining class, its bidi class, its decomposition, and more
// that normalization does not ask about. Almost every line has class 0 and
// no decomposition, and is passed over for that without its code point being
// read; the fields are found with find(), which a compiler evaluates as
// memchr.
constexpr unicode_data read_unicode_data() {
  unicode_data out;
  const ucd::file text = unicode_data_file;
  for (std::size_t at = 0; at < text.size();) {
    std::size_t end = text.find('\n', at);
    if (end == ucd::file::npos)
      end = text.size();
    const std::string_view row = text.substr(at, end - at);
    at = end + 1;
    std::array<std::size_t, 6> ends{};
    std::size_t from = 0;
    bool complete = true;
    for (std::size_t& one : ends) {
      one = row.find(';', from);
      if (one == ucd::file::npos) {
        complete = false;
        break;
      }
      from = one + 1;
    }
    if (!complete)
      continue;
    const std::string_view combining = row.substr(ends[2] + 1, ends[3] - ends[2] - 1);
    const std::string_view decomposition =
        row.substr(ends[4] + 1, ends[5] - ends[4] - 1);
    if (combining == "0" && decomposition.empty())
      continue;
    const char32_t code_point = ucd::hex(row.substr(0, ends[0]));
    if (combining != "0") {
      const std::uint8_t value = decimal(combining);
      if (!out.classes.empty() && out.classes.back().last + 1 == code_point &&
          out.classes.back().value == value)
        out.classes.back().last = code_point;
      else
        out.classes.push_back({code_point, code_point, value});
    }
    if (!decomposition.empty()) {
      mapping one{code_point, decomposition.front() == '<',
                  static_cast<std::uint32_t>(out.pool.size()), 0};
      const std::string_view rest =
          one.compatibility ? decomposition.substr(decomposition.find('>') + 1)
                            : decomposition;
      for (std::size_t start = rest.find_first_not_of(' ');
           start != ucd::file::npos;) {
        std::size_t stop = rest.find(' ', start);
        if (stop == ucd::file::npos)
          stop = rest.size();
        out.pool.push_back(ucd::hex(rest.substr(start, stop - start)));
        ++one.length;
        start = rest.find_first_not_of(' ', stop);
      }
      out.mappings.push_back(one);
    }
  }
  return out;
}

// What normalization reads of DerivedNormalizationProps.txt: the quick
// checks, as runs of the bits each sets, and Full_Composition_Exclusion.
struct normalization_props {
  std::vector<std::vector<ucd::run<std::uint8_t>>> checks;
  std::vector<ucd::run<bool>> excluded;
};

constexpr normalization_props read_normalization_props() {
  normalization_props out;
  out.checks.resize(6);
  ucd::each_line(normalization_props_file, [&](const ucd::line& one) {
    const std::string_view name = one.fields[0];
    if (one.count == 1) {
      if (name == "Full_Composition_Exclusion")
        out.excluded.push_back({one.first(), one.last(), true});
      return;
    }
    if (one.count != 2 || name.size() < 6 || !name.ends_with("_QC"))
      return;
    const std::string_view value = one.fields[1];
    std::size_t list = 0;
    std::uint8_t bits = 0;
    if (name == "NFD_QC" && value == "N")
      list = 0, bits = 0x01;
    else if (name == "NFKD_QC" && value == "N")
      list = 1, bits = 0x02;
    else if (name == "NFC_QC" && value == "N")
      list = 2, bits = 0x04;
    else if (name == "NFC_QC" && value == "M")
      list = 3, bits = 0x08;
    else if (name == "NFKC_QC" && value == "N")
      list = 4, bits = 0x10;
    else if (name == "NFKC_QC" && value == "M")
      list = 5, bits = 0x20;
    else
      return;
    out.checks[list].push_back({one.first(), one.last(), bits});
  });
  for (auto& list : out.checks)
    std::ranges::sort(list, {}, &ucd::run<std::uint8_t>::first);
  std::ranges::sort(out.excluded, {}, &ucd::run<bool>::first);
  return out;
}

// One code point's decompositions all the way down: canonical, where it has
// one, and by compatibility, where that is not the same.
struct decomposition {
  char32_t code_point = 0;
  std::uint16_t canonical_offset = 0;
  std::uint8_t canonical_length = 0;
  std::uint16_t compatibility_offset = 0;
  std::uint8_t compatibility_length = 0;
};

// A primary composite: what `first` and `second` compose to.
struct composition {
  char32_t first = 0;
  char32_t second = 0;
  char32_t composite = 0;
};

// Hangul syllables decompose by arithmetic (the Unicode Standard, section
// 3.12), and some mappings by compatibility have them in what they map to.
constexpr void decompose_hangul(char32_t syllable, std::vector<char32_t>& out) {
  const char32_t index = syllable - 0xAC00;
  out.push_back(0x1100 + index / 588);
  out.push_back(0x1161 + index % 588 / 28);
  if (index % 28 != 0)
    out.push_back(0x11A7 + index % 28);
}

struct laid_out {
  ucd::layout<std::uint8_t> classes;
  ucd::layout<std::uint8_t> checks;
  std::vector<decomposition> decompositions;
  std::vector<char32_t> pool;
  std::vector<composition> compositions;
};

constexpr laid_out lay_out() {
  const unicode_data data = read_unicode_data();
  const normalization_props props = read_normalization_props();
  laid_out out;
  out.classes = ucd::lay_out(data.classes);
  out.checks = ucd::lay_out(ucd::merged(props.checks));

  const auto mapping_of = [&](char32_t code_point) -> const mapping* {
    const auto found = std::ranges::lower_bound(data.mappings, code_point, {},
                                                &mapping::code_point);
    return found != data.mappings.end() && found->code_point == code_point
               ? &*found
               : nullptr;
  };
  const auto excluded = [&](char32_t code_point) {
    const auto after = std::ranges::upper_bound(props.excluded, code_point, {},
                                                &ucd::run<bool>::first);
    return after != props.excluded.begin() &&
           code_point <= std::ranges::prev(after)->last;
  };
  // All the way down: each code point a mapping maps to, decomposed again.
  const auto full = [&](const auto& self, char32_t code_point,
                        bool compatibility, std::vector<char32_t>& into) -> void {
    if (code_point >= 0xAC00 && code_point <= 0xD7A3) {
      decompose_hangul(code_point, into);
      return;
    }
    const mapping* one = mapping_of(code_point);
    if (one == nullptr || (one->compatibility && !compatibility)) {
      into.push_back(code_point);
      return;
    }
    for (std::uint32_t at = 0; at < one->length; ++at)
      self(self, data.pool[one->offset + at], compatibility, into);
  };

  for (const mapping& one : data.mappings) {
    decomposition entry{one.code_point};
    std::vector<char32_t> canonical;
    if (!one.compatibility) {
      full(full, one.code_point, false, canonical);
      entry.canonical_offset = static_cast<std::uint16_t>(out.pool.size());
      entry.canonical_length = static_cast<std::uint8_t>(canonical.size());
      out.pool.insert(out.pool.end(), canonical.begin(), canonical.end());
    }
    std::vector<char32_t> compatible;
    full(full, one.code_point, true, compatible);
    if (compatible != canonical) {
      entry.compatibility_offset = static_cast<std::uint16_t>(out.pool.size());
      entry.compatibility_length = static_cast<std::uint8_t>(compatible.size());
      out.pool.insert(out.pool.end(), compatible.begin(), compatible.end());
    }
    out.decompositions.push_back(entry);
    // A primary composite: a canonical mapping of two, not excluded.
    if (!one.compatibility && one.length == 2 && !excluded(one.code_point))
      out.compositions.push_back(
          {data.pool[one.offset], data.pool[one.offset + 1], one.code_point});
  }
  std::ranges::sort(out.compositions, {}, [](const composition& one) {
    return std::pair(one.first, one.second);
  });
  return out;
}

// Laid out twice, since what it is laid out into has to be as long as what
// it is: once to count, once to fill.
struct sizes {
  std::size_t class_blocks = 0;
  std::size_t check_blocks = 0;
  std::size_t decompositions = 0;
  std::size_t pool = 0;
  std::size_t compositions = 0;
};

inline constexpr sizes sized = [] {
  const laid_out laid = lay_out();
  return sizes{laid.classes.blocks.size() / ucd::block_size,
               laid.checks.blocks.size() / ucd::block_size,
               laid.decompositions.size(), laid.pool.size(),
               laid.compositions.size()};
}();
static_assert(sized.pool <= 65536, "an offset into the pool is 16 bits");

struct all_tables {
  ucd::two_stage_table<std::uint8_t, sized.class_blocks> classes;
  ucd::two_stage_table<std::uint8_t, sized.check_blocks> checks;
  std::array<decomposition, sized.decompositions> decompositions;
  std::array<char32_t, sized.pool> pool;
  std::array<composition, sized.compositions> compositions;
};

inline constexpr all_tables table = [] {
  const laid_out laid = lay_out();
  all_tables out{};
  out.classes = ucd::two_stage<std::uint8_t, sized.class_blocks>(laid.classes);
  out.checks = ucd::two_stage<std::uint8_t, sized.check_blocks>(laid.checks);
  std::ranges::copy(laid.decompositions, out.decompositions.begin());
  std::ranges::copy(laid.pool, out.pool.begin());
  std::ranges::copy(laid.compositions, out.compositions.begin());
  return out;
}();

constexpr const decomposition* decomposition_of(char32_t code_point) noexcept {
  const auto found = std::ranges::lower_bound(table.decompositions, code_point,
                                              {}, &decomposition::code_point);
  return found != table.decompositions.end() && found->code_point == code_point
             ? &*found
             : nullptr;
}

constexpr std::u32string_view in_pool(std::uint16_t offset,
                                      std::uint8_t length) noexcept {
  return {table.pool.data() + offset, length};
}

}  // namespace alef::tables::normalization_data

export namespace alef::tables {

constexpr std::uint8_t combining_class_of(char32_t code_point) noexcept {
  return normalization_data::table.classes[code_point];
}

constexpr quick_checks quick_checks_of(char32_t code_point) noexcept {
  return {normalization_data::table.checks[code_point]};
}

// The canonical decomposition of `code_point` all the way down, or nothing
// where it has none. Hangul syllables are not here: they are arithmetic.
constexpr std::u32string_view canonical_decomposition(
    char32_t code_point) noexcept {
  if (quick_checks_of(code_point).nfd() == quick_check::yes)
    return {};
  const auto* entry = normalization_data::decomposition_of(code_point);
  if (entry == nullptr || entry->canonical_length == 0)
    return {};
  return normalization_data::in_pool(entry->canonical_offset,
                                     entry->canonical_length);
}

// And by compatibility, which is the canonical one where that is all there
// is.
constexpr std::u32string_view compatibility_decomposition(
    char32_t code_point) noexcept {
  if (quick_checks_of(code_point).nfkd() == quick_check::yes)
    return {};
  const auto* entry = normalization_data::decomposition_of(code_point);
  if (entry == nullptr)
    return {};
  if (entry->compatibility_length != 0)
    return normalization_data::in_pool(entry->compatibility_offset,
                                       entry->compatibility_length);
  return normalization_data::in_pool(entry->canonical_offset,
                                     entry->canonical_length);
}

// What `first` and `second` compose to, where they are a primary composite's
// canonical mapping; 0 where they are not. Hangul is not here either.
constexpr char32_t primary_composite(char32_t first, char32_t second) noexcept {
  const auto& all = normalization_data::table.compositions;
  const auto found = std::ranges::lower_bound(
      all, std::pair(first, second), {},
      [](const normalization_data::composition& one) {
        return std::pair(one.first, one.second);
      });
  return found != all.end() && found->first == first && found->second == second
             ? found->composite
             : 0;
}

}  // namespace alef::tables
